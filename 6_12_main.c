
#define USE_HAL_DRIVER
#define HAL_DMA_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED

#include "stm32f0xx_hal.h"
#include "stm32f0xx.h"                  // Device header
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


// PORT A
#define FL_C 							0b1									// PA0: front left light 	 	output
#define FR_C 							0b10								// PA1: front right light 	output
#define BL_C 							0b100								// PA2: back left light			output
#define BR_C 							0b1000							// PA3: back right light		output
#define LT_In 						0b10000							// PA4: left turn						input
#define RT_In							0b100000						// PA5: right turn					input
#define BRK_C							0b10000000					// PA7: center brake light 	output
#define BRK_In						0b100000000					// PA8: brake 							analog input
#define MC_Contact_out		0b1000000000				// PA9: MC_Contact output		output
#define Hazard_In					0b10000000000				// PA10: Hazard lights			input
#define Cruise_Out				0b100000000000			// PA11: cruise control 		output
#define Cruise_In					0b1000000000000			// PA12: cruise control 		input
#define	PROG_34						0b10000000000000		// programmer								input
#define	PROG_37						0b100000000000000		// programmer								input
//REMVOED:#define	Trip_In						0b1000000000000000	// trip input								PA15	input

// PORT B
#define Accel_In					0b1000							// Accelerator input				PB3	input
#define Regen_In					0b10000							// Regenerative Breaking		PB4	input
#define Headlights_In			0b100000						// Headlights input					PB5	input
#define Headlights_C			0b1000000						// Headlights output				PB6 input

#define BlinkTR 					750					// Blink toggle rate
#define all_LEDs					0xF

void GPIO_init(void);
//void SysTick_Handler(void);
void Hazards(void);
void turnSignals(void);
void blink (unsigned int front, unsigned int back, unsigned int backBrake);
int is_Cruise(void);
int is_Braking(void);

//This is needed for the HAL_delay
void SysTick_Handler(void)
{
	HAL_IncTick();
}

int main(void)
{
	HAL_Init();
	GPIO_init();					// initialze portA and B
	HAL_Delay(5000);		//change to 5s
	GPIOA->BSRR = MC_Contact_out; 
	while(1)
	{
		int hazards_on = GPIOA->IDR & Hazard_In;
		if (hazards_on == Hazard_In) 
		 Hazards();
		else
			turnSignals();	
	}
		return 0;
}

//initializations of ports
void GPIO_init(void)
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;	// enable I/O port A and B clock
	

	//WRONG: bit 0-4 are outputs, 5-7 digital inputs, 8 input, 9 output
	//Corret: 0-4 out, 5-6 in, 7 out, 8 in(modified due to pcb), 9 out, 
	//				10 in, 11 out, 12 in...
	GPIOA->MODER = 0x0445055 ; //0x00440155;									
	GPIOA->PUPDR = 0;  							
	//GPIOA->BSRR = MC_Contact_out;		//all outputs except MC are low to start
	GPIOA->OTYPER = 0;
	GPIOA->OSPEEDR = 0x15555555;
	
	GPIOB->MODER = 0;								// all pins are inputs																						// 10 input, 11 output, 12 input
	GPIOB->PUPDR = 0;  				
	GPIOB->OTYPER = 0;
	GPIOB->OSPEEDR = 0x15555555;
	// GPIOA->MODER = (0b01 << GPIO_MODER_MODER7_Pos) | 
	
	// GPIOA->BSRR = 0x0000001F;		//all outputs are low to start, keep this line
}




/* Hazards:
	 -Uses the same outgoing signals as the blinkers
	 -Turns on all lights except brake (unless braking)
	 -Handles hazards + braking */
void Hazards(void)
{
	int hazards_on = GPIOA->IDR & Hazard_In;
	
	//if hazards are on ==> turn on all lights
  if (hazards_on == Hazard_In) //0x1000
	{   
		GPIOA->BSRR = all_LEDs	;		// turn on all lights
		
		//checks if brakes are on every 50 ms
    for (int i = 0; i < 10;  i++) 
		{
			//compare returned value from check function
      if (is_Braking() == 1) 										
				GPIOA->BSRR = BRK_C	; 		//set brakes (PA7) high				
		
			
      HAL_Delay(50);
    }
		HAL_Delay(BlinkTR);
		GPIOA->BRR = all_LEDs	;	// turn off all lights (was |= 0xF)
		//but still checks if brakes are on every 50 ms
    for (int i = 0; i<10; i++) 
		{
      if (is_Braking() == 1) 
				GPIOA->BSRR = BRK_C	; 		//set brakes (PA7) high				
			
      HAL_Delay(50);
    }
  }
}

void turnSignals(void) 
{
	int left_on = GPIOA->IDR & LT_In;
	int right_on = GPIOA->IDR & RT_In;
// if braking
  if (is_Braking() == 1) 
	{  
    if (left_on == LT_In) 
		{
			//set brakes and back right high 
			GPIOA->BSRR = BRK_C | BR_C;
			blink(FL_C,BL_C,BR_C);
		}
    else if (right_on == RT_In)	
		{	
			//set brakes and back left high
			GPIOA->BSRR = BRK_C | BL_C	; 								 
			blink(FR_C,BR_C,BL_C);
		}
    //No Turn Signal On
    else 
		{
			//center brake, back left, and back right high
			GPIOA->BSRR = BRK_C | BL_C | BR_C	; 
			
			GPIOA->BRR = FL_C | FR_C ;
			
		}
  }
	// if not braking
	else 
	{    
    if (left_on == LT_In) 			
		{
			//set cbrake and back right low
			GPIOA->BRR = BRK_C | BR_C;								
			blink(FL_C,BL_C,BR_C);
		}
    //Right Turn Signal On
		//take in and compare value for right turn input
    else if ( right_on == RT_In)	
		{
			//set brakes and back left low
			GPIOA->BRR = BRK_C | BL_C	;								
			blink(FR_C,BR_C,BL_C);
		}
    //No Turn Signal On
    else
		{
			//set brakes, back left, and back right low
			GPIOA->BRR = BRK_C | BL_C | BR_C | FL_C | FR_C ;
			
		}
  }
}

void blink (unsigned int front, unsigned int back, unsigned int backBrake) 
{
	//front, back: blinkers of one side
	//backBrake: the opposite back light, to turn on when the brakes are pressed
	GPIOA->BSRR = front | back;
	
	//check brakes
	for (int i = 0; i<10; i++)	
	{
		if (is_Braking() == 1)	// turn on brakes in addition
			GPIOA->BSRR = BRK_C | backBrake	; 
		//else
			//GPIOA->BRR = BRK_C & backBrake;
    HAL_Delay(50);		
   }
	HAL_Delay(BlinkTR);		//~750ms (delay)
	//turn off lights that are being blinked
	GPIOA->BRR =  front | back;	
	 
	 //check brakes again
	for (int i = 0; i<10; i++)	
	{
		if (is_Braking() == 1) 	// turn on brakes in addition
			GPIOA->BSRR = BRK_C | backBrake	; 						
		//else
			//GPIOA->BRR = BRK_C & backBrake;
		HAL_Delay(50);
  }
}

//Check if we are braking and return true if so
int is_Braking(void)
{
	int brakes_on = GPIOA->IDR & BRK_In;
	
	//if brakes are on ==> turn on brake lights
  if (brakes_on == BRK_In) 
	{   
		GPIOA->BSRR = BRK_C	;		// turn on brakes
		return 1;
	}	
	else
	{
		GPIOA->BRR	= BRK_C;	//clear brakes to low
		return 0;
	}
			
	/*
//for cruise	
	if(PORTA->IDR8 > 29 | PORTA->IDR13 == 1){ // Brakes_in or Regen_in
		GPIOA->BRR = ( Cruise_Out );						// turn cruise control off
		return true;
		}
	if(PORTA->IDR15 > 700){ 								// if Accel_in above threshold
		GPIOA->BRR = ( Cruise_Out );						// turn cruise control off
		return false;
	
	}*/
}


/*int is_Cruise(void)
{
	// if cruise control input is true
	if(Cruise_In == ( GPIOA->IDR & Cruise_In ) ) 
	{
		//if cruise control is on ==> turn it off
			if( Cruise_Out == ( GPIOA-> IDR & Cruise_Out ) )
			{			
				GPIOA->BRR = ( Cruise_Out );							
			}
			//else turn it on
			else
			{
				GPIOA->BSRR = ( Cruise_Out );			
			}
			return 1; 		//(not braking)
	}
	else
		return 0;
}
*/

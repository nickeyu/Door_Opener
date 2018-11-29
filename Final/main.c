/*
 * Animal_Door_Opener.c
 *
 * Created: 11/1/2018 2:48:49 PM
 * Author : nicke
 */ 

#include <avr/io.h>
#include <io.c>
#include <bit.h>
#include <scheduler.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <usart_ATmega1284.h>

#define B6 ( PINB & 0x40 )  //motion sensor pin B6

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADSC: setting this bit starts the first conversion.
	// ADATE: setting this bit enables auto-triggering. Since we are
	//        in Free Running Mode, a new conversion will trigger whenever
	//        the previous conversion completes.
}

int j;
int clkwise = 2;
unsigned short light_max = 100;   //based on lab room
char image_ready;
int change = 0;
int turnTime = 0;
int clear = 0;
char send_data = 0;
int mid_change = 0;
int motion_holder = 0;
int task_completed = 0;
int initialize = 0;
unsigned char motorArray[] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09}; //turning stepper motor array
int timeout = 0;
int cat_lcd = 0;
char night = 0;

enum usart_states { init_usart, usart_wait, usart_receive };

	
int TickFct_USART(int state) {
	
	switch(state)
	{
		case init_usart:
			state = usart_wait;
		break;
		
		case usart_wait:
			if ( (send_data == 1 && initialize == 0) || (send_data == 1 && task_completed == 1) ) {
				initialize = 1;
				motion_holder = 1;
				//LCD_DisplayString(1, "SENDING");
				if ( USART_IsSendReady(0)) {
					USART_Send('1' + night, 0);
					if ( USART_HasTransmitted(0)) {
						LCD_DisplayString(1, "SENDED DATA!");
					}
				}
				state = usart_receive;
			}
			else {
			    if ( cat_lcd == 0 ) {
					LCD_DisplayString(1, "Waiting");
					cat_lcd = 1;
				}
				state = usart_wait;
			}
		break;
		
		case usart_receive:
			if ( USART_HasReceived(0) ) 
			{
				image_ready = USART_Receive(0);
				image_ready = image_ready - 48; //decoding ascii from pi
				if ( image_ready == 2 ) {
					
					cat_lcd = 0;
					
					//wait_lcd = 1;
					task_completed = 1;
					motion_holder = 0;
				}
				state = usart_wait;
			}
			else {
				if ( timeout == 3000) {
					LCD_DisplayString(1, "Timeout!");
					state = usart_wait;
				}
				else
				{
					timeout++;
					state = usart_receive;
				}
			}
		break;
		
		default:
			state = init_usart;
		break;
	}
	
	return state;
}

enum motion_check { motion_init, motion_start, motion_on};
	
int TickFct_motion(int state) {
	
	switch(state)
	{
		
		case motion_init:
			state = motion_start;
			break;
			
		case motion_start:
			if ( B6 && motion_holder == 0 ) {
				send_data = 1;
				//clkwise = 1;
				state = motion_on;
			}
			else {
				state = motion_start;
			}
			break;
		
		case motion_on:
			if ( !B6 ) {
				//clkwise = 2;
				send_data = 0;
				state = motion_start;
			}
			else {
				state = motion_on;
			}
		break;
			
		default:
			state = motion_init;
			break;
	}
	
	return state;
}

enum button_check { start, checkBut, };

int TickFct_phase(int state) {
	
	switch(state)
	{
		case start:
			state = checkBut;
		break;
		
		case checkBut:
			if ( (ADC < light_max/2 && change == 1) ) {
				LCD_Cursor(17);
				LCD_WriteData('0' + 30);
				LCD_Cursor(18);
				LCD_WriteData('0' + 25);
				LCD_Cursor(19);
				LCD_WriteData('0' + 23);
				LCD_Cursor(20);
				LCD_WriteData('0' + 24);
				LCD_Cursor(21);
				LCD_WriteData('0' + 36);
				//LCD_DisplayString(17, "NIGHT");
				change = 0;
				night = 0;
				state = checkBut;
			}
			else if ( (ADC > light_max/2 && change == 0)) {
				//LCD_ClearScreen();
				//LCD_DisplayString(17, "DAY");
				LCD_Cursor(17);
				LCD_WriteData('0' + 20);
				LCD_Cursor(18);
				LCD_WriteData('0' + 17);
				LCD_Cursor(19);
				LCD_WriteData('0' + 41);
				LCD_Cursor(20);
				LCD_WriteData(32);
				LCD_Cursor(21);
				LCD_WriteData(32);
				LCD_Cursor(20);
				change = 1;
				night = 1;
				state = checkBut;
			}
			/*else if ( ADC == light_max/2 ) {
				clkwise = 0;
				state = checkBut;
			}*/
			else {
				state = checkBut;
			}
		break;
		
		default:
			state = start;
		break;
	}
	
	
	return state;
}

enum SM2_States { init, wait, clockwise, ctrclockwise };

int waitCounter = 0;
int display = 0;
int TickFct_Rotate(int state) {
	
	switch(state) {   //transitions
		
		case init: // Initial transition
			LCD_DisplayString(1, "Waiting");
		state = wait;
		
		break;
		
		case wait:
			if ( image_ready == 1) {
				task_completed = 0;
				if ( display == 0 ) {
					LCD_DisplayString(1, "Opening Door..");
					display = 1;
				}
				image_ready = 0;
				//clear = 1;
				state = clockwise;
			}
			else if ( mid_change == 1 && waitCounter == 100) {
				clear = 1;
				if ( display == 0 ) {
					LCD_DisplayString(1, "Closing Door..");
					display = 1;
				}
				state = ctrclockwise;
			}
			else {
				if (clear == 1 ){
					LCD_DisplayString(1, "Waiting");
					clear = 0;
				}
				display = 0;
				state = wait;
			}
		break;
		
		case clockwise:
			if ( turnTime != 3000) {
				state = clockwise;
			}
			else {
				mid_change = 1;
				state = wait;
			}
		break;
		
		case ctrclockwise:
			if ( turnTime != 0) {
				state = ctrclockwise;
			}
			else
			{
				cat_lcd = 0;
				motion_holder = 0;
				task_completed = 1;
				mid_change = 0;
				state = wait;
			}
		break;
		
		default:
		state = init;
		break;
	}
	
	switch(state) {   //actions
		
		case init:
		j = 0;
		break;

		case wait:
			if ( mid_change == 1 ) {
				waitCounter++;
			}
		break;
		
		case clockwise:
			if ( j < 0 ) {
				j = 7;
			}
			PORTB = motorArray[j];
			j--;
			turnTime++;
		break;
		
		case ctrclockwise:
			if ( j > 7 ) {
				j = 0;
			}
			PORTB = motorArray[j];
			j++;
			turnTime--;
		break;
		
		default:
		break;
	}

	return state;
}


int main(void)
{
    
	 DDRC = 0xFF; PORTC = 0x00; // LCD data lines
	 DDRD = 0xFF; PORTD = 0x03; // LCD control lines
	 DDRB = 0x0F; PORTB = 0xF0; // stepper motor(output) and motion sensor(input)
	 DDRA = 0x00; PORTA = 0xFF; // photosensor input
	 
	 // Initializes the LCD display
	 LCD_init();
	 ADC_init();
	 initUSART(0);
	 
	 // Starting at position 1 on the LCD screen, writes Hello World
	tasksNum = 4; // declare number of tasks
	task tsks[4]; // initialize the task array
	tasks = tsks; // set the task array
	
	// define tasks
	unsigned char i=0; // task counter
	tasks[i].state = start;
	tasks[i].period = 3;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_phase;
	
	++i; // task counter
	tasks[i].state = init;
	tasks[i].period = 3;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_Rotate;
	
	++i;
	tasks[i].state = motion_init;
	tasks[i].period = 3;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_motion;
	
	++i;
	tasks[i].state = init_usart;
	tasks[i].period = 90;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &TickFct_USART;
	
	
	TimerSet(3); // value set should be GCD of all tasks
	TimerOn();
	
    while (1) 
    {
	/*	if ( ADC < light_max/2) {
			LCD_WriteData('0'+4);
		}
		else {
			LCD_WriteData('0'+9);
		}*/
	}
}


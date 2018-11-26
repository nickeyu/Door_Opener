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
char motion = 1;
char image_ready;
int change = 0;
int turnTime = 0;
int clear = 0;
char send_data = 0;

unsigned char motorArray[] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09}; //turning stepper motor array

/*
enum LCD_Displayer { lcd_init, lcd_start, lcd_change };
	
int TickFct_LCD_Display(int state) {
	
	switch(state)
	{
		case lcd_init:
			state = lcd_start;
		break;
		
		case lcd_start:
			if ( change == 1 ) {
				state = lcd_change;
			}
			else {
				state = lcd_start;
			}
		break;
		
		case lcd_change: 
			
			state = lcd_start;
		break;
		
	}
	
	return state;
}	*/

enum usart_states { init_usart, usart_wait, usart_receive };
	
int TickFct_USART(int state) {
	
	switch(state)
	{
		case init_usart:
			state = usart_wait;
		break;
		
		case usart_wait:
			if (send_data) {
				motion = 1;
				if ( USART_IsSendReady(0)) {
					USART_Send(motion, 0);
				}
				state = usart_receive;
			}
			else {
				state = usart_wait;
			}
		break;
		
		case usart_receive:
			if ( USART_HasReceived(0) ) 
			{
				image_ready = USART_Receive(0);
				LCD_DisplayString(1, "RECEIVED:");
				LCD_WriteData('0'+ image_ready);
				state = usart_wait;
			}
			else {
				state = usart_receive;
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
			if ( B6 ) {
				send_data = 1;
				clkwise = 1;
				state = motion_on;
			}
			else {
				state = motion_start;
			}
			break;
		
		case motion_on:
			if ( !B6 ) {
				clkwise = 2;
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

int TickFct_Rotate(int state) {
	
	switch(state) {   //transitions
		
		case init: // Initial transition
			LCD_DisplayString(1, "Waiting");
		state = wait;
		
		break;
		
		case wait:
			if ( clkwise == 1 ) {
				LCD_DisplayString(1, "Turning Clkwise");
				clear = 1;
				state = clockwise;
			}
			else if ( clkwise == 0 ) {
				clear = 1;
				LCD_DisplayString(1, "Turning CtrClkwise");
				state = ctrclockwise;
			}
			else {
				if (clear == 1 ){
					LCD_DisplayString(1, "Waiting");
					clear = 0;
				}
				state = wait;
			}
		break;
		
		case clockwise:
			if ( clkwise == 1) {
				state = clockwise;
			}
			else if ( clkwise == 0 ) {
				state = ctrclockwise;
			}
			else {
				state = wait;
			}
		break;
		
		case ctrclockwise:
			if ( clkwise == 0) {
				state = ctrclockwise;
			}
			else if ( clkwise == 1 ) {
				state = clockwise;
			}
			else
			{
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
		
		break;
		
		case clockwise:
			if ( j < 0 ) {
				j = 7;
			}
			PORTB = motorArray[j];
			j--;
		//	turnTime++;
		break;
		
		case ctrclockwise:
			if ( j > 7 ) {
				j = 0;
			}
			PORTB = motorArray[j];
			j++;
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


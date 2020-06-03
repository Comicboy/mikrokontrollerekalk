/*
 * test.c
 *
 * Created: 2020.05.26. 10:23:10
 * Author : peisz
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char obst_front = 0;
volatile unsigned char obst_back = 0;
volatile unsigned char obst_left = 0;
volatile unsigned char obst_right = 0;
volatile double distance;
volatile uint16_t time1;
volatile uint16_t time2;
double finalTime;
volatile uint16_t count = 0;
volatile uint16_t front_count = 0;
volatile uint16_t back_count = 0;
volatile uint16_t right_count = 0;
volatile uint16_t left_count = 0;
volatile unsigned char measured = 0;
volatile unsigned char edge = 1;
volatile unsigned char sensor_num;


int main(void)
{
	DDRD = DDRD | (1 <<PD2);
	DDRD = DDRD | (1 <<PD3);
	DDRD = DDRD | (1 <<PD4);
	DDRD = DDRD | (1 <<PD5); //Trig
	DDRB = DDRB & ~(1 << PB0); //Echo pin
	DDRB = DDRB | (1 << PB5);	//LED
	
	PORTB = PORTB & ~(1 <<PB5); //LED clear
	
	sensor_num = 2;
	
	/*TIMER1 Init*/
	TCCR1B = TCCR1B | (1 << ICES1); //Input capture IT edge select (rising edge)
	TIMSK1 = TIMSK1 | (1 << ICIE1); //Input capture IT enable
	sei(); //Global IT enable
	TCCR1B = TCCR1B | (1 << CS12); //256 prescale
	
	/*TIMER2 Init*/
	TCCR2A = TCCR2A | (1 << WGM21); //CTC mode
	OCR2A = 39; //20 us
	TIMSK2 = TIMSK2 | (1 << OCIE2A); // CTC IT enable
	TCCR2B = TCCR2B | (1 << CS21); // 8 prescale
	
	
    while(1)
    {  
		if(measured == 1) //We received the whole echo pulse edge
		{
			measured = 0;
			
			if(time2 > time1) //If TIMER1 didn't overflow
			{
				finalTime = (time2 - time1)*0.000016; //256 prescale, 1 count = 1.6^10-5
			}
			else
			{
				finalTime = (65535 + time2 - time1)*0.000016;
			}
			
			distance = (finalTime*340)/2;
			
			if(distance <= 0.1)
			{
				switch(sensor_num)
				{
					case 2:
					obst_back = 1;
					break;
					
					case 3:
					obst_front = 1;
					break;
					
					case 4:
					obst_left = 1;
					break;
					
					case 5:
					obst_right = 1;
					break;
				}
			}
			else
			{
				switch(sensor_num)
				{
					case 2:
					obst_back = 0;
					break;
					
					case 3:
					obst_front = 0;
					break;
					
					case 4:
					obst_left = 0;
					break;
					
					case 5:
					obst_right = 0;
					break;
				}
			}
			
			if(obst_right == 1)
			{
				PORTB = PORTB | (1 << PB5);
			}
			else
			{
				PORTB = PORTB & ~(1 << PB5);
			}
			
			if(sensor_num < 5)
			{
				sensor_num = sensor_num + 1;
			}
			else
			{
				sensor_num = 2;
			}
			edge = 1;
		}
    }
}

ISR(TIMER2_COMPA_vect) //Every 20 us this ISR fires
{
	count = count + 1;
		
	switch(sensor_num) //Depending on the currently ranging side we set the trigger pin when the respective counter is 1 and clear it when it is 2
	{
		case 2:
		back_count = back_count + 1;
		if(back_count == 1)
		{
			PORTD = PORTD | (1 << PD2); //Trig set
		}
		
		if(back_count == 2)
		{
			PORTD = PORTD & ~(1 << PD2); //Trig clear
		}
		break;
		
		case 3:
		front_count = front_count + 1;
		if(front_count == 1)
		{
			PORTD = PORTD | (1 << PD3);
		}
		
		if(front_count == 2)
		{
			PORTD = PORTD & ~(1 << PD3);
		}
		break;
		
		case 4:
		left_count = left_count + 1;
		if(left_count == 1)
		{
			PORTD = PORTD | (1 << PD4);
		}
		
		if(left_count == 2)
		{
			PORTD = PORTD & ~(1 << PD4);
		}
		break;
		
		case 5:
		right_count = right_count + 1;
		if(right_count == 1)
		{
			PORTD = PORTD | (1 << PD5);
		}
		
		if(right_count == 2)
		{
			PORTD = PORTD & ~(1 << PD5);
		}
		break;
	}
	
	if(count == 1000) //After 700*20us = 14 ms we start again the ranging
	{
		count = 0;
		front_count = 0;
		back_count = 0;
		left_count = 0;
		right_count = 0;
	}
}

ISR(TIMER1_CAPT_vect) //THis ISR fires whenever there is a pin change on the echo pin
{
	switch(edge) //Rising if edge = 1, falling if edge = 0
	{
		case 1:
		time1 = ICR1L; //First read in the low 8 bits of ICR1
		time1 = time1 | (ICR1H << 8); //Read in the high 8 bits of ICR1
		edge = 0;
		TCCR1B = TCCR1B & ~(1 << ICES1); //Input capture falling edge
		break;
		
		case 0:
		time2 = ICR1L;
		time2 = time2 | (ICR1H << 8);
		TCCR1B = TCCR1B | (1 << ICES1); //Input capture rising edge
		measured = 1;
		break;
	}
}

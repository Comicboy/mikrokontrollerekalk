/*
 * test.c
 *
 * Created: 2020.05.26. 10:23:10
 * Author : peisz
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char obst = 0;
volatile double distance;
volatile uint16_t time1;
volatile uint16_t time2;
double finalTime;
volatile unsigned char count = 0;
volatile unsigned char measured = 0;
volatile unsigned char edge = 1;


int main(void)
{
	DDRD = DDRD | (1 <<PD5); //Trig
	DDRB = DDRB & ~(1 << PB0); //Echo pin
	DDRB = DDRB | (1 << PB5);	//LED
	
	PORTB = PORTB & ~(1 <<PB5); //LED clear
	
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
				PORTB = PORTB | (1 << PB5);
			}
			else
			{
				PORTB = PORTB & ~(1 << PB5);
			}
		}
    }
}

ISR(TIMER2_COMPA_vect) //Every 20 us this ISR fires
{
	count = count + 1;
	
	if(count == 1)
	{
		PORTD = PORTD | (1 << PD5); //Trig set
	}
	
	if(count == 2)
	{
		PORTD = PORTD & ~(1 << PD5); //Trig clear
	}
	
	if(count == 700)
	{
		count = 0;
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
		edge = 1;
		TCCR1B = TCCR1B | (1 << ICES1); //Input capture rising edge
		measured = 1;
		break;
	}
}

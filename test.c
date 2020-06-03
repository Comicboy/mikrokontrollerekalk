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
volatile unsigned char flag;
volatile unsigned char trig;
volatile unsigned char i = 0;
volatile double distance;
volatile unsigned char captured = 1;
volatile unsigned char time1;
volatile unsigned char time2;
volatile unsigned char finalTime;
volatile unsigned int timeee;


void uhMeasure(void)
{
	/*PCINT0 IT enable*/
	PCICR = PCICR | (1 << PCIE0);
	PCMSK0 = PCMSK0 | (1 << PCINT0);
	
	/*TIMER1 init for CTC*/
	TCCR1B = TCCR1B | (1 << WGM12); //CTC mode
	TIMSK1 = TIMSK1 | (1 << OCIE1A); //Output compare IT enable
	sei();
	OCR1A = 239; //15 us
	
	/*TIMER1 init for input capture*/
	TIMSK1 = TIMSK1 | (1 << ICIE1); //Input capture IT enable
	TCCR1B = TCCR1B & ~(1 << ICES1); //Input capture edge select (rising edge) with noise cancelation
	TCCR1B = TCCR1B | (1 << ICNC1);
	
	PORTD = PORTD | (1 << PD5); //Set trigger
	TCCR1B = TCCR1B | (1 << CS10); //Start TIMER1 without prescale
	
	while(trig != 1); //Wait until the measurement is completed
	trig = 1;
}

int main(void)
{
	DDRB = DDRB | (1 << PB5);
	PORTB = PORTB & ~(1 << PB5);
	
	DDRD = DDRD | (1 << PD5); //jobb szenzor
	DDRB = 0;
	DDRB = DDRB & ~(1 << PB0); // echo pin
	PORTB = PORTB | (1 << PB0);
	
	TIMSK2 = TIMSK2 | (1 << TOIE2); //TIMER2 OVF IT EN
	sei();
	TCCR2B = TCCR2B | (1 << CS22) | (1 << CS21) | (1 << CS20); //1024 prescale go*/
	trig = 0;
	
	
    while(1)
    {  
		if(flag == 1)
		{
			uhMeasure();
		}
		if(obst == 1)
		{
			PORTB = PORTB ^ (1 << PB5);
		}
    }
}

ISR(TIMER2_OVF_vect)
{
	if(i == 5)
	{
		flag = 1; //Ha hatodszorra ér ide, akkor lehet mérni (6*256*6.4*10^-5 = 98.304 ms)
		i = 0;
	}
	else
	{
		i = i + 1;
	}
}

ISR(TIMER1_COMPA_vect) //When TIMER1 reaches 15 us (239 count) this ISR fires
{
	TCCR1B = TCCR1B & ~(1 << CS10); //Stop TIMER1
	TCCR1B = TCCR1B & ~(1 << WGM12); //Normal mode
	TIMSK1 = TIMSK1 & ~(1 << OCIE1A); //CTC IT disable
	TCNT1 = 0; //Reset TIMER1
	
	PORTD = PORTD & ~(1 << PD5); //Clear trigger
}

ISR(PCINT0_vect) //When we receive the rising edge of the echo signal this ISR fires
{
	TCCR1B = TCCR1B | (1 << CS11) | (1 << CS10); //Start TIMER1 with 64 prescale
	PCICR = PCICR & ~(1 << PCIE0);
	PCMSK0 = PCMSK0 & ~(1 << PCINT0);
}

ISR(TIMER1_CAPT_vect) //When we receive the rising edge of the echo signal this ISR fires
{
	/*if(captured == 1)
	{
		time1 = ICR1; //Save the rising edge of the signal to time1 variable
		TCCR1B = TCCR1B & ~(1 << ICES1); //Input capture edge select (falling edge)
		captured = captured + 1;
	}
	else
	{
		time2 = ICR1; //Save the fallling edge of the signal to time2 variable
		captured = 1;
	}*/
	
	/*Stop and reset TIMER1*/
	TCCR1B = TCCR1B & ~(1 << CS11);
	TCCR1B = TCCR1B & ~(1 << CS10); //Stop TIMER1
	TIMSK1 = TIMSK1 & ~(1 << ICIE1); //Input capture IT disable
	TCNT1 = 0; //Reset TIMER1
	
	/*Compute distance*/
	//finalTime = time2 - time1;
	timeee = ICR1;
	distance = ((timeee*0.000004*340)/2); //distance = (pulsewidth*c)/2 where c = 340 m/s and pulsewidth is measured in seconds
	
	if(distance <= 0.1) //if the measured distance is greater than 0.1 m there is an obstacle in front of the sensor
	{
		obst = 1;
	}
	trig = 1; //Set the trigger flag to signal the measurement is over
}


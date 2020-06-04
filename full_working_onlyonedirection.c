/*
 * Autvehicle.c
 *
 * Created: 2020.05.22. 15:47:11
 * Author : Peisz Balázs
 */ 

#define F_CPU 16000000UL
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE  (((( F_CPU / 16) + (USART_BAUDRATE / 2)) / (USART_BAUDRATE)) - 1)
#define BIT_IS_SET(byte, bit) (byte & (1 << bit))
#define BIT_IS_CLEAR(byte, bit) (~(byte & (1 << bit)))

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

/*Az állapotváltozót megvalósító enum deklarációja*/
typedef volatile enum state {forward, still, halt}state;
state S = halt; //Állapotváltozó inicializációja

volatile unsigned char pressed = 0; //Nyomógomb segédváltozója

/*Akadály jelző változók (0 = nincs akadály; 1 = akadály van)*/
volatile unsigned obst_front = 0;
volatile unsigned obst_back = 0;
volatile unsigned obst_left = 0;
volatile unsigned obst_right = 0;

/*UH távolságméréshez használt változók*/
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
unsigned char memory[5];
unsigned char mem_turn[5];
unsigned char stored = 0;
unsigned char cntr = 0;

/*UART kommunikációhoz használt változók*/
unsigned char data[20]; //Az eltárolt adatok tömbje (20 állapotváltozás tárolására)
unsigned char transmission[20]; //Az EEPROM-ból kiolvasott adatok tömbje
uint8_t n = 0; //A tömb elemeit számláló segédváltozó
volatile unsigned char transmit = 0; //Jelzi, hogy megkezdődhet-e az adás a PC felé
volatile unsigned char received; //A beérkezett adatot tároló változó
char text[26] = "The route of the car was: ";




/*A motorvezérlő függvényei*/
void goForward(void) //A motort előremenetbe vezéreljük (IN1 = 1; IN2 = 0; EN = 1)
{
	PORTC = PORTC | (1 << PC3);
	PORTC = PORTC | (1 << PC4);
	PORTC = PORTC & ~(1 << PC5);
}

void goBackward(void) //A motort hátramenetbe vezéreljük (IN1 = 0; IN2 = 1; EN = 1)
{
	PORTC = PORTC | (1 << PC3);
	PORTC = PORTC & ~(1 << PC4);
	PORTC = PORTC | (1 << PC5);
}

void stop(void) //Leállítjuk a motort (in11 = 1; in 12 = 1)
{
	PORTC = PORTC & ~(1 << PC3);
}

/*A szervo motor függvényei*/
void turnLeft(void)
{
	/*TIMER0 inicializálása, PWM balra forduláshoz*/
	TCCR0A = TCCR0A | (1 << COM0A0); //Compare Output Mode, Phase Correct PWM (Set felfelé, clear lefelé számolásnál)
	TCCR0A = TCCR0A | (1 << COM0A1);
	TCCR0A = TCCR0A | (1 << WGM00); //Számlálás esetén TOP = 0xFF = 255
	OCR0A = 246; //1 ms-os impulzus generálása (igazából 246,1875 lenne)
	TCCR0B = TCCR0B | (1 << CS00);
	TCCR0B = TCCR0B | (1 << CS02); //1024-es prescale
	
}

void turnRight(void)
{
	/*TIMER0 inicializálása, PWM jobbra forduláshoz*/
	TCCR0A = TCCR0A | (1 << COM0A0); //Compare Output Mode, Phase Correct PWM (Set felfelé, clear lefelé számolásnál)
	TCCR0A = TCCR0A | (1 << COM0A1);
	TCCR0A = TCCR0A | (1 << WGM00); //Számlálás esetén TOP = 0xFF = 255
	OCR0A = 242; //2 ms-os impulzushoz igazából 238,375 számlálás kéne (UP és DOWN counting esetében is ez a komparálási érték), de a kísérlet szerint az autónak ez a legjobb
	TCCR0B = TCCR0B | (1 << CS00);
	TCCR0B = TCCR0B | (1 << CS02); //1024-es prescale
}

void stopTurning(void)
{
	/*Egyenesbe fordítjuk a kormányt*/
	OCR0A = 244; //1,5 ms-os impulzushoz igazából 242,28125 számlálás kéne (UP és DOWN counting esetében is ez a komparálási érték), de a kísérlet szerint az autónak ez a legjobb
}

ISR(TIMER2_COMPA_vect) //Az IT minden 20 us-bna lefut
{
	count = count + 1;
	
	switch(sensor_num) //Az éppen mérendő oldaltól függően állítjuk be a trigger pin értékét amikor a megfelelő számláló 1 és töröljük amikor 2
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
	
	if(count == 700) //1000*20us = 20 ms után újrakezdjük a mérést
	{
		count = 0;
		front_count = 0;
		back_count = 0;
		left_count = 0;
		right_count = 0;
	}
}

ISR(TIMER1_CAPT_vect) //Amikor logikai szint változás történik az echo pin-en lefut ez az IT
{
	switch(edge) //Felfutó ha edge = 1, lefutó ha edge = 0
	{
		case 1:
		time1 = ICR1L; //Először beolvassuk az alsó 8 bitjét az ICR1-nek, majd a felső 8-at
		time1 = time1 | (ICR1H << 8);
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

ISR(PCINT0_vect) //Ha megnyomjuk a nyomógombot
{
	pressed = pressed + 1;
	
	if(pressed == 1) //A gomb megnyomása egy 1-0, majd 0-1 váltás egymásutánja amelyet a pressed változóval tartunk nyilván
	{
		S = still;
	}
	if(pressed == 3)
	{
		S = halt;
	}
	if(pressed == 4)
	{
		pressed = 0;
	}
}

ISR(USART_RX_vect) //Ha bejövő soros kommunikáció van, akkor a tartalmától függően indítja meg az adást a mikrokontroller
{
	received = UDR0;
	if(received == '1')
	{
		transmit = 1;
	}
}

int main(void)
{   
	/*Motorvezérlő jeleit kezelő pinek outputra(1) állítása (PC3, PC4 és PC5-ös pinek)*/
	DDRC = DDRC | (1 << PC3); //Enable
	DDRC = DDRC | (1 << PC4); //IN1
	DDRC = DDRC | (1 << PC5); //IN2
	
	/*Szervo motor vezérlő pin beállítása*/
	DDRD = DDRD | (1 << PD6);
	
	/*UH távolságmérő jeleit kezelő pinek beállítása (PD2-5 Trig, PB0 Echo)*/
	DDRD = DDRD | (1 << PD2);
	DDRD = DDRD | (1 << PD3);
	DDRD = DDRD | (1 << PD4);
	DDRD = DDRD | (1 << PD5); //Trig
	DDRB = DDRB & ~(1 << PB0); //Echo pin
	
	DDRB = DDRB | (1 << PB5);	//LED
	PORTB = PORTB & ~(1 << PB5); //LED clear
	
	DDRB = DDRB & ~(1 << PB7);
	PORTB = PORTB | (1 << PB7);
	
	sensor_num = 2;
	
	/*USART Init*/
	UCSR0B = UCSR0B | (1 << RXEN0) | (1 << TXEN0); //receive and transmit on
	UCSR0C = UCSR0C | (1 << UCSZ00) | (1 << UCSZ01); //8 bit
	UBRR0H = UBRR0H | (BAUD_PRESCALE >> 8); //Baud rate beállítása
	UBRR0L = BAUD_PRESCALE;
	UCSR0B = UCSR0B | (1 << RXCIE0); //receive and IT en
	
	/*Pin change IT Init*/
	PCICR = PCICR | (1 << PCIE0);
	PCMSK0 = PCMSK0 | (1 << PCINT7); //Pin change interrupt a nyomógombra
	
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
	
    while (1) 
    {	
		/*A USART kommunikációt végző programrész*/
		if(transmit == 1)
		{
			for(int i = 0; i < 26; i = i + 1)
			{
				UDR0 = text[i];
				_delay_ms(250);
			}
			for(int j = 0; j < 10; j = j + 1)
			{
				UDR0 = transmission[j];
				UDR0 = ' ';
				_delay_ms(250);
			}
			transmit = 0;
		}
		
		/*Az UH szenzorok bejövő jelét kezelő programrész*/
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
			
			if(distance <= 0.3)
			{
				switch(sensor_num)
				{
					case 2:
					obst_back = 1;
					break;
					
					case 3:
					obst_front = 1;
					memory[stored] = 1;
					if(stored < 4)
					{
						stored = stored + 1;
					}
					else
					{
						stored = 0;
					}
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
					memory[stored] = 0;
					if(stored < 4)
					{
						stored = stored + 1;
					}
					else
					{
						stored = 0;
					}
					break;
					
					case 4:
					obst_left = 0;
					break;
					
					case 5:
					obst_right = 0;
					break;
				}
			}
			
			if(obst_front == 1)
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
		
		/*Az autó mozgását irányító állapotgép*/
		switch(S)
		{
			case halt:
				break;
			
			case still:
				
				if(obst_front == 1) //Ha az autó áll, de előtte akadály van akkor...
				{
					stop();
				}
				else //Ha szabad az út előre, menj előre
				{
					goForward();
				
					if(n < 20)
					{
						data[n] = 'F';
						n = n + 1;
					}
					S = forward;
				}
				break;
			
			case forward:
			
				if(obst_front == 1) //Ha az autó előtt akadály van akkor...
				{
					stop();
				}
				else
				{
					goForward();
				}
				break;
			
			
		}
    }
}


#define F_CPU 8000000

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


static volatile int pulse = 0;
static volatile int i = 0;
static volatile int litre = 0;
static volatile int hiflag = 0;
static volatile int lowflag=0;
static volatile int flag = 0;



#define BAUD_PRESCALE (((F_CPU / (BAUDRATE * 16UL))) - 1)	/* Define prescale value */

void USART_Init(unsigned long);
char USART_RxChar();
void USART_TxChar(char);

void USART_Init(unsigned long BAUDRATE)
{
	UCSRB |= (1 << RXEN) | (1 << TXEN);				/* Enable USART transmitter and receiver */
	UCSRC |= (1 << URSEL)| (1 << UCSZ0) | (1 << UCSZ1);	/* Write USCRC for 8 bit data and 1 stop bit */
	UBRRL = BAUD_PRESCALE;							/* Load UBRRL with lower 8 bit of prescale value */
	UBRRH = (BAUD_PRESCALE >> 8);					/* Load UBRRH with upper 8 bit of prescale value */
}

char USART_RxChar()									/* Data receiving function */
{
	while (!(UCSRA & (1 << RXC)));					/* Wait until new data receive */
	return(UDR);									/* Get and return received data */
}

void USART_TxChar(char data)						/* Data transmitting function */
{
	UDR = data;										/* Write data to be transmitting in UDR */
	while (!(UCSRA & (1<<UDRE)));					/* Wait until data transmit and buffer get empty */
}

void USART_SendString(char *str)					/* Send string of USART data function */
{
	int i=0;
	while (str[i]!=0)
	{
		USART_TxChar(str[i]);						/* Send each char of string till the NULL */
		i++;
	}
}


ISR(INT0_vect)
{
	if(i==1)
	{
		TCCR1B = 0;
		pulse = TCNT1;
		TCNT1 = 0;
		i = 0;
	}
	
	if(i==0)
	{
		TCCR1B |= 1<<CS10;
		i = 1;
	}
}

int main(void)
{
	USART_Init(9600);
	int16_t lvl = 0;
	
	DDRA=0xFF; //MOTOR (OUTPUT)
	DDRB=0xFF; //TRIGGER (MCU TO SENSOR)
	DDRC=0XFF; //OUTPUT (LED)
	
	GICR |= 1<<INT0;	//INTERRUPT_0 (INPUT FROM SENSOR ECHO)
	MCUCR |= 1<<ISC00;	//ANY LOGICAL CHANGE IN INT0
	
	sei();

		PORTA = 0b00000000;

	while(1)
	{
		
		PORTB |= 1<<PINB0;	//trigger ON
		_delay_us(50);
		PORTB &= ~(1<<PINB0);	//TURN OFF TRIGGER
		
		lvl = pulse/464; //DISTANCE IN CM
		
		
		//////// LED : PORT C ////////////
		if(lvl>=0 && lvl<6)					//HIGH LEVEL -> motor OFF
		{
			PORTA |= 0b00100000;	// C0 = tank full
			PORTA &= 0b00111111;
			flag = 1; //for litre count
			hiflag = 1;	//for pump
			lowflag = 0;
		}
		else if(lvl>=6 && lvl<23)	//NORMAL LEVEL
		{
			PORTA |= 0b01000000;	// C1 = normal
			PORTA &= 0b01011111;
		}
		else if(lvl>=23)	//LOW LEVEL -> motor ON
		{
			PORTA |= 0b10000000;	// C2 = tank empty
			PORTA &= 0b10011111;
			hiflag = 0;
			lowflag = 1;
		}		
	/*	else if(lvl>=15)
		{
			PORTC = 0b00001000;
		} */
	
	/////   MOTOR : (PORT A)   //////
	
		if(lvl<6)					//HIGH LEVEL -> MOTOR OFF
		{
			PORTA &= 0b11111100;	//pump off
			//USART_SendString("TANK FULL! \nTurning off pump.\n");
		}
		
			
		else if(lvl>=6 && lvl<=23)	//NORMAL LEVEL -> MOTOR ON
			{
				if(hiflag==1 && lowflag==0) 
					PORTA &= 0b11111100;	//pump off (clockwise)
				else if(lowflag==1 && hiflag==0)
					PORTA |= 0b00000001;
			}
		
		else if(lvl>23)	//LOW LEVEL -> MOTOR ON
		{
			PORTA |= 0b00000001;	//pump on (clockwise)
			//USART_SendString("TANK NEARLY EMPTY! \nTurning on pump.\n");
		}	
		
		/////   BLUETOOTH : D0,D1 (Rxd Txd)   //////
		
		if(lvl>=23 && flag==1)				//LOW LEVEL
		{
			litre += 2;
			char msg[100];
			strcpy(msg, "\nYou have consumed ");
			
			char litre_string[32];
			sprintf(litre_string, "%d", litre);
			
			strcat(msg, litre_string);
			strcat(msg, " Litres of water today.\n");		//char *strcat(char *dest, const char *src);
			
			USART_SendString(msg);
			flag = 0;
		}
		
		_delay_ms(400);

	}
}



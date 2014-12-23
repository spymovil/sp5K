/*! \file rprintf.c \brief printf routine and associated routines. */
//*****************************************************************************
//
// File Name	: 'rprintf.c'
// Title		: printf routine and associated routines
// Author		: Pascal Stang - Copyright (C) 2000-2002
// Created		: 2000.12.26
// Revised		: 2003.5.1
// Version		: 1.0
// Target MCU	: Atmel AVR series and other targets
// Editor Tabs	: 4
//
// NOTE: This code is currently below version 1.0, and therefore is considered
// to be lacking in some functionality or documentation, or may not be fully
// tested.  Nonetheless, you can expect most functions to work.
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
// SPYMOVIL
// Modificado para considerar las 2 UARTS del Atmega1284P
// No se necesita la funcion rprintfInit ya que la funcion base rprintfChar usa
// las 2 uarts en forma embebida.
//
//*****************************************************************************


#include "sp5KFRTOS.h"

#ifndef TRUE
	#define TRUE	-1
	#define FALSE	0
#endif

#define INF     32766	// maximum field size to print
#define READMEMBYTE(a,char_ptr)	((a)?(pgm_read_byte(char_ptr)):(*char_ptr))

// use this to store hex conversion in RAM
//static char HexChars[] = "0123456789ABCDEF";
// use this to store hex conversion in program memory
//static prog_char HexChars[] = "0123456789ABCDEF";
static char __attribute__ ((progmem)) HexChars[] = "0123456789ABCDEF";

#define hexchar(x)	pgm_read_byte( HexChars+((x)&0x0f) )
//#define hexchar(x)	((((x)&0x0F)>9)?((x)+'A'-10):((x)+'0'))

// function pointer to single character output routine
static void (*rputchar)(unsigned char c);

/*------------------------------------------------------------------------------------*/

// function pointer to debug Allow routine
static s08 (*rprintfDebugAllowed)(u08 dModo);

/*------------------------------------------------------------------------------------*/

// *** rprintf initialization ***
// you must call this function once and supply the character output
// routine before using other functions in this library
void rprintfInit(void (*putchar_func)(unsigned char c))
{
	rputchar = putchar_func;
}

// *** rprintfChar ***
// send a character/byte to the current output device
void rprintfChar(u08 nUart, unsigned char c)
{
	// do LF -> CR/LF translation
	if(c == '\n')
		//rputchar('\r');
		xUartPutChar(nUart, '\r', ( portTickType ) 10);
	// send character
	//rputchar(c);
	xUartPutChar(nUart, c, ( portTickType ) 10);
	return;
}

// *** rprintfStr ***
// prints a null-terminated string stored in RAM
void rprintfStr(u08 nUart, char str[])
{
	// send a string stored in RAM
	// check to make sure we have a good pointer
	if (!str) return;

	// print the string until a null-terminator
	while (*str)
		rprintfChar( nUart, *str++);
}

// *** rprintfProgStr ***
// prints a null-terminated string stored in program ROM
void rprintfProgStr(u08 nUart, const prog_char str[])
{
	// print a string stored in program memory
	register char c;

	// check to make sure we have a good pointer
	if (!str) return;
	
	// print the string until the null-terminator
	while((c = pgm_read_byte(str++)))
		rprintfChar(nUart, c);
}

// *** rprintfCRLF ***
// prints carriage return and line feed
void rprintfCRLF(u08 nUart)
{
	// print CR/LF
	//rprintfChar('\r');
	// LF -> CR/LF translation built-in to rprintfChar()
	rprintfChar(nUart, '\n');
}

// *** rprintfu04 ***
// prints an unsigned 4-bit number in hex (1 digit)
void rprintfu04(u08 nUart, unsigned char data)
{
	// print 4-bit hex value
//	char Character = data&0x0f;
//	if (Character>9)
//		Character+='A'-10;
//	else
//		Character+='0';
	rprintfChar(nUart, hexchar(data));
}


// *** rprintfNum ***
// special printf for numbers only
// see formatting information below
//	Print the number "n" in the given "base"
//	using exactly "numDigits"
//	print +/- if signed flag "isSigned" is TRUE
//	use the character specified in "padchar" to pad extra characters
//
//	Examples:
//	uartPrintfNum(10, 6,  TRUE, ' ',   1234);  -->  " +1234"
//	uartPrintfNum(10, 6, FALSE, '0',   1234);  -->  "001234"
//	uartPrintfNum(16, 6, FALSE, '.', 0x5AA5);  -->  "..5AA5"
void rprintfNum(u08 nUart, char base, char numDigits, char isSigned, char padchar, long n)
{
	// define a global HexChars or use line below
	//static char HexChars[16] = "0123456789ABCDEF";
	char *p, buf[32];
	unsigned long x;
	unsigned char count;

	// prepare negative number
	if( isSigned && (n < 0) )
	{
		x = -n;
	}
	else
	{
	 	x = n;
	}

	// setup little string buffer
	count = (numDigits-1)-(isSigned?1:0);
  	p = buf + sizeof (buf);
  	*--p = '\0';
	
	// force calculation of first digit
	// (to prevent zero from not printing at all!!!)
	*--p = hexchar(x%base); x /= base;
	// calculate remaining digits
	while(count--)
	{
		if(x != 0)
		{
			// calculate next digit
			*--p = hexchar(x%base); x /= base;
		}
		else
		{
			// no more digits left, pad out to desired length
			*--p = padchar;
		}
	}

	// apply signed notation if requested
	if( isSigned )
	{
		if(n < 0)
		{
   			*--p = '-';
		}
		else if(n > 0)
		{
	   		*--p = '+';
		}
		else
		{
	   		*--p = ' ';
		}
	}

	// print the string right-justified
	count = numDigits;
	while(count--)
	{
		rprintfChar(nUart, *p++);
	}
}

#ifdef RPRINTF_FLOAT
// *** rprintfFloat ***
// floating-point print
void rprintfFloat(u08 nUart, char numDigits, double x)
{
	unsigned char firstplace = FALSE;
	unsigned char negative;
	unsigned char i, digit;
	double place = 1.0;

	// save sign
	negative = (x<0);
	// convert to absolute value
	x = (x>0)?(x):(-x);

	// find starting digit place
	for(i=0; i<15; i++)
	{
		if((x/place) < 10.0)
			break;
		else
			place *= 10.0;
	}
	// print polarity character
	if(negative)
		rprintfChar(nUart, '-');
	else
		rprintfChar(nUart, '+');

	// print digits
	for(i=0; i<numDigits; i++)
	{
		digit = (x/place);

		if(digit | firstplace | (place == 1.0))
		{
			firstplace = TRUE;
			rprintfChar(nUart, digit+0x30);
		}
		else
			rprintfChar(nUart, ' ');

		if(place == 1.0)
		{
			rprintfChar(nUart, '.');
		}

		x -= (digit*place);
		place /= 10.0;
	}
}
#endif

#ifdef RPRINTF_SIMPLE
// *** rprintf1RamRom ***
// called by rprintf() - does a simple printf (supports %d, %x, %c)
// Supports:
// %d - decimal
// %x - hex
// %c - character
int rprintf1RamRom(u08 nUart, unsigned char stringInRom, const char *format, ...)
{
	// simple printf routine
	// define a global HexChars or use line below
	//static char HexChars[16] = "0123456789ABCDEF";
	char format_flag;
	unsigned int u_val, div_val, base;
	va_list ap;

	va_start(ap, format);
	for (;;)
	{
		while ((format_flag = READMEMBYTE(stringInRom,format++) ) != '%')
		{	// Until '%' or '\0'
			if (!format_flag)
			{
				va_end(ap);
				return(0);
			}
			rprintfChar(nUart, format_flag);
		}

		switch (format_flag = READMEMBYTE(stringInRom,format++) )
		{
			case 'c': format_flag = va_arg(ap,int);
			default:  rprintfChar(nUart, format_flag); continue;
			case 'd': base = 10; div_val = 10000; goto CONVERSION_LOOP;
//			case 'x': base = 16; div_val = 0x10;
			case 'x': base = 16; div_val = 0x1000;

			CONVERSION_LOOP:
			u_val = va_arg(ap,int);
			if (format_flag == 'd')
			{
				if (((int)u_val) < 0)
				{
					u_val = - u_val;
					rprintfChar(nUart, '-');
				}
				while (div_val > 1 && div_val > u_val) div_val /= 10;
			}
			do
			{
				//rprintfChar(pgm_read_byte(HexChars+(u_val/div_val)));
				rprintfu04(nUart, u_val/div_val);
				u_val %= div_val;
				div_val /= base;
			} while (div_val);
		}
	}
	va_end(ap);
}

/*------------------------------------------------------------------------------------*/

#endif



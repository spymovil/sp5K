/*! \file uart2.h \brief Dual UART driver with buffer support. */
//*****************************************************************************
//
// File Name	: 'uart2.h'
// Title		: Dual UART driver with buffer support
// Author		: Pascal Stang - Copyright (C) 2000-2002
// Created		: 11/20/2000
// Revised		: 07/04/2004
// Version		: 1.0
// Target MCU	: ATMEL AVR Series
// Editor Tabs	: 4
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
///	\ingroup driver_avr
/// \defgroup uart2 UART Driver/Function Library for dual-UART processors (uart2.c)
/// \code #include "uart2.h" \endcode
/// \par Overview
///		This is a UART driver for AVR-series processors with two hardware
///		UARTs such as the mega161 and mega128. This library provides both
///		buffered and unbuffered transmit and receive functions for the AVR
///		processor UART.�Buffered access means that the UART can transmit
///		and receive data in the "background", while your code continues
///		executing.� Also included are functions to initialize the UARTs,
///		set the baud rate, flush the buffers, and check buffer status.
///
/// \note	For full text output functionality, you may wish to use the rprintf
///		functions along with this driver.
///
/// \par About UART operations
///		Most Atmel AVR-series processors contain one or more hardware UARTs
///		(aka, serial ports).  UART serial ports can communicate with other 
///		serial ports of the same type, like those used on PCs.  In general,
///		UARTs are used to communicate with devices that are RS-232 compatible
///		(RS-232 is a certain kind of serial port).
///	\par
///		By far, the most common use for serial communications on AVR processors
///		is for sending information and data to a PC running a terminal program.
///		Here is an exmaple:
///	\code
/// uartInit();					// initialize UARTs (serial ports)
/// uartSetBaudRate(0, 9600);	// set UART0 speed to 9600 baud
/// uartSetBaudRate(1, 115200);	// set UART1 speed to 115200 baud
///
/// rprintfInit(uart0SendByte);	// configure rprintf to use UART0 for output
/// rprintf("Hello UART0\r\n");	// send "hello world" message via UART0
///
/// rprintfInit(uart1SendByte);	// configure rprintf to use UART1 for output
/// rprintf("Hello UART1\r\n");	// send "hello world" message via UART1
/// \endcode
///
/// \warning The CPU frequency (F_CPU) must be set correctly in \c global.h
///		for the UART library to calculate correct baud rates.  Furthermore,
///		certain CPU frequencies will not produce exact baud rates due to
///		integer frequency division round-off.  See your AVR processor's
///		 datasheet for full details.
//
//*****************************************************************************
//@{

#ifndef UART2_FRTOS_H_
#define UART2_FRTOS_H_

//------------------------------------------------------------------------------------
// UARTs

#define UART0	0
#define UART1	1

#define GPRS_UART	0
#define CMD_UART 	1

#define serBAUD_DIV_CONSTANT			( ( unsigned portLONG ) 16 )

/* Tamao de las colas de la UART */
#define uart0RXBUFFER_LEN	640
#define uart0TXBUFFER_LEN	( ( u08 ) ( 128 ))
#define uart1RXBUFFER_LEN	( ( u08 ) ( 128 ))
#define uart1TXBUFFER_LEN	( ( u08 ) ( 128 ))

/* Constants for writing to UCSRB. */
#define serRX_INT_ENABLE				( ( unsigned portCHAR ) 0x80 )
#define serRX_ENABLE					( ( unsigned portCHAR ) 0x10 )
#define serTX_INT_ENABLE				( ( unsigned portCHAR ) 0x20 )
#define serTX_ENABLE					( ( unsigned portCHAR ) 0x08 )

/* Constants for writing to UCSRC. */
#define serUCSRC_SELECT					( ( unsigned portCHAR ) 0x00 )
#define serEIGHT_DATA_BITS				( ( unsigned portCHAR ) 0x06 )

#define UART_DEFAULT_BAUD_RATE		9600	///< default baud rate for UART0
/*------------------------------------------------------------------------------------*/

#define vUart0InterruptOn()									\
{															\
	unsigned portCHAR ucByte;								\
															\
	ucByte = UCSR0B;										\
	ucByte |= serTX_INT_ENABLE;								\
	UCSR0B = ucByte;										\
}
/*------------------------------------------------------------------------------------*/

#define vUart1InterruptOn()									\
{															\
	unsigned portCHAR ucByte;								\
															\
	ucByte = UCSR1B;										\
	ucByte |= serTX_INT_ENABLE;								\
	UCSR1B = ucByte;										\
}

/*------------------------------------------------------------------------------------*/

#define vUart0InterruptOff()								\
{															\
	unsigned portCHAR ucInByte;							\
															\
	ucInByte = UCSR0B;										\
	ucInByte &= ~serTX_INT_ENABLE;							\
	UCSR0B = ucInByte;										\
}
/*------------------------------------------------------------------------------------*/

#define vUart1InterruptOff()								\
{															\
	unsigned portCHAR ucInByte;							\
															\
	ucInByte = UCSR1B;										\
	ucInByte &= ~serTX_INT_ENABLE;							\
	UCSR1B = ucInByte;										\
}
/*------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------*/

void xUartInit(  u08 nUart, u32 baudrate);
void xUartEnable(u08 nUart);
void xUartDisable(u08 nUart);

s08 xUartPutChar( u08 nUart, u08 cOutChar, u08 xBlockTime );
s08 xUart0PutChar( u08 cOutChar, u08 xBlockTime );
s08 xUart1PutChar( u08 cOutChar, u08 xBlockTime );
s08 cmdUartPutChar( u08 cOutChar );

s08 xUartGetChar( u08 nUart, u08 *pcRxedChar, u08 xBlockTime );

s08 uartQueueIsEmpty(u08 nUart);
void xUartQueueFlush(u08 nUart);

#endif /* UART2_FRTOS_H_ */

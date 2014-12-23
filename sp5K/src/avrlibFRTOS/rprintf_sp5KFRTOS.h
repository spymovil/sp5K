/*! \file rprintf.h \brief printf routine and associated routines. */
//****************************************************************************
//
// File Name	: 'rprintf.h'
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
///	\ingroup general
/// \defgroup rprintf printf() Function Library (rprintf.c)
/// \code #include "rprintf.h" \endcode
/// \par Overview
///		The rprintf function library provides a simplified (reduced) version of
///		the common C printf() function.� See the code files for details about
///		which printf features are supported.� Also in this library are a
///		variety of functions for fast printing of certain common data types
///		(variable types).� Functions include print string from RAM, print
///		string from ROM, print string snippet, print hex byte/short/long, and
///		a custom-formatted number print, as well as an optional floating-point
///		print routine.
///
/// \note	All output from the rprintf library can be directed to any device
///		or software which accepts characters.� This means that rprintf output
///		can be sent to the UART (serial port) or can be used with the LCD
///		display libraries to print formatted text on the screen.
//
//****************************************************************************
//@{

#ifndef RPRINTF_H
#define RPRINTF_H

// needed for use of PSTR below
#include <avr/pgmspace.h>
#include "sp5KFRTOS.h"

// configuration
// defining RPRINTF_SIMPLE will compile a smaller, simpler, and faster printf() function
// defining RPRINTF_COMPLEX will compile a larger, more capable, and slower printf() function
#ifndef RPRINTF_COMPLEX
	#define RPRINTF_SIMPLE
#endif

// Define RPRINTF_FLOAT to enable the floating-point printf function: rprintfFloat()
// (adds +4600bytes or 2.2Kwords of code)
#define RPRINTF_FLOAT

// defines/constants
#define STRING_IN_RAM	0
#define STRING_IN_ROM	1

// make a putchar for those that are used to using it
//#define putchar(c)	rprintfChar(c);

// functions

void rprintfInit(void (*putchar_func)(unsigned char c));

//! prints a single character to the current output device
void rprintfChar(u08 nUart, unsigned char c);

//! prints a null-terminated string stored in RAM
void rprintfStr(u08 nUart, char str[]);

//! prints a string stored in program rom
/// \note This function does not actually store your string in
/// program rom, but merely reads it assuming you stored it properly.
void rprintfProgStr(u08 nUart, const prog_char str[]);

//! Using the function rprintfProgStrM(...) automatically causes 
/// your string to be stored in ROM, thereby not wasting precious RAM.
/// Example usage:
/// \code
/// rprintfProgStrM("Hello, this string is stored in program rom");
/// \endcode
#define rprintfProgStrM(nUart, string)			(rprintfProgStr(nUart, PSTR(string)))

//! Prints a carriage-return and line-feed.
/// Useful when printing to serial ports/terminals.
void rprintfCRLF(u08 nUart);

// Prints the number contained in "data" in hex format
// u04,u08,u16,and u32 functions handle 4,8,16,or 32 bits respectively
void rprintfu04(u08 nUart, unsigned char data);	///< Print 4-bit hex number. Outputs a single hex character.

#ifdef RPRINTF_FLOAT
	//! floating-point print routine
	void rprintfFloat(u08 nUart, char numDigits, double x);
#endif


#ifdef RPRINTF_SIMPLE
	//! A simple printf routine.
	/// Called by rprintf() - does a simple printf (supports %d, %x, %c).
	/// Supports:
	/// - %d - decimal
	/// - %x - hex
	/// - %c - character
	int rprintf1RamRom(u08 nUart, unsigned char stringInRom, const char *format, ...);
	int dRprintf1RamRom(u08 nUart, u08 dModo, unsigned char stringInRom, const char *format, ...);

	// #defines for RAM or ROM operation
	#define rprintf1(nUart, format, args...)  		rprintf1RamRom(nUart, STRING_IN_ROM, PSTR(format), ## args)
	#define rprintf1RAM(nUart, format, args...)		rprintf1RamRom(nUart, STRING_IN_RAM, format, ## args)

	// *** Default rprintf(...) ***
	// this next line determines what the the basic rprintf() defaults to:
	#define rprintf(nUart, format, args...)  		rprintf1RamRom(nUart, STRING_IN_ROM, PSTR(format), ## args)
	#define dRprintf(nUart, dModo, format, args...)  	    dRprintf1RamRom(nUart, dModo, STRING_IN_ROM, PSTR(format), ## args)
#endif


#endif
//@}

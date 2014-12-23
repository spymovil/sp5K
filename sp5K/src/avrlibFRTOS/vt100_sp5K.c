/*! \file vt100.c \brief VT100 terminal function library. */
//*****************************************************************************
//
// File Name	: 'vt100.c'
// Title		: VT100 terminal function library
// Author		: Pascal Stang - Copyright (C) 2002
// Created		: 2002.08.27
// Revised		: 2002.08.27
// Version		: 0.1
// Target MCU	: Atmel AVR Series
// Editor Tabs	: 4
//
// NOTE: This code is currently below version 1.0, and therefore is considered
// to be lacking in some functionality or documentation, or may not be fully
// tested.  Nonetheless, you can expect most functions to work.
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#include "sp5KFRTOS.h"

// Program ROM constants

// Global variables

// Functions
void vt100Init(u08 nUart)
{
	// initializes terminal to "power-on" settings
	// ESC c
	rprintfProgStrM(nUart, "\x1B\x63");
}

void vt100ClearScreen(u08 nUart)
{
	// ESC [ 2 J
	rprintfProgStrM(nUart, "\x1B[2J");
}

void vt100SetAttr(u08 nUart, u08 attr)
{
	// ESC [ Ps m
	rprintf(nUart, "\x1B[%dm",attr);
}

void vt100SetCursorMode(u08 nUart, u08 visible)
{
	if(visible)
		// ESC [ ? 25 h
		rprintf(nUart, "\x1B[?25h");
	else
		// ESC [ ? 25 l
		rprintf(nUart, "\x1B[?25l");
}

void vt100SetCursorPos(u08 nUart, u08 line, u08 col)
{
	// ESC [ Pl ; Pc H
	rprintf(nUart, "\x1B[%d;%dH",line,col);
}


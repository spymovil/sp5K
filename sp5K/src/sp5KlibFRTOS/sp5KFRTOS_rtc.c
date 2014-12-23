/*
 * sp5KFRTOS_rtc.c
 *
 *  Created on: 01/11/2013
 *      Author: root
 *
 * Funciones del RTC DS1340-33 modificadas para usarse con FRTOS.
 *
 *
 */
//------------------------------------------------------------------------------------

#include "sp5KFRTOS.h"

static char bcd2dec(char num);
static char dec2bcd(char num);

u08 data[8];
//------------------------------------------------------------------------------------

s08 rtcGetTime(RtcTimeType *rtc)
{
	// Retorna la hora formateada en la estructura RtcTimeType
	// No retornamos el valor de EOSC ni los bytes adicionales.

u08 oscStatus;
s08 retStatus;

	retStatus = RTC_read( &data );
	if (!retStatus)
		return(-1);

	// Decodifico los resultados.
	oscStatus = 0;
	if ( (data[0] & 0x80) == 0x80 ) {		// EOSC
		oscStatus = 1;
	}
	rtc->sec = bcd2dec(data[0] & 0x7F);
	rtc->min = bcd2dec(data[1]);
	rtc->hour = bcd2dec(data[2] & 0x3F);
	rtc->day = bcd2dec(data[4] & 0x3F);
	rtc->month = bcd2dec(data[5] & 0x1F);
	rtc->year = bcd2dec(data[6]) + 2000;

	return(1);
}
//------------------------------------------------------------------------------------

s08 rtcSetTime(RtcTimeType *rtc)
{
	// Setea el RTC con la hora pasada en la estructura RtcTimeType

s08 retStatus;
//u08 i;

	data[0] = 0;	// EOSC = 0 ( rtc running)
	data[1] = dec2bcd(rtc->min);
	data[2] = dec2bcd(rtc->hour);
	data[3] = 0;
	data[4] = dec2bcd(rtc->day);
	data[5] = dec2bcd(rtc->month);
	data[6] = dec2bcd(rtc->year);

//	for ( i=0; i<7; i++) {
//		rprintf(CMD_UART, "I=%d, v=%x\r\n\0", i, data[i]);
//	}
	retStatus = RTC_write( &data );
	return(retStatus);
}
//------------------------------------------------------------------------------------

s08 RTC_read( char *data)
{
	// Funcion generica que lee los 8 primeros bytes de datos del RTC
	// Hay que atender que *data tenga al menos los 8 bytes.

s08 ret = FALSE;

	ret = i2c_masterRead (DS1340, 0, DS1340_ID, DS1340_ADDR, 8, data);
	return(ret);
}
//------------------------------------------------------------------------------------

s08 RTC_write( char *data)
{
s08 ret = FALSE;

	// Funcion generica que escribe los 7 primeros bytes del RTC
	ret = i2c_masterWrite (DS1340, 0, DS1340_ID, DS1340_ADDR, 7, data);
	return(ret);
}
//------------------------------------------------------------------------------------

static char dec2bcd(char num)
{
	// Convert Decimal to Binary Coded Decimal (BCD)
	return ((num/10 * 16) + (num % 10));
}
//------------------------------------------------------------------------------------

static char bcd2dec(char num)
{
	// Convert Binary Coded Decimal (BCD) to Decimal
	return ((num/16 * 10) + (num % 16));
}
//------------------------------------------------------------------------------------

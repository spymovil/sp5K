/*
 * sp5KFRTOS_ads7828.c
 *
 *  Created on: 16/01/2014
 *      Author: root
 *
 *  Funciones del ADS7828 modificadas para usarse con FRTOS.
 *  Se toman como base las funciones de avrlib.
 *
 */
//------------------------------------------------------------------------------------

#include "sp5KFRTOS.h"

//------------------------------------------------------------------------------------

s08 ADS7828_write( u08 *data)
{
s08 ret = FALSE;

	ret = i2c_masterWrite (ADS7828, 0 , ADS7828_ID, ADS7828_ADDR, 1, data);
	return(ret);

}
//------------------------------------------------------------------------------------

s08 ADS7828_read( u08 *data)
{
s08 ret = FALSE;

	ret = adc_masterRead (ADS7828, 0 , ADS7828_ID, ADS7828_ADDR, 2, data);
	return(ret);

}
//------------------------------------------------------------------------------------

void ADS7828_init(void)
{
	return;
}
//------------------------------------------------------------------------------------

s08 ADS7828_convert(u08 channel, u16 *value)
{

u08 ads7828Channel;
u08 ads7828CmdByte;
u08 buffer[2];
u08 status;
u16 retValue;

	if ( channel > 7) {
		return(FALSE);
	}
	// Convierto el canal 0-7 al C2/C1/C0 requerido por el conversor.
	ads7828Channel = (((channel>>1) | (channel&0x01)<<2)<<4) | ADS7828_CMD_SD;
	// do conversion
	// Armo el COMMAND BYTE
	ads7828CmdByte = ads7828Channel & 0xF0;	// SD=1 ( single end inputs )
	ads7828CmdByte |= ADS7828_CMD_PDMODE2;	// Internal reference ON, A/D converter ON
	// start conversion on requested channel

	status = ADS7828_write( &ads7828CmdByte);
	if (!status) {
		return(status);
	}

	// Espero el settle time
	//vTaskDelay(1);

	// retrieve conversion result
	status = ADS7828_read( &buffer);
	retValue = (buffer[0]<<8) | buffer[1];
//	rprintf(CMD_UART, "ADC chD[%d][%d][%x][%d][%x]\r\n",retValue, buffer[0],buffer[0], buffer[1],buffer[1]);
	// pack bytes and return result
	*value = retValue;

	// Apago el conversor
//	ads7828CmdByte = ads7828Channel & 0xF0;
//	ads7828CmdByte |= ADS7828_CMD_PDMODE0;	// Internal reference OFF, A/D converter OFF
//	status = ADS7828_write( &ads7828CmdByte);

	return (status);

}
//------------------------------------------------------------------------------------


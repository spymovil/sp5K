/*
 *  sp5KFRTOS_eeprom.c
 *
 *  Created on: 01/11/2013
 *      Author: root
 *
 *  Funciones de EE modificadas para usarse con FRTOS.
 */
//------------------------------------------------------------------------------------

#include "sp5KFRTOS.h"

//------------------------------------------------------------------------------------

s08 EE_read( u16 i2c_address,u08 length, char *data)
{
s08 ret = FALSE;

	ret = i2c_masterRead (EE24X, i2c_address, EE_ID, EE_ADDR, length, data);
	return(ret);

}
//------------------------------------------------------------------------------------

s08 EE_write( u16 i2c_address,u08 length, char *data)
{
s08 ret = FALSE;

	ret = i2c_masterWrite (EE24X, i2c_address, EE_ID, EE_ADDR, length, data);
	return(ret);

}
//------------------------------------------------------------------------------------

s08 EE_pageWrite( u16 i2c_address,u08 length, char *data)
{
s08 ret = FALSE;

	if (length > EERDBUFLENGHT) {
		length = EERDBUFLENGHT;
	}

	if (  (i2c_address % EEPAGESIZE) != 0 ) {
		i2c_address = (i2c_address / EEPAGESIZE ) * EEPAGESIZE;
	}

	ret = i2c_masterWrite (EE24X, i2c_address, EE_ID, EE_ADDR, length, data);
	return(ret);

}
//------------------------------------------------------------------------------------

/*
 *  sp5KFRTOS_mcp.c
 *
 *  Created on: 01/11/2013
 *      Author: root
 *
 *  Funciones del MCP23008 modificadas para usarse con FRTOS.
 */
//------------------------------------------------------------------------------------

#include "sp5KFRTOS.h"

//------------------------------------------------------------------------------------

s08 MCP_write( u16 i2c_address, u08 dev_addr, u08 length, u08 *data)
{
s08 ret = FALSE;

	ret = i2c_masterWrite (MPC23008, i2c_address, MCP_ID, dev_addr, length, data);
	return(ret);

}
//------------------------------------------------------------------------------------

s08 MCP_read( u16 i2c_address, u08 dev_addr, u08 length, u08 *data)
{
s08 ret = FALSE;

	ret = i2c_masterRead (MPC23008, i2c_address, MCP_ID, dev_addr, length, data);
	return(ret);

}
//------------------------------------------------------------------------------------

void MCP_init(void)
{
	// inicializo los MCP para la configuracion de pines del HW sp5K.
	// Como accedo al bus I2C, debo hacerlo una vez que arranco el RTOS.

u08 data,status;
u08 regValue;
s08 retS;

	// Detecto si tengo un MCP23008 o MCP23018 ( placa c/valvulas)
	retS = FALSE;
	mcpDevice = MCP_NONE;
	regValue = 0;
	retS = MCP_read( MCPADDR_IODIR, MCP_ADDR1, 1, &regValue);
	if (retS) {
		// Lei correctamente un registro del MCP23008. En POR el registro 0 debe ser 0xFF
//		if ( regValue == 0xFF ) {
			mcpDevice = MCP23008;
//		}
	}

	regValue = 0;
	retS = MCP_read( MCP2_IODIRA, MCP_ADDR2, 1, &regValue);
	if (retS) {
		// Lei correctamente un registro del MCP23018. En POR el registro 0 debe ser 0xFF
//		if ( regValue == 0xFF ) {
			mcpDevice = MCP23018;
//		}
	}

	// MCP0_IODIR: inputs(1)/outputs(0)
	data = 0;
	data |= ( BV(MCP_GPIO_IGPRSDCD) | BV(MCP_GPIO_IGPRSRI) );
	status = MCP_write( MCPADDR_IODIR, MCP_ADDR0, 1, &data);

	// MCP0_IPOL: polaridad normal
	data = 0;
	status = MCP_write( MCPADDR_IPOL, MCP_ADDR0, 1, &data);

	// MCP0_GPINTEN: inputs interrupt on change.
	data = 0;
	//data |= ( BV(MCP_GPIO_IGPRSDCD) | BV(MCP_GPIO_IGPRSRI) | BV(MCP_GPIO_ITERMPWRSW) );
	data |=  BV(MCP_GPIO_IGPRSDCD);
	status = MCP_write( MCPADDR_GPINTEN, MCP_ADDR0, 1, &data);

	// MCP0_INTCON: Compara contra su valor anterior
	data = 0;
	status = MCP_write( MCPADDR_INTCON, MCP_ADDR0, 1, &data);

	// MCP0_IOCON: INT active H
	data = 2;
	status = MCP_write( MCPADDR_IOCON, MCP_ADDR0, 1, &data);

	// MCP0_GPPU: pull-ups
	// Habilito los pull-ups en DCD
	data = 0;
//	data |= ( BV(MCP_GPIO_IGPRSDCD) );
	status = MCP_write( MCPADDR_GPPU, MCP_ADDR0, 1, &data);

	//
	// TERMPWR ON
	// Al arrancar prendo la terminal para los logs iniciales.
	data = 0;
	data |= BV(MCP_GPIO_OTERMPWR);	// TERMPWR = 1
	status = MCP_write( MCPADDR_OLAT, MCP_ADDR0, 1, &data);

	//---------------------------------------------------------

	if ( mcpDevice == MCP23008 ) {
		// MCP1_IODIR: inputs(1)/outputs(0)
		// Configuro DIN0/DIN1 como entradas(1) y SENSORSPWR como output(0)
		data = 0;
		data |= ( BV(MCP_GPIO_DIN0) | BV(MCP_GPIO_DIN1) );
		status = MCP_write( MCPADDR_IODIR, MCP_ADDR1, 1, &data);

		// MCP1_GPPU: pull-ups
		// Habilito los pull-ups en DIN0/1
		data = 0;
		data |= ( BV(MCP_GPIO_DIN0) | BV(MCP_GPIO_DIN1) );
		status = MCP_write( MCPADDR_GPPU, MCP_ADDR1, 1, &data);

		// MCP1_GPINTEN: inputs interrupt on change.
		// Habilito que DIN0/1 generen una interrupcion on-change.
		data = 0;
		data |= ( BV(MCP_GPIO_DIN0) | BV(MCP_GPIO_DIN1) );
		status = MCP_write( MCPADDR_GPINTEN, MCP_ADDR1, 1, &data);

		// MCP1_DEFVAL: valores por defecto para comparar e interrumpir
		//data = 0;
		//status = MCP_write( MCPADDR_DEFVAL, MCP_ADDR1, 1, &data);

		// MCP1_INTCON: controlo como comparo para generar la interrupcion.
		// Con 1, comparo contra el valor fijado en DEFVAL
		// Con 0 comparo contra su valor anterior.
		data = 0;
		//data |= ( BV(MCP_GPIO_DIN0) | BV(MCP_GPIO_DIN1) );
		status = MCP_write( MCPADDR_INTCON, MCP_ADDR1, 1, &data);

		// MCP1_IOCON: INT active H.
		data = 2;
		status = MCP_write( MCPADDR_IOCON, MCP_ADDR1, 1, &data);
	}

	//
	//---------------------------------------------------------

	if ( mcpDevice == MCP23018 ) {
		// MCP2:
		// IOCON
		data = 0x63; // 0110 0011
		//                      1->INTCC:Read INTCAP clear interrupt
		//                     1-->INTPOL: INT out pin active high
		//                    0--->ORDR: Active driver output. INTPOL set the polarity
		//                   0---->X
		//                 0----->X
		//                1------>SEQOP: sequential disabled. Address ptr does not increment
		//               1------->MIRROR: INT pins are ored
		//              0-------->BANK: registers are in the same bank, address sequential
		status = MCP_write( MCP2_IOCON, MCP_ADDR2, 1, &data);
		//
		// DIRECCION
		// 0->output
		// 1->input
		data = 0x80; // 1000 0000 ( GPA0..GPA6: outputs, GPA7 input )
		status = MCP_write( MCP2_IODIRA	, MCP_ADDR2, 1, &data);
		data = 0x64; // 0110 0100
		status = MCP_write( MCP2_IODIRB	, MCP_ADDR2, 1, &data);
		//
		// PULL-UPS
		// 0->disabled
		// 1->enabled
		data = 0xFF; // 1111 1111
		status = MCP_write( MCP2_GPPUA, MCP_ADDR2, 1, &data);
		data = 0xFF; // 1111 1111
		status = MCP_write( MCP2_GPPUB, MCP_ADDR2, 1, &data);
		//
		// Valores iniciales de las salidas en 0
		data = 0x00;
		status = MCP_write( MCP2_OLATA, MCP_ADDR2, 1, &data);
		status = MCP_write( MCP2_OLATB, MCP_ADDR2, 1, &data);
		//
		// GPINTEN: inputs interrupt on change.
		// Habilito que DIN0/1 generen una interrupcion on-change.
		// El portA no genera interrupciones
		data = 0;
		status = MCP_write( MCP2_GPINTENA, MCP_ADDR2, 1, &data);
		//data = 0x60; // 0110 0000
		data |= ( BV(MCP2_GPIO_DIN0) | BV(MCP2_GPIO_DIN1) );
		status = MCP_write( MCP2_GPINTENB, MCP_ADDR2, 1, &data);
		//
		// DEFVALB: valores por defecto para comparar e interrumpir
		//data = 0;
		//status = MCP_write( MCP2_DEFVALB, MCP_ADDR2, 1, &data);

		// INTCON: controlo como comparo para generar la interrupcion.
		// Con 1, comparo contra el valor fijado en DEFVAL
		// Con 0 vs. su valor anterior.
		data = 0;
		//data |= ( BV(MCP2_GPIO_DIN0) | BV(MCP2_GPIO_DIN1) );
		status = MCP_write( MCP2_INTCONB, MCP_ADDR2, 1, &data);

		// Borro interrupciones pendientes
		status = MCP_read( MCP2_INTCAPB, MCP_ADDR2 , 1, &data);
	}

}
//------------------------------------------------------------------------------------

s08 MCP_queryDcd( u08 *pin)
{
// MCP23008 logic

s08 retS;
u08 regValue;

	// DCD es el bit1, mask = 0x02

	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	*pin = ( regValue & 0x02) >> 1;
	//*pin = ( regValue & _BV(1) ) >> 1;		// bit1, mask = 0x02
	return(retS);
}
//------------------------------------------------------------------------------------

s08 MCP_queryRi( u08 *pin)
{
// MCP23008 logic

s08 retS;
u08 regValue;

	// RI es el bit2, mask = 0x04

	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	*pin = ( regValue & 0x04) >> 2;
	*pin = ( regValue & _BV(2) ) >> 1;		// bit2, mask = 0x04
	return(retS);
}

//------------------------------------------------------------------------------------

s08 MCP_queryTermPwrSw( u08 *pin)
{
// MCP23008 logic

s08 retS = TRUE;
u08 regValue;

	// TERMSW es el bit 7, mask = 0x80
	//retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	//*pin = ( regValue & 0x80) >> 7;

	// El TERMSW se cablea a PD7.
	*pin = ( TERMSW_PIN & _BV(7) ) >> 7;
	return(retS);
}
//------------------------------------------------------------------------------------

s08 MCP_setLed( u08 value )
{
// MCP23008 logic

s08 retS;
u08 regValue;


	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP_GPIO_OLED);	// Led = 0
		} else {
			regValue |= BV(MCP_GPIO_OLED);	// Led = 1
		}
		retS = MCP_write( MCPADDR_OLAT, MCP_ADDR0, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------

s08 MCP_setGprsPwr( u08 value )
{
// MCP23008 logic

s08 retS;
u08 regValue;

	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP_GPIO_OGPRSPWR);	// GPRSPWR = 0
		} else {
			regValue |= BV(MCP_GPIO_OGPRSPWR);	// GPRSPWR = 1
		}
		retS = MCP_write( MCPADDR_OLAT, MCP_ADDR0, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------

s08 MCP_setGprsSw( u08 value )
{
// MCP23008 logic

s08 retS;
u08 regValue;

	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP_GPIO_OGPRSSW);	// GPRSSW = 0
		} else {
			regValue |= BV(MCP_GPIO_OGPRSSW);	// GPRSSW = 1
		}
		retS = MCP_write( MCPADDR_OLAT, MCP_ADDR0, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------

s08 MCP_setTermPwr( u08 value )
{
// MCP23008 logic

s08 retS;
u08 regValue;

	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP_GPIO_OTERMPWR);	// TERMPWR = 0
		} else {
			regValue |= BV(MCP_GPIO_OTERMPWR);	// TERMPWR = 1
		}
		retS = MCP_write( MCPADDR_OLAT, MCP_ADDR0, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------

s08 MCP_setSensorPwr( u08 value )
{
// MCP23008 | MCP23018	analog

s08 retS = FALSE;
u08 regValue;

	switch (mcpDevice) {
	case MCP23008:
		retS = MCP_read( MCPADDR_GPIO, MCP_ADDR1 , 1, &regValue);
		if ( retS ) {
			if  (value == 0) {
				regValue &= ~BV(MCP_GPIO_PWRSENSORS);		// SENSORPWR = 0
			} else {
				regValue |= BV(MCP_GPIO_PWRSENSORS);		// SENSORPWR = 1
			}
			retS = MCP_write( MCPADDR_OLAT, MCP_ADDR1, 1, &regValue);
		}
		break;
	case MCP23018:
		retS = MCP_read( MCP2_OLATB, MCP_ADDR2 , 1, &regValue);
		if ( retS ) {
			if  (value == 0) {
				regValue &= ~BV(MCP2_PWRSENSORS	);	// SENSORPWR = 0
			} else {
				regValue |= BV(MCP2_PWRSENSORS	);	// SENSORPWR = 1
			}
			retS = MCP_write( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
		}
		break;
	}

	return(retS);

}
//------------------------------------------------------------------------------------

s08 MCP_queryDin0( u08 *pin)
{
// MCP23008 | MCP23018	analog

s08 retS;
u08 regValue;

	switch (mcpDevice) {
	case MCP23008:
		// Din0 es el bit2, mask = 0x04
		retS = MCP_read( MCPADDR_GPIO, MCP_ADDR1 , 1, &regValue);
		*pin = ( regValue & 0x04) >> 2;
		break;
	case MCP23018:
		retS = MCP_read( MCP2_GPIOB, MCP_ADDR2 , 1, &regValue);
		*pin = ( regValue & 0x20) >> 6;
		break;
	}

	return(retS);
}
//------------------------------------------------------------------------------------

s08 MCP_queryDin1( u08 *pin)
{
// MCP23008 | MCP23018	analog

s08 retS;
u08 regValue;

	switch (mcpDevice) {
	case MCP23008:
		// Din1 es el bit3, mask = 0x08
		retS = MCP_read( MCPADDR_GPIO, MCP_ADDR1 , 1, &regValue);
		*pin = ( regValue & 0x08) >> 3;
		break;
	case MCP23018:
		retS = MCP_read( MCP2_GPIOB, MCP_ADDR2 , 1, &regValue);
		*pin = ( regValue & 0x10) >> 5;
		break;
	}
	return(retS);
}
//------------------------------------------------------------------------------------

s08 MCP_setAnalogPwr( u08 value )
{
// MCP23018	analog

s08 retS;
u08 regValue;

	if ( mcpDevice != MCP23018) {
		return(FALSE);
	}

	retS = MCP_read( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	//rprintf( CMD_UART, "retS1=%d, v=%d\r\n", retS, value);

	if ( retS ) {
		if  (value == 0) {
			regValue &= ~BV(MCP2_OANALOG);	// O_ANALOGPWR = 0
		} else {
			regValue |= BV(MCP2_OANALOG);	// O_ANALOGPWR = 1
		}
		retS = MCP_write( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	}

	//rprintf( CMD_UART, "retS2=%d, v=%d\r\n", retS, value);

	return(retS);

}
//------------------------------------------------------------------------------------
// DOBLE CONSIGNA
//------------------------------------------------------------------------------------
s08 MCP_setVReset( u08 value )
{
// MCP23018	analog

s08 retS;
u08 regValue;

	retS = MCP_read( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP2_RESET);	// nRESET = 0
		} else {
			regValue |= BV(MCP2_RESET);		// nRESET = 1
		}
		retS = MCP_write(  MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------
s08 MCP_setVSleep( u08 value )
{
// MCP23018	analog

s08 retS;
u08 regValue;

	retS = MCP_read( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	if ( retS ) {

		if  (value == 0) {
			regValue &= ~BV(MCP2_SLEEP);	// nSLEEP = 0
		} else {
			regValue |= BV(MCP2_SLEEP);		// nSLEEP = 1
		}
		retS = MCP_write(  MCP2_OLATB, MCP_ADDR2, 1, &regValue);
	}

	return(retS);

}
//------------------------------------------------------------------------------------
s08 MCP_setPH(  char phase, u08 chip, u08 value )
{
// MCP23018	analog

s08 retS;
u08 regValue;

	if ( (phase == 'A') && ( chip == 1)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_PHA1);
		}
		if  (value == 1) {
			regValue |= BV(MCP2_PHA1);
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'A') && ( chip == 2)) {
		retS = MCP_read( MCP2_OLATB, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_PHA2);
		}
		if  (value == 1) {
			regValue |= BV(MCP2_PHA2);
		}
		retS = MCP_write(  MCP2_OLATB, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'B') && ( chip == 1)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_PHB1);
		}
		if  (value == 1) {
			regValue |= BV(MCP2_PHB1);
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'B') && ( chip == 2)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_PHB2);
		}
		if  (value == 1) {
			regValue |= BV(MCP2_PHB2);
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	return(retS);

}
//------------------------------------------------------------------------------------
s08 MCP_setEN(  char phase, u08 chip, u08 value )
{
// MCP23018	analog

s08 retS;
u08 regValue;

	if ( (phase == 'A') && ( chip == 1)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_ENA1);	// ENA = 0
		}
		if  (value == 1) {
			regValue |= BV(MCP2_ENA1);	// ENA = 1
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'A') && ( chip == 2)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_ENA2);	// ENA = 0
		}
		if  (value == 1) {
			regValue |= BV(MCP2_ENA2);	// ENA = 1
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'B') && ( chip == 1)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_ENB1);	// ENB = 0
		}
		if  (value == 1) {
			regValue |= BV(MCP2_ENB1);	// ENB = 1
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	if ( (phase == 'B') && ( chip == 2)) {
		retS = MCP_read( MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		if  (value == 0) {
			regValue &= ~BV(MCP2_ENB2);	// ENB = 0
		}
		if  (value == 1) {
			regValue |= BV(MCP2_ENB2);	// ENB = 1
		}
		retS = MCP_write(  MCP2_OLATA, MCP_ADDR2, 1, &regValue);
		return(retS);
	}

	return(retS);

}
//------------------------------------------------------------------------------------

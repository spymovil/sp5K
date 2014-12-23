/*
 * sp5K_utils.c
 *
 *  Created on: 21/01/2014
 *      Author: root
 */
#include "sp5K.h"

//------------------------------------------------------------------------------------

void loadDefaults( systemVarsType *sVars)
{
	// Valores por default.
u08 channel;

	memset (sVars, '\0', sizeof(sVars));
	sVars->initByte = 0x49;
	sVars->initStatus = FALSE;
	strncpy_P(sVars->dlgId, PSTR("SPY999\0"),DLGID_LENGTH);
	strncpy_P(sVars->serverPort, PSTR("80\0"),PORT_LENGTH	);
	strncpy_P(sVars->passwd, PSTR("spymovil123\0"),PASSWD_LENGTH);
	strncpy_P(sVars->serverScript, PSTR("/cgi-bin/sp5K/sp5K.pl\0"),SCRIPT_LENGTH);
//	strncpy_P(sVars->serverScript, PSTR("/cgi-bin/sp5K/test.pl\0"),SCRIPT_LENGTH);

	sVars->csq = 0;
	sVars->dbm = 0;

	sVars->dcd = 0;
	sVars->ri = 0;
	sVars->termsw = 0;

	sVars->wrkMode = WK_NORMAL;
// DEBUG
	sVars->debugLevel = D_NONE;
//	sVars->debugLevel = D_GPRS;
	sVars->logLevel = LOG_NONE;
//	sVars->logLevel = LOG_INFO;

	strncpy_P(sVars->apn, PSTR("SPYMOVIL.VPNANTEL\0"),APN_LENGTH);

	sVars->consigna.tipo = CONS_NONE;
	sVars->consigna.hh_A = 2;
	sVars->consigna.mm_A = 0;
	sVars->consigna.hh_B = 5;
	sVars->consigna.mm_B = 30;

#ifdef CHANNELS_3
	strncpy_P(sVars->serverAddress, PSTR("192.168.0.9\0"),IP_LENGTH);

	sVars->pwrMode = PWR_DISCRETO;
	sVars->timerPoll = 300;			// Poleo c/5 minutos
	sVars->timerDial = 10800;		// Transmito c/3 hs.

	// Todos los canales quedan por default en 0-20mA, 0-6k.
	for ( channel = 0; channel < NRO_CHANNELS; channel++) {
		sVars->Imin[channel] = 0;
		sVars->Imax[channel] = 20;
		sVars->Mmin[channel] = 0;
		sVars->Mmax[channel] = 6;
		sVars->offmmin[channel] = 0;
		sVars->offmmax[channel] = 0;
	}

	// Canales digitales
	strncpy_P(sVars->aChName[0], PSTR("pA\0"),3);
	strncpy_P(sVars->aChName[1], PSTR("pB\0"),3);
	strncpy_P(sVars->aChName[2], PSTR("pC\0"),3);

	strncpy_P(sVars->dChName[0], PSTR("v0\0"),3);
	sVars->magPP[0] = 0.1;
	strncpy_P(sVars->dChName[1], PSTR("v1\0"),3);
	sVars->magPP[1] = 0.1;

#endif

#ifdef CHANNELS_8
	strncpy_P(sVars->serverAddress, PSTR("192.168.1.9\0"),IP_LENGTH);
//	strncpy_P(sVars->serverAddress, PSTR("192.168.0.9\0"),IP_LENGTH);

	sVars->pwrMode = PWR_CONTINUO;
	sVars->timerPoll = 60;			// Poleo c/1 minutos
	sVars->timerDial = 10800;		// Transmito c/3 hs.

	// Todos los canales quedan por default en 0-20mA, 0-100.
	for ( channel = 0; channel < NRO_CHANNELS; channel++) {
		sVars->Imin[channel] = 4;
		sVars->Imax[channel] = 20;
		sVars->Mmin[channel] = 0;
		sVars->Mmax[channel] = 100;
		sVars->offmmin[channel] = 0;
		sVars->offmmax[channel] = 0;
		strncpy_P(sVars->aChName[channel], PSTR("X\0"),2);
	}

	// Canales digitales
	strncpy_P(sVars->dChName[0], PSTR("X\0"),2);
	sVars->magPP[0] = 1;
	strncpy_P(sVars->dChName[1], PSTR("X\0"),2);
	sVars->magPP[1] = 1;

#endif
}
//------------------------------------------------------------------------------------

s08 saveSystemParamsInEE( systemVarsType *sVars )
{

	int EEaddr;
	int eeStatus = 0;

	// Almaceno en EE
	EEaddr = EEADDR_START_PARAMS;
	if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
		paramStore(sVars, EEaddr, sizeof(systemVarsType));
		xSemaphoreGive( sem_systemVars);
	}

	// Confirmo la escritura.
	vTaskDelay( (100 / portTICK_RATE_MS) );
	if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
		eeStatus = paramLoad(sVars, EEaddr, sizeof(systemVarsType));
		xSemaphoreGive( sem_systemVars);
	}

	return(eeStatus);

}
//------------------------------------------------------------------------------------

s08 setParamTimerPoll(u08 *s)
{
s16 timer;

// Actualizo el timer de poleo de sensores de presion en modo Normal
// Se ajusta siempre a un multiplo de TIME_SLOT

	timer = atoi(s);

	// El tiempo de poleo no puede ser inferior a TIME_SLOTs
	if ( timer < TIME_SLOT ) {
		timer = TIME_SLOT;
	}

	// Ajusto el timerPoll a un multiplo entero de TIME_SLOT
	timer = ( timer / TIME_SLOT ) * TIME_SLOT;

	if (timer > 0) {
		systemVars.timerPoll = timer;
		SIGNAL_tkDataRefresh();
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamTimerDial(u08 *s)
{
long timer;

// Actualizo el timer de poleo de sensores de presion en modo Normal

	timer = atol(s);

	if (timer > 0) {
		systemVars.timerDial = timer;
		SIGNAL_tkGprsRefresh();
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamWrkMode(u08 *s0, u08 *s1)
{
/*	if ((!strcmp_P( strupr(s), PSTR("IDLE")))) {
		systemVars.wrkMode = WK_IDLE;
		SIGNAL_tkGprsRefresh();
		SIGNAL_tkDataRefresh();
		return(TRUE);
	}
*/
	if ((!strcmp_P(strupr(s0), PSTR("SERVICE")))) {
		systemVars.wrkMode = WK_SERVICE;
		//vTaskSuspend( h_tkGprs );
		//vTaskSuspend( h_tkData );
		//wdt_disable();
		//sp5K_gprsApagar( sp5KFRTOS_sleep );
		SIGNAL_tkGprsRefresh();
		SIGNAL_tkDataRefresh();
		// Por las dudas
		xSemaphoreGive( sem_CmdUart );
		return(TRUE);
	}

	if ((!strcmp_P(strupr(s0), PSTR("MONITOR")))) {

		systemVars.wrkMode =  WK_MONITOR;

		if ((!strcmp_P( strupr(s1), PSTR("SQE")))) {
			vTaskSuspend( h_tkData );
			wdt_disable();
			SIGNAL_tkGprsRefresh();
		}

		if ((!strcmp_P(strupr(s1), PSTR("FRAME")))) {
			vTaskSuspend( h_tkGprs );
			wdt_disable();
			SIGNAL_tkDataRefresh();
		}

		xSemaphoreGive( sem_CmdUart );
		return(TRUE);
	}

	return(FALSE);

}
//------------------------------------------------------------------------------------

s08 setParamPwrMode(u08 *s)
{
	if ((!strcmp_P( strupr(s), PSTR("CONTINUO")))) {
		systemVars.pwrMode = PWR_CONTINUO;
		SIGNAL_tkGprsRefresh();
		SIGNAL_tkDataRefresh();
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("DISCRETO")))) {
		systemVars.pwrMode = PWR_DISCRETO;
		SIGNAL_tkGprsRefresh();
		SIGNAL_tkDataRefresh();
		return(TRUE);
	}

	return(FALSE);

}
//------------------------------------------------------------------------------------

s08 sp5K_debugControl ( u08 dMode )
{
	/* Esta es la funcion local que debemos definir para que determine si permite la salida
	 * de debug o no.
	 */

	// Si no esta habilitado la modalidad de debug, salgo
	if ( (systemVars.debugLevel & dMode) == 0 ) {
		return(FALSE);
	}

	return(TRUE);

}
//------------------------------------------------------------------------------------

s08 setParamDebugLevel(u08 *s)
{

	if ((!strcmp_P( strupr(s), PSTR("NONE")))) {
		systemVars.debugLevel = D_NONE;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("+DATA")))) {
		systemVars.debugLevel += D_DATA;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("-DATA")))) {
		if ( ( systemVars.debugLevel & D_DATA) != 0 ) {
			systemVars.debugLevel -= D_DATA;
			return(TRUE);
		}
	}

	if ((!strcmp_P( strupr(s), PSTR("+I2C")))) {
		systemVars.debugLevel += D_I2C;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("-I2C")))) {
		if ( ( systemVars.debugLevel & D_I2C) != 0 ) {
			systemVars.debugLevel -= D_I2C;
			return(TRUE);
		}
	}

	if ((!strcmp_P( strupr(s), PSTR("+BD")))) {
		systemVars.debugLevel += D_BD;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("-BD")))) {
		if ( ( systemVars.debugLevel & D_BD) != 0 ) {
			systemVars.debugLevel -= D_BD;
			return(TRUE);
		}
	}

	if ((!strcmp_P( strupr(s), PSTR("+GPRS")))) {
		systemVars.debugLevel += D_GPRS;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("-GPRS")))) {
		if ( ( systemVars.debugLevel & D_GPRS) != 0 ) {
			systemVars.debugLevel -= D_GPRS;
			return(TRUE);
		}
	}

	if ((!strcmp_P( strupr(s), PSTR("+DIGITAL")))) {
		systemVars.debugLevel += D_DIGITAL;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("-DIGITAL")))) {
		if ( ( systemVars.debugLevel & D_DIGITAL) != 0 ) {
			systemVars.debugLevel -= D_DIGITAL;
			return(TRUE);
		}
	}

	if ((!strcmp_P( strupr(s), PSTR("ALL")))) {
		systemVars.debugLevel = D_DATA + D_BD + D_I2C + D_DIGITAL;
		return(TRUE);
	}

	return(FALSE);
}
//------------------------------------------------------------------------------------

s08 setParamLogLevel(u08 *s)
{

	if ((!strcmp_P( strupr(s), PSTR("NONE")))) {
		systemVars.logLevel = LOG_NONE;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("INFO")))) {
		systemVars.logLevel = LOG_INFO;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("WARN")))) {
		systemVars.logLevel = LOG_WARN;
		return(TRUE);
	}

	if ((!strcmp_P( strupr(s), PSTR("ALER")))) {
		systemVars.logLevel = LOG_ALERT;
		return(TRUE);
	}

	return(FALSE);
}
//------------------------------------------------------------------------------------

s08 setParamImin(u08 *s0, u08 *s1)
{
u08 channel, IminX;

	channel = atoi(s0);
	IminX = atoi(s1);

	if (channel < NRO_CHANNELS) {
		systemVars.Imin[channel] = IminX;
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamImax(u08 *s0, u08 *s1)
{
u08 channel, ImaxX;

	channel = atoi(s0);
	ImaxX = atoi(s1);

	if (channel < NRO_CHANNELS) {
		systemVars.Imax[channel] = ImaxX;
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamMmin(u08 *s0, u08 *s1)
{
u08 channel, MminX;

	channel = atoi(s0);
	MminX = atoi(s1);

	if (channel < NRO_CHANNELS) {
		systemVars.Mmin[channel] = MminX;
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamMmax(u08 *s0, u08 *s1)
{
u08 channel;
u16 MmaxX;

	channel = atoi(s0);
	MmaxX = (u16) atol(s1);

	rprintf(CMD_UART, "MMAX %d=%d\r\n", channel, MmaxX);

	if (channel < NRO_CHANNELS) {
		systemVars.Mmax[channel] = MmaxX;
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamOffset(u08 tipo, u08 *s0, u08 *s1)
{
u08 channel;
double OffsetX;

	channel = atoi(s0);
	OffsetX = atof(s1);

	if (channel < NRO_CHANNELS) {
		switch(tipo) {
		case 0: systemVars.offmmin[channel] = OffsetX;
				return(TRUE);
				break;
		case 1: systemVars.offmmax[channel] = OffsetX;
				return(TRUE);
				break;
		}
	}

	return(FALSE);

}
//------------------------------------------------------------------------------------

s08 setParamAname(u08 *s0, u08 *name)
{
u08 channel, OffsetX;

	channel = atoi(s0);

	if (channel < NRO_CHANNELS) {
		memset ( systemVars.aChName[channel], '\0',   PARAMNAME_LENGTH );
		memcpy( systemVars.aChName[channel], name, ( PARAMNAME_LENGTH - 1 ));
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamDname(u08 *s0, u08 *name)
{
u08 channel, OffsetX;

	channel = atoi(s0);

	if (channel < 2) {
		memset ( systemVars.dChName[channel], '\0', PARAMNAME_LENGTH );
		memcpy( systemVars.dChName[channel], name, ( PARAMNAME_LENGTH - 1 ) );
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamMagP(u08 *s0, u08 *s1)
{
u08 channel;
double magPP;

	channel = atoi(s0);
	magPP = atof(s1);

	if (channel < 2) {
		systemVars.magPP[channel] = magPP;
		return(TRUE);
	} else {
		return(FALSE);
	}
}
//------------------------------------------------------------------------------------

s08 setParamConsigna(u08 *s0, u08 *s1)
{

char *s;

	// none
	if (!strcmp_P( strupr(s0), PSTR("NONE\0"))) {
		systemVars.consigna.tipo = CONS_NONE;
		return(TRUE);
	}

	// doble
	if (!strcmp_P( strupr(s0), PSTR("DOBLE\0"))) {
		systemVars.consigna.tipo = CONS_DOBLE;
		return(TRUE);
	}

	// Hora de consigna A
	if (!strcmp_P( strupr(s0), PSTR("DIA\0"))) {
		systemVars.consigna.tipo = CONS_DOBLE;
		systemVars.consigna.hh_A = atoi(s1);
		s = strchr(s1,':');
		s++;
		systemVars.consigna.mm_A = atoi(s);
		return(TRUE);
	}

	// Hora de consigna B
	if (!strcmp_P( strupr(s0), PSTR("NOCHE\0"))) {
		systemVars.consigna.tipo = CONS_DOBLE;
		systemVars.consigna.hh_B = atoi(s1);
		s = strchr(s1,':');
		s++;
		systemVars.consigna.mm_B = atoi(s);
		return(TRUE);
	}

	return(FALSE);

}
//------------------------------------------------------------------------------------
void initBlueTooth(void)
{

	// Inicializo el BlueTooth por si aun no esta inicializado.
	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {

		vt100ClearScreen(CMD_UART);
		vTaskDelay( (portTickType)( 10 / portTICK_RATE_MS ) );
		xUartQueueFlush(UART1);
		// Cambio a 9600 el UART1
		portENTER_CRITICAL();
			outb(UBRR1L, 103);
			outb(UBRR1H, 103>>8);
			outb(UCSR1B, BV(RXCIE1)| BV(RXEN1)|BV(TXEN1));
			UCSR1C = ( serUCSRC_SELECT | serEIGHT_DATA_BITS );
			outb(UCSR1A, BV(U2X1));
		portEXIT_CRITICAL();

		vTaskDelay( (portTickType)( 10 / portTICK_RATE_MS ) );

		rprintfStr(CMD_UART, "AT+BAUD8\r\n" );
		vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );
		xUartQueueFlush(UART1);

		// Cambio a 115200 el UART1
		portENTER_CRITICAL();
			outb(UBRR1L, 8);
			outb(UBRR1H, 8>>8);
			outb(UCSR1B, BV(RXCIE1)| BV(RXEN1)|BV(TXEN1));
			UCSR1C = ( serUCSRC_SELECT | serEIGHT_DATA_BITS );
			outb(UCSR1A, BV(U2X1));
		portEXIT_CRITICAL();

		xSemaphoreGive( sem_CmdUart );
	}

}
//------------------------------------------------------------------------------------

void logPrintStr(t_logLevel logLevel, char *buff)
 {
	// Todos los mensajes de nivel de log igual o inferior al configurado se muestran.

	if ( logLevel > systemVars.logLevel ) {
		return;
	}

 	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
 		rprintfChar(CMD_UART, LOGTAG );
 		rprintfStr(CMD_UART, buff );
 		xSemaphoreGive( sem_CmdUart);
 	}
 }
/*------------------------------------------------------------------------------------*/

long memDataStart()
{
	extern int __data_start;
	return (long) &__data_start;
}
//----------------------------------------------------------------------------------

long memDataEnd ()
{
	extern int __data_end;
	return (long) &__data_end;
}
//----------------------------------------------------------------------------------

long memBssStart()
{
	extern int __bss_start;
	return (long) &__bss_start;
}
//----------------------------------------------------------------------------------

long memBssEnd ()
{
	extern int __bss_end;
	return (long) &__bss_end;
}
//----------------------------------------------------------------------------------

long memHeapStart ()
{
	extern int __heap_start;
	return (long) &__heap_start;
}
//----------------------------------------------------------------------------------

long memHeapEnd()
{
	extern int *__brkval;
	return(__brkval);
}
//----------------------------------------------------------------------------------

long  memRamStart()
{
	char v;
	return (long) &v;

}
/*------------------------------------------------------------------------------------*/

long memRamEnd()
{
	return(RAMEND);
}
/*------------------------------------------------------------------------------------*/

long freeRam ()
{
  extern int __heap_start, *__brkval;
  char v;
  long hs;

  hs = (long)(&v) - (long)(__brkval);
  return (hs);
  //return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

/*------------------------------------------------------------------------------------*/
void initConsignas(void)
{
	// Inicio con el sistema deshabilitado.
	MCP_setEN(  'A', 1, 0 );
	MCP_setEN(  'A', 2, 0 );
	MCP_setEN(  'B', 1, 0 );
	MCP_setEN(  'B', 2, 0 );

	// Y en modo sleep.
	MCP_setVSleep( 0 );
	MCP_setVSleep( 1 );

	// La consigna correspondiente la activo al inicializar el modulo de control.

}
/*------------------------------------------------------------------------------------*/
s08 setValvesPhase(char phase, u08 chip, u08 value)
{

s08 retS = FALSE;

    rprintf(CMD_UART, "vPH: ph=%c, ch=%d, val=%d\r\n\0", phase, chip, value);
	retS = MCP_setPH( phase, chip, value );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesEnable(char phase, u08 chip, u08 value)
{

s08 retS = FALSE;


	//rprintf( CMD_UART, "EN: ph=%d, chip=%d, value=%d\r\n", phase, chip, value);
	retS = MCP_setEN( phase, chip, value );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesReset(u08 value)
{
s08 retS = FALSE;

	retS =  MCP_setVReset( value );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesSleep(u08 value)
{
s08 retS = FALSE;

	retS =  MCP_setVSleep( value );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesPulse(  char phase, u08 chip, u08 value, u16 sleepTime )
{

s08 retS = FALSE;

//		snprintf_P( cmd_printfBuff,CHAR128,PSTR("DEBUG PH: ph=%c, ch=%d, v=%c, s=%d\r\n\0"), phase, chip, value, sleepTime );
//		sp5K_printStr(cmd_printfBuff);

	// Habilito la fase
	MCP_setVSleep( 1 );
	MCP_setVReset( 1 );

	// Genero el pulso
	switch(value) {
	case '+':
		retS = MCP_setPH( phase, chip, 1 );
		retS = MCP_setEN( phase, chip, 1 );
		vTaskDelay( (portTickType)(sleepTime / portTICK_RATE_MS) );
		retS = MCP_setEN( phase, chip, 0 );
		retS = MCP_setPH( phase, chip, 0 );
		break;
	case '-':
		retS = MCP_setPH( phase, chip, 0 );
		retS = MCP_setEN( phase, chip, 1 );
		vTaskDelay( (portTickType)(sleepTime / portTICK_RATE_MS) );
		retS = MCP_setEN( phase, chip, 0 );
		retS = MCP_setPH( phase, chip, 1 );
		break;
	}

	// Deshabilito
	MCP_setVReset( 0 );
	MCP_setVSleep( 0 );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesOpen(char phase, u08 chip )
{

s08 retS;

	retS = setValvesPulse( phase, chip, '+', 100 );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setValvesClose(char phase, u08 chip )
{

	s08 retS;

	retS = setValvesPulse( phase, chip, '-', 100 );
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setConsignaDia(void)
{

s08 retS = FALSE;

	retS = setValvesClose('B', 1);
	vTaskDelay( (portTickType)(1000 / portTICK_RATE_MS) );
	retS = setValvesOpen('A', 1);
	return(retS);

}
/*------------------------------------------------------------------------------------*/
s08 setConsignaNoche(void)
{

s08 retS = FALSE;

	retS = setValvesClose('A', 1);
	vTaskDelay( (portTickType)(1000 / portTICK_RATE_MS) );
	retS = setValvesOpen('B', 1);
	return(retS);

}
/*------------------------------------------------------------------------------------*/

/*
 * sp5K_tkCmd.c
 *
 *  Created on: 27/12/2013
 *      Author: root
 */


#include "sp5K.h"

char cmd_printfBuff[CHAR128];
static void loadSystemParams(void);
struct {
	double adcIn[8];
	u16 batt;
	frameDataType frame;
} dataRecord;


void pv_snprintfP_OK(void )
{
	sp5K_printStr("OK\r\n\0");
}

void pv_snprintfP_ERR(void)
{
	sp5K_printStr("ERROR\r\n\0");
}

/*------------------------------------------------------------------------------------*/

void tkCmd(void * pvParameters)
{

u08 c;

( void ) pvParameters;

	// !!! Requiere que este inicializado el FRTOS porque usa semaforos.
	MCP_init();		// IO ports for hardware

	vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );

	initDebugRtc();
	initBlueTooth();

	vt100ClearScreen(CMD_UART);

	// Inicializo los parametros de trabajo ( leo la internal EE ).
	loadSystemParams();

	// Inicializo la memoria. Leo el estado de los punteros ( leo la external EE).
	BD_init();
	//
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );
	snprintf_P( cmd_printfBuff,CHAR128,PSTR("\r\n.INIT EEmem BD: wrPtr=%d, rdPtr=%d, recUsed=%d, recFree=%d\r\n\0"),  BD_getWRptr(),  BD_getRDptr(), BD_getRcsUsed(), BD_getRcsFree() );
	logPrintStr(LOG_NONE, cmd_printfBuff);
	switch (mcpDevice) {
	case MCP23008:
#ifdef CHANNELS_3
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("System: 3 analogIn,2 digitalIn\r\n\0"));
#endif
#ifdef CHANNELS_8
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("System: 8 analogIn,2 digitalIn\r\n\0"));
#endif
		break;
	case MCP23018:
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("System: 3 analogIn,2 digitalIn,4 valves\r\n\0"));
		break;
	default:
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("ERROR !! No analog system detected\r\n\0"));
		break;
	}
	logPrintStr(LOG_NONE, cmd_printfBuff);

	// Inicializo el sistema de consignas.
	initConsignas();
	switch (systemVars.consigna.tipo) {
	case CONS_NONE:
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Init consignas: none\r\n\0"));
		break;
	case CONS_DOBLE:
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Init consignas: doble\r\n\0"));
		break;
	default:
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Init consignas: ERROR\r\n\0"));
		break;
	}
	logPrintStr(LOG_NONE, cmd_printfBuff);
	//


	// Init Banner.
	snprintf_P( cmd_printfBuff,CHAR128,PSTR("\r\nSpymovil %s %s"), SP5K_MODELO, SP5K_VERSION);
	logPrintStr(LOG_NONE, cmd_printfBuff);
	snprintf_P( cmd_printfBuff,CHAR128,PSTR(" %d-%s-%s"), NRO_CHANNELS, EE_TYPE, FTYPE);
	logPrintStr(LOG_NONE, cmd_printfBuff);
	snprintf_P( cmd_printfBuff,CHAR128,PSTR(" %s %s\r\n\0"), SP5K_REV, SP5K_DATE );
	logPrintStr(LOG_NONE, cmd_printfBuff);

#ifdef MEMINFO
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_Uart0TxQueue=%lu\r\n\0"), HE_Uart0TxQueue );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_Uart1RxQueue=%lu\r\n\0"), HE_Uart1RxQueue );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_Uart1TxQueue=%lu\r\n\0"), HE_Uart1TxQueue );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_I2Cbus=%lu\r\n\0"), HE_I2Cbus );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_CmdUart=%lu\r\n\0"), HE_CmdUart );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_GprsUart=%lu\r\n\0"), HE_GprsUart );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_digitalIn=%lu\r\n\0"), HE_digitalIn );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_systemVars=%lu\r\n\0"), HE_systemVars );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_BD=%lu\r\n\0"), HE_BD );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_tkCmd=%lu\r\n\0"), HE_tkCmd );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_tkDigitalIn=%lu\r\n\0"), HE_tkDigitalIn );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_tkData=%lu\r\n\0"), HE_tkData );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_tkControl=%lu\r\n\0"), HE_tkControl );
	logPrintStr(LOG_NONE, &cmd_printfBuff);
	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("HE_tkGprs=%lu\r\n\0"), HE_tkGprs );
	logPrintStr(LOG_NONE, &cmd_printfBuff);

#endif

	// Habilito al resto del sistema a arrancar
	if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
		systemVars.initStatus = TRUE;
		xSemaphoreGive( sem_systemVars );
	}

	for( ;; )
	{

		// Para medir el uso del stack
		tkCmd_uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		clearWdg(WDG_CMD);
		while(xUartGetChar(CMD_UART, &c, (50 / portTICK_RATE_MS ) ) ) {
			cmdlineInputFunc(c);
			// Cada caracer recibido reinicio el timer.
			restartTimerTerminal();
		}
		/* run the cmdline execution functions */
		cmdlineMainLoop();

	}

}
/*------------------------------------------------------------------------------------*/

static void loadSystemParams(void)
{

u08 EEaddr;
s08 eeStatus = FALSE;
u08 i;
systemVarsType sVars;

	// Leo la configuracion:  Intento leer hasta 3 veces.
	EEaddr = EEADDR_START_PARAMS;
	for (i=0; i<3; i++) {
		eeStatus = paramLoad(&sVars, EEaddr, sizeof(systemVarsType));
		if (eeStatus == TRUE) break;
	}

	vTaskDelay( 100 / portTICK_RATE_MS );

	// Confirmo que pude leer bien.
	if ( (eeStatus == FALSE) || (sVars.initByte != 0x49) ) {
		// No pude leer la EE correctamente o es la primera vez que arranco.
		// Inicializo a default.
		snprintf_P( &cmd_printfBuff,CHAR128,PSTR("loadSystemParams::Load EE error: default !!\r\n\0") );
		logPrintStr(LOG_NONE, &cmd_printfBuff);

		loadDefaults(&sVars);
		// Almaceno en la EE para el proximo arranque.
		paramStore(&sVars, EEaddr, sizeof(systemVars));

	} else {
		// Pude leer la eeprom correctamente.
		snprintf_P( &cmd_printfBuff,CHAR128,PSTR("loadSystemParams::Load config ok !!\r\n\0") );
		logPrintStr(LOG_NONE, &cmd_printfBuff);
	}

	// Siempre debo arrancar con esto en FALSE para aguantar a las tareas.
	sVars.initStatus = FALSE;

	sVars.wrkMode = WK_NORMAL;
	strncpy_P(sVars.dlgIp, PSTR("000.000.000.000\0"),16);
	sVars.csq = 0;
	sVars.dbm = 0;
	sVars.dcd = 0;
	sVars.ri = 0;
	sVars.termsw = 0;

	// Actualizo el systemVars.
	if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
		memcpy(&systemVars, &sVars, sizeof(systemVarsType));
		xSemaphoreGive( sem_systemVars );
	}

	snprintf_P( &cmd_printfBuff,CHAR128,PSTR("loadSystemParams::Config analogs=%d\r\n\0"), NRO_CHANNELS );
	logPrintStr(LOG_NONE, &cmd_printfBuff);

}
//------------------------------------------------------------------------------------

void SP5K_statusFunction(void)
{

RtcTimeType rtcDateTime;
u08 pos, channel;
u16 dialTimer;
u08 mStatus;
frameDataType frame;
u08 gprsState, gprsSubState, dataState;

	rtcGetTime(&rtcDateTime);
	getDataFrame(&frame);
	dialTimer = getTimeToNextDial();

	memset( &cmd_printfBuff, '\0', CHAR128);

	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("\r\nSpymovil %s %s\0"), SP5K_MODELO, SP5K_VERSION);
		rprintfStr(CMD_UART, cmd_printfBuff );
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(" %d-%s-%s\0"), NRO_CHANNELS, EE_TYPE, FTYPE);
		rprintfStr(CMD_UART, cmd_printfBuff );
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(" %s %s\r\n\0"), SP5K_REV, SP5K_DATE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* DlgId */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("dlgid: %s\r\n\0"), systemVars.dlgId );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* Fecha y Hora */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("rtc: %02d/%02d/%04d \0"),rtcDateTime.day,rtcDateTime.month, rtcDateTime.year );
		rprintfStr(CMD_UART, cmd_printfBuff );
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("%02d:%02d:%02d\r\n\0"),rtcDateTime.hour,rtcDateTime.min, rtcDateTime.sec );
		rprintfStr(CMD_UART, cmd_printfBuff );

		// SERVER ---------------------------------------------------------------------------------------
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(">Server:\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* APN */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  apn: %s\r\n\0"), systemVars.apn );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* SERVER IP:SERVER PORT */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  server ip:port: %s:%s\r\n\0"), systemVars.serverAddress,systemVars.serverPort );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* SERVER SCRIPT */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  server script: %s\r\n\0"), systemVars.serverScript );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* SERVER PASSWD */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  passwd: %s\r\n\0"), systemVars.passwd );
		rprintfStr(CMD_UART, cmd_printfBuff );

		// MODEM ---------------------------------------------------------------------------------------
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(">Modem:\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* Modem status */
		mStatus = getModemStatus();
		switch (mStatus) {
		case M_OFF:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  modem: OFF\r\n\0"));
			break;
		case M_OFF_IDLE:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  modem: OFF Idle\r\n\0"));
			break;
		case M_ON_CONFIG:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  modem: ON & configurating.\r\n\0"));
			break;
		case M_ON_READY:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  modem: ON & ready(apn,ip).\r\n\0"));
			break;
		default:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  modem: ERROR\r\n\0"));
			break;
		}
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* DLGIP */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  dlg ip: %s\r\n\0"), systemVars.dlgIp );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* CSQ */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  signalQ: csq=%d, dBm=%d\r\n\0"), systemVars.csq, systemVars.dbm );
		rprintfStr(CMD_UART, cmd_printfBuff );

		// DCD/RI/TERMSW
		if ( systemVars.dcd == 0 ) { pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  pines: dcd=ON,\0")); }
		if ( systemVars.dcd == 1 ) { pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  pines: dcd=OFF,\0"));}
		if ( systemVars.ri == 0 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("ri=ON,\0")); }
		if ( systemVars.ri == 1 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("ri=OFF,\0"));}
		if ( systemVars.termsw == 1 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("term=ON\r\n\0")); }
		if ( systemVars.termsw == 0 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("term=OFF\r\n\0"));}
		rprintfStr(CMD_UART, cmd_printfBuff );

		// SYSTEM ---------------------------------------------------------------------------------------
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(">System:\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* Memoria */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  memory: wrPtr=%d, rdPtr=%d, usedRec=%d, freeRecs=%d\r\n\0"), BD_getWRptr(), BD_getRDptr(), BD_getRcsUsed(), BD_getRcsFree() );
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* WRK mode (NORMAL / SERVICE) */
		switch (systemVars.wrkMode) {
		case WK_NORMAL:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  wrkmode: normal\r\n\0"));
			break;
		case WK_SERVICE:
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("  wrkmode: service\r\n\0"));
			break;
		case WK_MONITOR:
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("  wrkmode: monitor\r\n\0"));
			break;
		default:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  wrkmode: ERROR\r\n\0"));
			break;
		}
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* PWR mode (CONTINUO / DISCRETO) */
		switch (systemVars.pwrMode) {
		case PWR_CONTINUO:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  pwrmode: continuo\r\n\0"));
			break;
		case PWR_DISCRETO:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  pwrmode: discreto\r\n\0"));
			break;
		default:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  pwrmode: ERROR\r\n\0"));

			break;
		}
		rprintfStr(CMD_UART, cmd_printfBuff );

#ifdef CHANNELS_3
		/* Consigna */
		switch (systemVars.consigna.tipo) {
		case CONS_NONE:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  consigna: none\r\n\0"));
			break;
		case CONS_DOBLE:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  consigna: doble (dia->%02d:%02d, noche->%02d:%02d)\r\n\0"), systemVars.consigna.hh_A,systemVars.consigna.mm_A,systemVars.consigna.hh_B,systemVars.consigna.mm_B );
			break;
		default:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  consigna: ERROR\r\n\0"));
			break;
		}
		rprintfStr(CMD_UART, cmd_printfBuff );
#endif

		/* Timers */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  timerPoll: %d/%d\r\n\0"),systemVars.timerPoll, getTimeToNextPoll() );
		rprintfStr(CMD_UART, cmd_printfBuff );

		if (systemVars.pwrMode == PWR_CONTINUO ) {
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  timerDial: -1(%d)/\0"),systemVars.timerDial );
		} else {
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  timerDial: %d/\0"),systemVars.timerDial );
		}
		pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("%d\r\n\0"), dialTimer );
		rprintfStr(CMD_UART, cmd_printfBuff );

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  khTimer: %d\r\n\0"),houseKeepingTimer );
		rprintfStr(CMD_UART, cmd_printfBuff );

		// Debug Options.
		pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  debugLevel: \0"));
		if ( systemVars.debugLevel == D_NONE) {
    		pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("none") );
    	} else {
    		if ( (systemVars.debugLevel & D_DATA) != 0) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("+data")); }
    		if ( (systemVars.debugLevel & D_GPRS) != 0) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("+gprs")); }
    		if ( (systemVars.debugLevel & D_BD) != 0)   { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("+bd")); }
    		if ( (systemVars.debugLevel & D_I2C) != 0)  { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("+i2c")); }
    		if ( (systemVars.debugLevel & D_DIGITAL) != 0)  { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("+digital")); }
    	}
		snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		// Log Options
		pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  logLevel: \0"));
		if ( systemVars.logLevel == LOG_NONE) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("none")); }
		if ( systemVars.logLevel == LOG_INFO) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("info")); }
		if ( systemVars.logLevel == LOG_WARN) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("warn")); }
		if ( systemVars.logLevel == LOG_ALERT) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("alert")); }
		snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		getStatusGprsSM( &gprsState, &gprsSubState);
		getStatusDataSM( &dataState );
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  tkGprs:%d/%d, tkData:%d\r\n\0"), gprsState, gprsSubState, dataState);
		rprintfStr(CMD_UART, cmd_printfBuff );

		// CONFIG ---------------------------------------------------------------------------------------
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(">Config:\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		/* Configuracion del sistema */
		switch (mcpDevice) {
		case MCP23008:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  analog=%d, digital=2.\r\n\0"),NRO_CHANNELS);
			break;
		case MCP23018:
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("  analog=%d, digital=2, valves=4.\r\n\0"),NRO_CHANNELS);
			break;
		default:
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("ERROR !! No analog system detected\r\n\0"));
			break;
		}
		rprintfStr(CMD_UART, cmd_printfBuff );

#ifdef CHANNELS_3
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  batt{0-15V}\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );
#endif

		for ( channel = 0; channel < NRO_CHANNELS; channel++) {
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("  a%d{%d-%dmA/%d-%d, %.2f/%.2f}(%s)\r\n\0"),channel, systemVars.Imin[channel],systemVars.Imax[channel],systemVars.Mmin[channel],systemVars.Mmax[channel], systemVars.offmmin[channel], systemVars.offmmax[channel], systemVars. aChName[channel] );
			rprintfStr(CMD_UART, cmd_printfBuff );
		}
		/* Configuracion de canales digitales */
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  d0{%.2f p/p} (%s)\r\n\0"), systemVars.magPP[0],systemVars.dChName[0]);
		rprintfStr(CMD_UART, cmd_printfBuff );
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("  d1{%.2f p/p} (%s)\r\n\0"), systemVars.magPP[1],systemVars.dChName[1]);
		rprintfStr(CMD_UART, cmd_printfBuff );

		// VALUES ---------------------------------------------------------------------------------------
		snprintf_P( cmd_printfBuff,CHAR128,PSTR(">Values:\r\n\0"));
		rprintfStr(CMD_UART, cmd_printfBuff );

		pos = 0;
		// Armo el frame
		memset( &cmd_printfBuff, NULL, CHAR128);
		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR( "  %04d%02d%02d,"),frame.rtc.year,frame.rtc.month, frame.rtc.day );
		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%02d%02d%02d,"),frame.rtc.hour,frame.rtc.min, frame.rtc.sec );
		// Valores analogicos
		for ( channel = 0; channel < NRO_CHANNELS; channel++) {
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%s=%.2f,"),systemVars.aChName[channel],frame.analogIn[channel] );
		}
		// Valores digitales.
		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%sP=%.2f,"),systemVars.dChName[0], frame.din0_pCount );
		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR( "%sP=%.2f"),  systemVars.dChName[1], frame.din1_pCount );

#ifdef CHANNELS_3
		// Bateria
		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR(",bt=%.2f%"),frame.batt );
#endif

		pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR("\r\n\0") );
		// Imprimo
		rprintfStr(CMD_UART, &cmd_printfBuff );

		xSemaphoreGive( sem_CmdUart );
	}
}
/*------------------------------------------------------------------------------------*/

void sp5K_readFunction(void)
{

u08 devId, address, regValue, pin, channel, pcbChannel, adcChannel;
u08 length = 10;
s08 retS;
u08 mcp_address;
RtcTimeType rtcDateTime;
u08 i;
u16 adcRetValue;
char eeRdBuffer[EERDBUFLENGHT];
u16 eeAddress;
u08 pos;
frameDataType rdFrame;
u16 recCount;
double I,M,D;
u08 bdGetStatus;
u16 rcdIndex;
u08 b[9];

	memset( cmd_printfBuff, NULL, CHAR128);
	makeargv();

	// RTC
	// Lee la hora del RTC.
	if (!strcmp_P( strupr(argv[1]), PSTR("RTC\0"))) {
		retS = rtcGetTime(&rtcDateTime);
		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("%02d/%02d/%04d "),rtcDateTime.day,rtcDateTime.month, rtcDateTime.year );
			pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("%02d:%02d:%02d\r\n\0"),rtcDateTime.hour,rtcDateTime.min, rtcDateTime.sec );
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// DCD
	if (!strcmp_P( strupr(argv[1]), PSTR("DCD\0"))) {
		retS = MCP_queryDcd(&pin);
		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			if ( pin == 1 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("DCD ON\r\n\0")); }
			if ( pin == 0 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("DCD OFF\r\n\0")); }
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// RI
	if (!strcmp_P( strupr(argv[1]), PSTR("RI\0"))) {
		retS = MCP_queryRi(&pin);
		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			if ( pin == 1 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("RI ON\r\n\0")); }
			if ( pin == 0 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("RI OFF\r\n\0")); }
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// DIN0
	if (!strcmp_P( strupr(argv[1]), PSTR("DIN0\0"))) {
		retS = MCP_queryDin0(&pin);
		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("DIN0 %d\r\n\0"), pin);
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// DIN1
	if (!strcmp_P( strupr(argv[1]), PSTR("DIN1\0"))) {
		retS = MCP_queryDin1(&pin);
		if (retS ) {
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("DIN1 %d\r\n\0"), pin);
		} else {
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(cmd_printfBuff);
		return;
	}

	// TERMSW
	if (!strcmp_P( strupr(argv[1]), PSTR("TERMSW\0"))) {
		retS = MCP_queryTermPwrSw(&pin);
		if (retS ) {
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			if ( pin == 1 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("TERMSW ON\r\n\0")); }
			if ( pin == 0 ) { pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("TERMSW OFF\r\n\0")); }
		} else {
			snprintf_P( cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// MCP
	// read mcp 0|1|2 addr
	if (!strcmp_P( strupr(argv[1]), PSTR("MCP\0"))) {
		devId = atoi(argv[2]);
		address = atoi(argv[3]);

		if ( devId == 0 ) { mcp_address = MCP_ADDR0; }
		if ( devId == 1 ) { mcp_address = MCP_ADDR1; }
		if ( devId == 2 ) { mcp_address = MCP_ADDR2; }

		retS = MCP_read( address, mcp_address, 1, &regValue);
		if (retS ) {
			// Convierto el resultado a binario.
			pos = snprintf_P( cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			strcpy(b,byte_to_binary(regValue));
			pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("mcp %d: reg=[%d] data=[0X%03x] [%s] \r\n\0"),devId, address,regValue, b);
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}
	// *****************************************************************************************************
	// SOLO PARA USO EN MODO SERVICE.
	if ( systemVars.wrkMode != WK_SERVICE) {
		snprintf_P( &cmd_printfBuff,CHAR128,PSTR("WARNING: Debe pasar a modo SERVICE !!!\r\n"));
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// ADC
	// read adc channel
	// El canal es de 0..3/0..7 y representa el canal fisico en el conector, NO
	// EL PROPIO CANAL DEL A/D

	if (!strcmp_P( strupr(argv[1]), PSTR("ADC\0"))) {

		pcbChannel = atoi(argv[2]);

		if ( NRO_CHANNELS == 3 ) {
			switch (pcbChannel ) {
			case 0:
				adcChannel = 3;
				break;
			case 1:
				adcChannel = 5;
				break;
			case 2:
				adcChannel = 7;
				break;
			case 3:
				adcChannel = 1;		// Bateria
				break;
			}
		}

		retS = ADS7828_convert( adcChannel, &adcRetValue );

		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("adc_%d(%d)=[%d]\r\n\0"),pcbChannel, adcChannel, adcRetValue);
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// FRAME
	// Lee todos los canales y presenta el frame.
	if (!strcmp_P( strupr(argv[1]), PSTR("FRAME\0"))) {
		SIGNAL_tkDataReadFrame();
		return;
	}

	// EEPROM
	// read ee addr length
	if (!strcmp_P( strupr(argv[1]), PSTR("EE\0"))) {
		eeAddress = atol(argv[2]);
		length = atoi(argv[3]);
		// Buffer control.
		if (length > EERDBUFLENGHT) {
			length = EERDBUFLENGHT;
		}
		retS = EE_read( eeAddress, length, &eeRdBuffer);
		if (retS ) {
			pos = snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n"));
			pos += snprintf_P( &cmd_printfBuff[pos],CHAR128,PSTR("addr=[%d] data=[%s]\r\n\0"),eeAddress,eeRdBuffer);
		} else {
			snprintf_P( &cmd_printfBuff,CHAR128,PSTR("ERROR\r\n\0"));
		}
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// MEMORY DUMP
	if (!strcmp_P( strupr(argv[1]), PSTR("MEMORY\0"))) {

		recCount = 0;

		for (;;) {

			vTaskDelay( (portTickType)(50 / portTICK_RATE_MS) );
			rcdIndex = BD_getRDptr();
			bdGetStatus =  BD_get( &rdFrame, rcdIndex);
			// BD vacia
			if (bdGetStatus == 0) {
				break;
			}

			recCount++;

			// Armo el frame
			memset( cmd_printfBuff, NULL, CHAR128);
			pos = 0;
			pos = snprintf_P( &cmd_printfBuff, CHAR128, PSTR("(%d/%d)>" ), recCount, rcdIndex);
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR( "%04d%02d%02d,"),rdFrame.rtc.year,rdFrame.rtc.month, rdFrame.rtc.day );
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%02d%02d%02d,"),rdFrame.rtc.hour,rdFrame.rtc.min, rdFrame.rtc.sec );
			// Valores analogicos
			for ( channel = 0; channel < NRO_CHANNELS; channel++) {
				pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%s=%.2f"),systemVars.aChName[channel],rdFrame.analogIn[channel] );
				pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR(","));
			}
			// Valores digitales.
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR("%s=%.2f,"), systemVars.dChName[0], rdFrame.din0_pCount );
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR( "%s=%.2f"), systemVars.dChName[1], rdFrame.din0_pCount );
#ifdef CHANNELS_3
			// Bateria
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128, PSTR(",bt=%.2f"), rdFrame.batt );
#endif
			pos += snprintf_P( &cmd_printfBuff[pos], CHAR128,PSTR("<\r\n\0") );

			// Imprimo
			sp5K_printStr(&cmd_printfBuff);

			BD_delete(-1);
			taskYIELD();
		}

		snprintf_P( &cmd_printfBuff,CHAR128,PSTR("OK\r\n\0"));
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// CMD NOT FOUND
	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
		rprintfProgStrM(CMD_UART, "ERROR\r\n");
		rprintfProgStrM(CMD_UART, "CMD NOT DEFINED\r\n");
		xSemaphoreGive( sem_CmdUart );
	}
	return;


}
//------------------------------------------------------------------------------------

void sp5K_writeFunction(void)
{

u08 dateTimeStr[11];
char tmp[3];
RtcTimeType rtcDateTime;
char *p;
u08 devId, address, regValue;
u08 value = 0;
s08 retS = FALSE;
u08  mcp_address = 0;
u08 length = 0;
u08 *nro = NULL;
u08 *msg = NULL;
u08 timeout;
char phase;
u08 chip;
u16 sleepTime;

	memset( cmd_printfBuff, NULL, CHAR128);
	makeargv();

	// Parametros:
	if (!strcmp_P( strupr(argv[1]), PSTR("PARAM\0")))
	{

		// PASSWD
		if (!strcmp_P( strupr(argv[2]), PSTR("PASSWD\0"))) {
			memset(systemVars.passwd, '\0', sizeof(systemVars.passwd));
			memcpy(systemVars.passwd, argv[3], sizeof(systemVars.passwd));
			systemVars.passwd[PASSWD_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		// APN
		if (!strcmp_P( strupr(argv[2]), PSTR("APN\0"))) {
			memset(systemVars.apn, '\0', sizeof(systemVars.apn));
			memcpy(systemVars.apn, argv[3], sizeof(systemVars.apn));
			systemVars.apn[APN_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		// SERVER PORT
		if (!strcmp_P( strupr(argv[2]), PSTR("PORT\0"))) {
			memset(systemVars.serverPort, '\0', sizeof(systemVars.serverPort));
			memcpy(systemVars.serverPort, argv[3], sizeof(systemVars.serverPort));
			systemVars.serverPort[PORT_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		// SERVER IP
		if (!strcmp_P( strupr(argv[2]), PSTR("IP\0"))) {
			memset(systemVars.serverAddress, '\0', sizeof(systemVars.serverAddress));
			memcpy(systemVars.serverAddress, argv[3], sizeof(systemVars.serverAddress));
			systemVars.serverAddress[IP_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		// SERVER SCRIPT
		// write param script cgi-bin/oseApp/scripts/oseSp5K001.pl
		if (!strcmp_P( strupr(argv[2]), PSTR("SCRIPT\0"))) {
			memset(systemVars.serverScript, '\0', sizeof(systemVars.serverScript));
			memcpy(systemVars.serverScript, argv[3], sizeof(systemVars.serverScript));
			systemVars.serverScript[SCRIPT_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		// MagP ( magnitud  por pulso )
		if (!strcmp_P( strupr(argv[2]), PSTR("MAGP\0"))) {
			retS = setParamMagP(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// dname X ( digital name X )
		if (!strcmp_P( strupr(argv[2]), PSTR("DNAME\0"))) {
			retS = setParamDname(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// aname X ( analog name X )
		if (!strcmp_P( strupr(argv[2]), PSTR("ANAME\0"))) {
			retS = setParamAname(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// imin X
		if (!strcmp_P( strupr(argv[2]), PSTR("IMIN\0"))) {
			retS = setParamImin(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// imax X
		if (!strcmp_P( strupr(argv[2]), PSTR("IMAX\0"))) {
			retS = setParamImax(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// Mmin X
		if (!strcmp_P( strupr(argv[2]), PSTR("MMIN\0"))) {
			retS = setParamMmin(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// Mmax X
		if (!strcmp_P( strupr(argv[2]), PSTR("MMAX\0"))) {
			retS = setParamMmax(argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// offmmin X
		if (!strcmp_P( strupr(argv[2]), PSTR("OFFMMIN\0"))) {
			retS = setParamOffset(0,argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// offmmax X
		if (!strcmp_P( strupr(argv[2]), PSTR("OFFMMAX\0"))) {
			retS = setParamOffset(1,argv[3], argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		/* DebugLevel */
		if (!strcmp_P( strupr(argv[2]), PSTR("DEBUGLEVEL\0"))) {
			retS = setParamDebugLevel(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		/* LogLevel */
		if (!strcmp_P( strupr(argv[2]), PSTR("LOGLEVEL\0"))) {
			retS = setParamLogLevel(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		// dlgId
		if (!strcmp_P( strupr(argv[2]), PSTR("DLGID\0"))) {
			memcpy(systemVars.dlgId, argv[3], sizeof(systemVars.dlgId));
			systemVars.dlgId[DLGID_LENGTH - 1] = '\0';
			pv_snprintfP_OK();
			return;
		}

		/* TimerPoll */
		if (!strcmp_P( strupr(argv[2]), PSTR("TIMERPOLL\0"))) {
			retS = setParamTimerPoll(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		/* TimerDial */
		if (!strcmp_P( strupr(argv[2]), PSTR("TIMERDIAL\0"))) {
			retS = setParamTimerDial(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		/* Wrkmode */
		if (!strcmp_P( strupr(argv[2]), PSTR("WRKMODE\0"))) {
			retS = setParamWrkMode(argv[3],argv[4]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

		/* Pwrmode */
		if (!strcmp_P( strupr(argv[2]), PSTR("PWRMODE\0"))) {
			retS = setParamPwrMode(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}

/*		// MONITOR
		// write param monitor [sqe|frame]
		if (!strcmp_P( strupr(argv[2]), PSTR("MONITOR\0"))) {
			retS = setParamMonitor(argv[3]);
			if ( retS ) {
				pv_snprintfP_OK();
			} else {
				pv_snprintfP_ERR();
			}
			return;
		}
*/


	}

	// CONSIGNA
	if (!strcmp_P( strupr(argv[2]), PSTR("CONSIGNA\0"))) {
		retS = setParamConsigna(argv[3], argv[4]);
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// ALL SystemVars ( save )
	if (!strcmp_P( strupr(argv[1]), PSTR("SAVE\0"))) {
		retS = saveSystemParamsInEE( &systemVars );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// RTC
	if (!strcmp_P( strupr(argv[1]), PSTR("RTC\0"))) {
		/* YYMMDDhhmm */
		memcpy(dateTimeStr, argv[2], 10);
		// year
		tmp[0] = dateTimeStr[0]; tmp[1] = dateTimeStr[1];	tmp[2] = '\0';
		rtcDateTime.year = atoi(tmp);
		// month
		tmp[0] = dateTimeStr[2]; tmp[1] = dateTimeStr[3];	tmp[2] = '\0';
		rtcDateTime.month = atoi(tmp);
		// day of month
		tmp[0] = dateTimeStr[4]; tmp[1] = dateTimeStr[5];	tmp[2] = '\0';
		rtcDateTime.day = atoi(tmp);
		// hour
		tmp[0] = dateTimeStr[6]; tmp[1] = dateTimeStr[7];	tmp[2] = '\0';
		rtcDateTime.hour = atoi(tmp);
		// minute
		tmp[0] = dateTimeStr[8]; tmp[1] = dateTimeStr[9];	tmp[2] = '\0';
		rtcDateTime.min = atoi(tmp);

		retS = rtcSetTime(&rtcDateTime);

		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// Si no estoy en modo service no puedo hacer estas tareas administrativas.

	if ( systemVars.wrkMode != WK_SERVICE) {
		snprintf_P( &cmd_printfBuff,CHAR128,PSTR("WARNING: Debe pasar a modo SERVICE !!!\r\n\0"));
		sp5K_printStr(&cmd_printfBuff);
		return;
	}

	// MCP
	// write mcp 0|1|2 addr value
	if (!strcmp_P( strupr(argv[1]), PSTR("MCP\0"))) {
		devId = atoi(argv[2]);
		address = atoi(argv[3]);
		regValue = strtol(argv[4],NULL,2);

		if ( devId == 0 ) { mcp_address = MCP_ADDR0; }
		if ( devId == 1 ) { mcp_address = MCP_ADDR1; }
		if ( devId == 2 ) { mcp_address = MCP_ADDR2; }

		retS = MCP_write( address,  mcp_address, 1, &regValue);
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// EEPROM
	if (!strcmp_P( strupr(argv[1]), PSTR("EE\0"))) {
		address = atoi(argv[2]);
		p = argv[3];
		while (*p != NULL) {
			p++;
			length++;
			if (length > CMDLINE_BUFFERSIZE )
				break;
		}

		retS = EE_write( address, length, argv[3] );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// LED
	// write led 0|1
	if (!strcmp_P( strupr(argv[1]), PSTR("LED\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setLed( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// gprsPWR
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRSPWR\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setGprsPwr( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// gprsSW
	if (!strcmp_P( strupr(argv[1]), PSTR("GPRSSW\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setGprsSw( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// termPWR
	if (!strcmp_P( strupr(argv[1]), PSTR("TERMPWR\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setTermPwr( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// sensorPWR
	if (!strcmp_P( strupr(argv[1]), PSTR("SENSORPWR\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setSensorPwr( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// analogPWR
	if (!strcmp_P( strupr(argv[1]), PSTR("ANALOGPWR\0"))) {
		value = atoi(argv[2]);
		retS = MCP_setAnalogPwr( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// SMS
	if (!strcmp_P(argv[1], PSTR("SMS\0"))) {
		nro = argv[2];
		msg = cmdlineGetArgStr(3);

		rprintfStr(GPRS_UART, "AT+CMGS=\"+\0" );
		rprintfStr(GPRS_UART, nro );
		rprintfStr(GPRS_UART, "\r\0");

		// Espero el prompt > para enviar el mensaje.
		timeout = 10;
		xUartQueueFlush(GPRS_UART);
		while (timeout > 0) {
			vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ));
			if (strstr( &xUart0RxedCharsBuffer.buffer, ">") != NULL) {
				break;
			}
			timeout--;
		}
		rprintfStr(GPRS_UART, msg );
		rprintfStr(GPRS_UART, "\r");

		rprintfStr(CMD_UART, &xUart0RxedCharsBuffer.buffer );
		rprintfCRLF(CMD_UART);

		rprintfStr(GPRS_UART, "\032");

		return;
	}

	// ATCMD
	if (!strcmp_P(argv[1], PSTR("ATCMD\0"))) {
		msg = argv[2];
		timeout = atoi(argv[3]);
		if (timeout == 0)
			timeout = 1;

		xUartQueueFlush(GPRS_UART);
		rprintfStr(GPRS_UART, msg);
		rprintfStr(GPRS_UART, "\r\0");

		rprintfStr(CMD_UART, "sent>\0");
		rprintfStr(CMD_UART, msg);
		rprintfCRLF(CMD_UART);

		while (timeout > 0) {
			vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ));
			timeout--;
		}

		rprintfStr(CMD_UART, &xUart0RxedCharsBuffer.buffer );
		rprintfCRLF(CMD_UART);

		return;
	}

#ifdef CHANNELS_3

	// Valves: Phase
	// write ph A1 1
	if (!strcmp_P( strupr(argv[1]), PSTR("PH\0"))) {

		phase = toupper( argv[2][0]  );
		//(s0)++;
		chip = argv[2][1] - 0x30;
		value = atoi(argv[3]);

		retS = setValvesPhase(phase,chip, value);
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// Valves: Enable
	// write en A1 1
	if (!strcmp_P( strupr(argv[1]), PSTR("EN\0"))) {

		phase = toupper( argv[2][0]  );
		//(s0)++;
		chip = argv[2][1] - 0x30;
		value = atoi(argv[3]);

		retS = setValvesEnable(phase,chip, value);
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// Valves: Reset
	if (!strcmp_P( strupr(argv[1]), PSTR("VRESET\0"))) {

		value = atoi(argv[2]);

		retS = setValvesReset( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// Valves: Sleep
	if (!strcmp_P( strupr(argv[1]), PSTR("VSLEEP\0"))) {

		value = atoi(argv[2]);

		retS = setValvesSleep( value );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;
	}

	// Valves: Pulse [A/B][1/2] {+|-} {ms}
	if (!strcmp_P( strupr(argv[1]), PSTR("VPULSE\0"))) {

		phase = toupper( argv[2][0]  );
		chip = argv[2][1] - 0x30;
		value = toupper( argv[3][0]  );
		sleepTime = atol(argv[4]);

		if ( sleepTime > 5000 ) {
			sleepTime = 5000;
		}

		retS = setValvesPulse( phase, chip, value, sleepTime );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;

	}

	// Valves: OPEN
	if (!strcmp_P( strupr(argv[1]), PSTR("OPEN\0"))) {

		phase = toupper( argv[2][0]  );
		chip = argv[2][1] - 0x30;

		retS = setValvesOpen( phase, chip );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;

	}

	// Valves: CLOSE
	if (!strcmp_P( strupr(argv[1]), PSTR("CLOSE\0"))) {

		phase = toupper( argv[2][0]  );
		chip = argv[2][1] - 0x30;

		retS = setValvesClose( phase, chip );
		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;

	}

	// Valves: Consigna
	if (!strcmp_P( strupr(argv[1]), PSTR("CONSIGNA\0"))) {

		if (!strcmp_P( strupr(argv[2]), PSTR("DIA\0"))) {
			retS = setConsignaDia();
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("NOCHE\0"))) {
			retS = setConsignaNoche();
		}

		if ( retS ) {
			pv_snprintfP_OK();
		} else {
			pv_snprintfP_ERR();
		}
		return;

	}
#endif

	// CMD NOT FOUND
	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
		rprintfProgStrM(CMD_UART, "ERROR\r\n");
		rprintfProgStrM(CMD_UART, "CMD NOT DEFINED\r\n");
		xSemaphoreGive( sem_CmdUart );
	}
	return;

}
//------------------------------------------------------------------------------------
void sp5K_stackFunction(void)
{
	// Funcion de debug que muestra el estado de los stacks de c/tarea.

	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
		vt100ClearScreen(CMD_UART);
		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Control: [%lu | %d]\r\n\0"), tkControl_uxHighWaterMark, tkControl_STACK_SIZE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Digital: [%lu | %d]\r\n\0"), tkDigitalIn_uxHighWaterMark, tkDigitalIn_STACK_SIZE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Data: [%lu | %d]\r\n\0"), tkData_uxHighWaterMark, tkData_STACK_SIZE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Cmd: [%lu | %d]\r\n\0"), tkCmd_uxHighWaterMark, tkCmd_STACK_SIZE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		snprintf_P( cmd_printfBuff,CHAR128,PSTR("Gprs: [%lu | %d]\r\n\0"),tkGprs_uxHighWaterMark, tkGprs_STACK_SIZE );
		rprintfStr(CMD_UART, cmd_printfBuff );

		xSemaphoreGive( sem_CmdUart );
	}


}
//------------------------------------------------------------------------------------
void sp5K_helpFunction(void)
{

	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {

		rprintfProgStrM(CMD_UART, "Spymovil \0");
		rprintfProgStrM(CMD_UART, SP5K_MODELO);
		rprintfChar(CMD_UART, ' ');
		rprintfProgStrM(CMD_UART, SP5K_VERSION);
		rprintfChar(CMD_UART, ' ');
		rprintfProgStrM(CMD_UART, EE_TYPE);
		rprintfChar(CMD_UART, '-');
		rprintfProgStrM(CMD_UART, FTYPE);
		rprintfChar(CMD_UART, ' ');
		rprintfProgStrM(CMD_UART, SP5K_REV);
		rprintfChar(CMD_UART, ' ');
		rprintfProgStrM(CMD_UART, SP5K_DATE);
		rprintfCRLF(CMD_UART);

		rprintfProgStrM(CMD_UART, "Available commands are:\r\n");
		rprintfProgStrM(CMD_UART, "-cls\r\n\0");
		rprintfProgStrM(CMD_UART, "-help\r\n\0");
		rprintfProgStrM(CMD_UART, "-reset {default ,memory}\r\n\0");
		rprintfProgStrM(CMD_UART, "-status\r\n\0");

		rprintfProgStrM(CMD_UART, "-write rtc YYMMDDhhmm \r\n");
		rprintfProgStrM(CMD_UART, "       param { wrkmode [service | monitor {sqe|frame}], pwrmode [continuo|discreto] } \r\n");
		rprintfProgStrM(CMD_UART, "              timerpoll, timerdial, dlgid \r\n");
		rprintfProgStrM(CMD_UART, "              debuglevel +/-{none,i2c,bd,data,gprs,digital,all} \r\n");
		rprintfProgStrM(CMD_UART, "              loglevel (none, info, all)\r\n");
		rprintfProgStrM(CMD_UART, "              imin|imax|mmin|mmax|magp|offmmin|offmmax X val\r\n");
		rprintfProgStrM(CMD_UART, "              aname, dname\r\n");
		rprintfProgStrM(CMD_UART, "              apn, port, ip, script, passwd\r\n");
#ifdef CHANNELS_3
		rprintfProgStrM(CMD_UART, "              consigna [none|doble], [dia|noche] {hh:mm} \r\n");
#endif
		rprintfProgStrM(CMD_UART, "              save\r\n");

		if ( systemVars.wrkMode == WK_SERVICE) {
			rprintfProgStrM(CMD_UART, "       mcp devId regAddr {xxxxxxxx}\r\n");
			rprintfProgStrM(CMD_UART, "       ee addr string\r\n");
			rprintfProgStrM(CMD_UART, "       led {0|1},gprspwr {0|1},gprssw {0|1},termpwr {0|1},sensorpwr {0|1},analogpwr {0|1}\r\n");
			rprintfProgStrM(CMD_UART, "       sms {nro,msg}, atcmd {atcmd,timeout}\r\n");
#ifdef CHANNELS_3
			rprintfProgStrM(CMD_UART, "       ph [A/B][1/2] {0|1}, en [A/B][1/2] {0|1}, vreset {0|1} vsleep {0|1}\r\n");
			rprintfProgStrM(CMD_UART, "       vpulse [A/B][1/2] {+|-} {ms}\r\n");
			rprintfProgStrM(CMD_UART, "       open [A/B][1/2], close [A/B][1/2], consigna [dia|noche] \r\n");
#endif
		}

		rprintfProgStrM(CMD_UART, "-read rtc\r\n");
		rprintfProgStrM(CMD_UART, "      mcp devId regAddr\r\n");
		rprintfProgStrM(CMD_UART, "      dcd,ri,termsw,din0,din1\r\n");

		if ( systemVars.wrkMode == WK_SERVICE) {

			rprintfProgStrM(CMD_UART, "      adc{channel}, frame\r\n");
			rprintfProgStrM(CMD_UART, "      ee addr lenght\r\n");
			rprintfProgStrM(CMD_UART, "      memory \r\n");
			rprintfCRLF(CMD_UART);
		}

		xSemaphoreGive( sem_CmdUart );
	}
}
//------------------------------------------------------------------------------------

void sp5K_resetFunction(void)
{

systemVarsType sVars;

	makeargv();

	if (!strcmp_P(strupr(argv[1]), PSTR("DEFAULT\0"))) {
		// Cargo los parametros caracteristicos a OSE ??
		loadDefaults(&sVars);
		// Guardo los nuevos parametros en la EE para ser leidos al arranque
		saveSystemParamsInEE( &sVars );
	}

	// Memory
	if (!strcmp_P(strupr(argv[1]), PSTR("MEMORY\0"))) {
//		vTaskSuspend( h_tkGprs );
		vTaskSuspend( h_tkData );
		vTaskSuspend( h_tkDigitalIn );
		vTaskSuspend( h_tkControl );
		wdt_disable();
		// Por las dudas
		xSemaphoreGive( sem_CmdUart );
		xSemaphoreGive( sem_BD );
		xSemaphoreGive( sem_I2Cbus );
		BD_drop();
	}

	/* Reseteo al micro activando el watchdog */
//	cli();
	wdt_reset();
	/* Start timed equence */
//	WDTCSR |= (1<<WDCE) | (1<<WDE);
	/* Set new prescaler(time-out) value = 64K cycles (~0.5 s) */
//	WDTCSR = (1<<WDE) | (1<<WDP2) | (1<<WDP0);
//	sei();

	wdt_enable(WDTO_30MS);
	while(1) {}

}

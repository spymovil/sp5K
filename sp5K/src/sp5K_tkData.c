/*
 * sp5K_tkData.c
 *
 *  Created on: 30/07/2014
 *      Author: root
 *
 *  20141010:
 *  Modificado para tener una FSM simple, sin slots.
 *
 *
 *  Modificado para manejar el sistema de valvulas.
 *  La idea es manejar el tiempo con slots. C/u dura 15s e implica 5s de settle time
 *  al prender el sensor y 10s de poleo.
 *  De este modo, la  unidad basica de espera ( datos ) son 15, y todos los tiempos ( timePoll ) deben
 *  ajustarse a multiplos.
 *  Los slots pueden ser inactivos ( IDLE, no hacen nada), de sample ( SAMPLE, polean) o de almacenar en BD
 *  ( SAVE, guarda los datos en la BD).
 *  Utilizo entonces un contador global de slots de 16 bits.
 *  Las reglas son:
 *  - contador * 15s % timerpoll == 0 ==> SAVE slot
 *  - contador * 15s % timerSample == 0 ==> SAMPLE slot
 *  - else IDLE slot.
 *  El timerSample es una variable similar al timerPoll, local.
 *  Para nuestro caso ( no valvulas ) timerSample = timerPoll.
 *  timerSample DEBE ser divisible por timerPoll, multiplo de TIME_SLOT.
 *
 *
 */
//
#include "sp5K.h"

struct {
	double adcIn[8];
	u16 batt;
	frameDataType frame;
} dataRecord;

static char pvDATA_printfBuff[CHAR256];

static s08 fl_tkDataReloadConfigFlag;
static s08 fl_tkDataReadFrameFlag;

// Estados
typedef enum {	tkdST_INIT 		= 0,
				tkdST_DATA		= 1,
				tkdST_IDLE		= 2,
				tkdST_AWAIT01	= 3,
				tkdST_AWAIT02	= 4,
				tkdST_POLL		= 5
} t_tkData_state;

// Eventos
typedef enum {
	dEV00 = 0, 	// Init
	dEV01 = 1,	// ReloadConfig
	dEV02 = 2,	// wrkMode = NORMAL
	dEV03 = 3,	// wrkMode = MONITOR
	dEV04 = 4,	// wrkMode = IDLE
	dEV05 = 5,	// wrkMode = SERVICE
	dEV06 = 6,  // ReadFrame_flag = TRUE
	dEV07 = 7,	// timerPoll_expired
	dEV08 = 8,	// timer1_expired
	dEV09 = 9	// nroPoleos NOT 0

} t_tkData_eventos;

#define dEVENT_COUNT		10

static s08 dEventos[dEVENT_COUNT];

// transiciones
static int trD00_fn(void);
static int trD01_fn(void);
static int trD02_fn(void);
static int trD03_fn(void);
static int trD04_fn(void);
static int trD05_fn(void);
static int trD06_fn(void);
static int trD07_fn(void);
static int trD08_fn(void);
static int trD09_fn(void);
static int dacRELOAD_CONFIG_fn(const char *s);

// Local timers
typedef struct {
	s16 timer;
	u32 tickReference;
} t_DataTimerStruct;

static t_DataTimerStruct timerPoll, timer1;

static u08 nroPoleos;

static s08 initStatus = FALSE;
static u08 tkData_state;
static s08 firstCicle = TRUE;

#define CICLOS_POLEO	3

// Funciones generales
static void DATAGetNextEvent(void);
static void pvDATA_updateTimers(void);
static void pvDATA_loadTimer( t_DataTimerStruct *sTimer, s16 value);
static void pvDATA_printExitMsg(u08 code);

/*------------------------------------------------------------------------------------*/
void tkData(void * pvParameters)
{

( void ) pvParameters;

	while (! initStatus ) {
		if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
			initStatus = systemVars.initStatus;
			xSemaphoreGive( sem_systemVars );
		}
		vTaskDelay( 10 );
	}

	vTaskDelay( (portTickType)(2000 / portTICK_RATE_MS) );

	memset( &pvDATA_printfBuff, '\0', sizeof(pvDATA_printfBuff));
	snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::Task starting...\r\n\0"));
	logPrintStr(LOG_NONE, pvDATA_printfBuff);

	tkData_state = tkdST_INIT;
	initStatus = TRUE;
	//

	for( ;; )
	{

		// Para medir el uso del stack
		tkData_uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );
		clearWdg(WDG_DATA);
		pvDATA_updateTimers();
		DATAGetNextEvent();

		// El manejar la FSM con un switch por estado y no por transicion me permite
		// priorizar las transiciones.
		// Luego de c/transicion debe venir un break así solo evaluo de a 1 transicion por loop.
		//
		switch ( tkData_state ) {
		case tkdST_INIT:
			tkData_state = trD00_fn();	// TR00
			break;
		case tkdST_DATA:
			if ( dEventos[dEV01] ) { tkData_state = dacRELOAD_CONFIG_fn("tkdST_DATA"); break; }
			if ( dEventos[dEV02] == TRUE  ) { tkData_state = trD03_fn();break; } 	// TR03	, Modo NORMAL
			if ( dEventos[dEV03] == TRUE  ) { tkData_state = trD04_fn();break; } 	// TR04	, Modo MONITOR
			if ( dEventos[dEV04] == TRUE  ) { tkData_state = trD02_fn();break; } 	// TR02	, Modo IDLE
			if ( dEventos[dEV05] == TRUE  ) { tkData_state = trD01_fn();break; } 	// TR01	, Modo SERVICE
			break;
		case tkdST_IDLE:
			if ( dEventos[dEV01] ) { tkData_state = dacRELOAD_CONFIG_fn("tkdST_IDLE"); break; }
			if ( dEventos[dEV06] == TRUE  ) { tkData_state = trD09_fn(); break; } 	// TR09
			break;
		case tkdST_AWAIT01:
			if ( dEventos[dEV01] ) { tkData_state = dacRELOAD_CONFIG_fn("tkdST_AWAIT01"); break; }
			if ( dEventos[dEV07] == TRUE  ) { tkData_state = trD05_fn(); break; } 	// TR05
			break;
		case tkdST_AWAIT02:
			if ( dEventos[dEV01] ) { tkData_state = dacRELOAD_CONFIG_fn("tkdST_AWAIT02"); break; }
			if ( dEventos[dEV08] == TRUE  ) { tkData_state = trD06_fn(); break; } 	// TR06
			break;
		case tkdST_POLL:
			if ( dEventos[dEV01] ) { tkData_state = dacRELOAD_CONFIG_fn("tkdST_POLL"); break; }
			if ( dEventos[dEV09] == TRUE  ) {
				tkData_state = trD07_fn();  // TR07
			} else {
				tkData_state = trD08_fn();  // TR08
			}
			break;
		default:
			snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::ERROR state NOT DEFINED\r\n\0"));
			logPrintStr(LOG_NONE, pvDATA_printfBuff);
			tkData_state = tkdST_INIT;
			initStatus = TRUE;
			break;

		}
	}

}
/*------------------------------------------------------------------------------------*/
static void DATAGetNextEvent(void)
{
// Evaluo todas las condiciones que generan los eventos que disparan las transiciones.
// Tenemos un array de eventos y todos se evaluan.

u08 i;

	// Inicializo la lista de eventos.
	for ( i=0; i < dEVENT_COUNT; i++ ) {
		dEventos[i] = FALSE;
	}


	// Evaluo los eventos
	// EV00: initStatus = TRUE
	if ( initStatus == TRUE ) { dEventos[dEV00] = TRUE; }
	// EV01: ReloadConfig
	if ( fl_tkDataReloadConfigFlag == TRUE ) { dEventos[dEV01] = TRUE; }
	// EV02: wrkMode = NORMAL
	if ( systemVars.wrkMode == WK_NORMAL ) { dEventos[dEV02] = TRUE; }
	// EV03:  wrkMode = MONITOR
	if ( systemVars.wrkMode == WK_MONITOR ) { dEventos[dEV03] = TRUE; }
	// EV04:  wrkMode = IDLE
	if ( systemVars.wrkMode == WK_IDLE ) { dEventos[dEV04] = TRUE; }
	// EV05:  wrkMode = SERVICE
	if ( systemVars.wrkMode == WK_SERVICE ) { dEventos[dEV05] = TRUE; }
	// EV06: ReadFrame_flag = TRUE
	if ( fl_tkDataReadFrameFlag == TRUE ) { dEventos[dEV06] = TRUE; }
	// EV07: timerPoll = 0
	if ( timerPoll.timer == 0 ) { dEventos[dEV07] = TRUE; }
	// EV08: timer1 = 0
	if ( timer1.timer == 0 ) { dEventos[dEV08] = TRUE; }
	// EV09: nroPoleos NOT 0
	if ( nroPoleos > 0 ) { dEventos[dEV09] = TRUE; }

}
/*------------------------------------------------------------------------------------*/
static int dacRELOAD_CONFIG_fn(const char *s)
{

	snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::RCONF:%s\r\n\0"),s);
	sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff);
	fl_tkDataReloadConfigFlag = FALSE;
	fl_tkDataReadFrameFlag = FALSE;

	return(tkdST_INIT);

}
/*------------------------------------------------------------------------------------*/
static int trD00_fn(void)
{
	// Evento inicial. Solo salta al primer estado operativo.
	// Inicializo el sistema aqui
	// tkdST_INIT->tkdST_DATA

	initStatus = FALSE;
	fl_tkDataReloadConfigFlag = FALSE;
	fl_tkDataReadFrameFlag = FALSE;
	firstCicle = TRUE;

	if ( systemVars.pwrMode == PWR_CONTINUO ) {
		// Prender los sensores.
		MCP_setSensorPwr( 1 );
		MCP_setAnalogPwr( 1 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD00->pwron sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff);
	} else {
		// Apagar los sensores.
		MCP_setSensorPwr( 0 );
		MCP_setAnalogPwr( 0 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD00->pwroff sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff);
	}

	pvDATA_printExitMsg(0);
	return(tkdST_DATA);
}
/*------------------------------------------------------------------------------------*/
static int trD01_fn(void)
{
	// tkdST_DATA->tkdST_IDLE

	// Solo apago los sensores ( modo discreto )
	if ( systemVars.pwrMode == PWR_DISCRETO ) {
		MCP_setSensorPwr( 0 );
		MCP_setAnalogPwr( 0 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD01->pwroff sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );
	}

	pvDATA_printExitMsg(1);
	return(tkdST_IDLE);
}
/*------------------------------------------------------------------------------------*/
static int trD02_fn(void)
{
	// tkdST_DATA->tkdST_IDLE

	// Solo apago los sensores ( modo discreto )
	if ( systemVars.pwrMode == PWR_DISCRETO ) {
		MCP_setSensorPwr( 0 );
		MCP_setAnalogPwr( 0 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD02->pwroff sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );
	}

	pvDATA_printExitMsg(2);
	return(tkdST_IDLE);
}
/*------------------------------------------------------------------------------------*/
static int trD03_fn(void)
{
	// tkdST_DATA->tkdST_AWAIT01 (wrkMode NORMAL)

u16 tPoll;

	// Init timerPoll
	if ( firstCicle == TRUE ) {
		tPoll = 10;
	} else {
		tPoll = systemVars.timerPoll - (CICLOS_POLEO * 1.5) - 5;
	}

	pvDATA_loadTimer( &timerPoll, tPoll);

	snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD03 (%d)\r\n\0"), tPoll);
	sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );

	pvDATA_printExitMsg(3);
	return(tkdST_AWAIT01);
}
/*------------------------------------------------------------------------------------*/
static int trD04_fn(void)
{
	// tkdST_DATA->tkdST_AWAIT01 ( wrkMode MONITOR )

	// Init timerPoll
	pvDATA_loadTimer( &timerPoll, 1);

	pvDATA_printExitMsg(4);
	return(tkdST_AWAIT01);
}
/*------------------------------------------------------------------------------------*/
static int trD05_fn(void)
{
	// tkdST_AWAIT01->tkdST_AWAIT02

	// Prender los sensores.
	if ( systemVars.pwrMode == PWR_DISCRETO ) {
		MCP_setSensorPwr( 1 );
		MCP_setAnalogPwr( 1 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD05->pwron sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff);
	}

	// Init timer1 ( pwr settle time )
	pvDATA_loadTimer( &timer1, 5);

	pvDATA_printExitMsg(5);
	return(tkdST_AWAIT02);

}
/*------------------------------------------------------------------------------------*/
static int trD06_fn(void)
{
	// tkdST_AWAIT02->tkdST_POLL

u08 channel;

	// Init Nro.Poleos
	nroPoleos = CICLOS_POLEO;

	// Init Data Structure
	for ( channel = 0; channel < 8; channel++) {
		dataRecord.adcIn[channel] = 0;
	}
	dataRecord.batt = 0;

	pvDATA_printExitMsg(6);
	return(tkdST_POLL);
}
/*------------------------------------------------------------------------------------*/
static int trD07_fn(void)
{
	// tkdST_POLL->tkdST_POLL
	// Poleo

	u08 channel;
	u16 adcRetValue;
	s08 retS;

	if ( nroPoleos > 0 ) {
		nroPoleos--;
	}

	// Dummy convert para prender el ADC ( estabiliza la medida).
	// Esta espera de 1.5s es util entre ciclos de poleo.
	retS = ADS7828_convert( 0, &adcRetValue );
	vTaskDelay( (portTickType)(1500 / portTICK_RATE_MS) );
	//
	for ( channel = 0; channel < 8; channel++) {
		retS = ADS7828_convert( channel, &adcRetValue );
		dataRecord.adcIn[channel] += adcRetValue;

		memset( &pvDATA_printfBuff, '\0', sizeof(pvDATA_printfBuff));
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD07->(poll):ciclo=%d, arcRet=%.0f, adcIn[%d]=%.1f\r\n\0"), nroPoleos ,(float)adcRetValue, channel, dataRecord.adcIn[channel]);
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );
		taskYIELD();
	}

	pvDATA_printExitMsg(7);
	return(tkdST_POLL);

}
/*------------------------------------------------------------------------------------*/
static int trD08_fn(void)
{
	// tkdST_POLL->tkdST_DATA

double I,M,D;
u08 channel;
s08 retS = FALSE;
u08 pos = 0;
s08 saveFlag = TRUE;

	// Apago sensores( modo discreto )
	if ( systemVars.pwrMode == PWR_DISCRETO ) {
		MCP_setSensorPwr( 0 );
		MCP_setAnalogPwr( 0 );
		snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD08->pwroff sensors\r\n\0"));
		sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );
	}

	fl_tkDataReadFrameFlag = FALSE;

	// Promedio:
	for ( channel = 0; channel < 8; channel++) {
		dataRecord.adcIn[channel] /= CICLOS_POLEO;
		memset( &pvDATA_printfBuff, '\0', sizeof(pvDATA_printfBuff));
		snprintf_P( pvDATA_printfBuff,CHAR64,PSTR("tkData::trD08->AvgAdcIn[%d]=%.1f\r\n\0"),channel, dataRecord.adcIn[channel]);
		sp5K_debugPrintStr( D_DATA,  pvDATA_printfBuff );
	}

#ifdef CHANNELS_3
	// Asignacion de canales logicos a fisicos.
	dataRecord.batt = dataRecord.adcIn[1];		// Bateria ( ADC1)
	dataRecord.adcIn[0] = dataRecord.adcIn[3];	// Canal 0 ( ADC3)
	dataRecord.adcIn[1] = dataRecord.adcIn[5];	// Canal 1 ( ADC5)
	dataRecord.adcIn[2] = dataRecord.adcIn[7];	// Canal 2 ( ADC7)
#endif

#ifdef CHANNELS_8
	// Asignacion de canales logicos a fisicos.
/*	dataRecord.batt = 0;						// Bateria
	dataRecord.adcIn[0] = dataRecord.adcIn[0];	// Canal 0 ( J9.0-ADC0)
	dataRecord.adcIn[1] = dataRecord.adcIn[1];	// Canal 1 ( J9.1-ADC1)
	dataRecord.adcIn[2] = dataRecord.adcIn[2];	// Canal 2 ( J9.2-ADC2)
	dataRecord.adcIn[3] = dataRecord.adcIn[3];	// Canal 3 ( J9.3-ADC3)

	dataRecord.adcIn[4] = dataRecord.adcIn[4];	// Canal 4 ( J5-ADC4)
	dataRecord.adcIn[5] = dataRecord.adcIn[5];	// Canal 5 ( J6-ADC5)
	dataRecord.adcIn[6] = dataRecord.adcIn[6];	// Canal 6 ( J7-ADC6)
	dataRecord.adcIn[7] = dataRecord.adcIn[7];	// Canal 7 ( J8-ADC7)
*/
#endif

	// Make Frame
	// Paso1: Inserto el timeStamp.
	rtcGetTime(&dataRecord.frame.rtc);
	//
	// Paso2: Inserto las medidas digitales
	readDigitalCounters( &dataRecord.frame.din0_pCount, &dataRecord.frame.din1_pCount, TRUE );
	// Convierto los pulsos a los valores de la magnitud.
	dataRecord.frame.din0_pCount *= systemVars.magPP[0];
	dataRecord.frame.din1_pCount *= systemVars.magPP[1];
	//
	// Paso3: Calculo las magnitudes.
#ifdef CHANNELS_3
	dataRecord.frame.batt = (double) (15 * dataRecord.batt) / 4096;
#endif
	for ( channel = 0; channel < NRO_CHANNELS; channel++) {
		// Calculo la corriente
		I = (double) dataRecord.adcIn[channel] * 20 / 4096;
		// Calculo la pendiente
		M = 0;
		D = ( systemVars.Imax[channel] - systemVars.Imin[channel] );
		if ( D != 0 ) {
			M = ( (systemVars.Mmax[channel] + systemVars.offmmax[channel]) - ( systemVars.Mmin[channel] + systemVars.offmmin[channel]) ) / D;
		}
		dataRecord.frame.analogIn[channel] = M * ( I - systemVars.Imin[channel] ) + systemVars.Mmin[channel] + systemVars.offmmin[channel];
	}

	// Print frame
	memset( &pvDATA_printfBuff, '\0', CHAR256);
	pos = snprintf_P( pvDATA_printfBuff, CHAR256, PSTR("tkData::{" ));
	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256,PSTR( "%04d%02d%02d,"),dataRecord.frame.rtc.year,dataRecord.frame.rtc.month, dataRecord.frame.rtc.day );
	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256, PSTR("%02d%02d%02d,"),dataRecord.frame.rtc.hour,dataRecord.frame.rtc.min, dataRecord.frame.rtc.sec );

	// Valores analogicos
	for ( channel = 0; channel < NRO_CHANNELS; channel++) {
		pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256, PSTR("%s=%.2f,"),systemVars.aChName[channel],dataRecord.frame.analogIn[channel] );
	}

	// Valores digitales.
	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256, PSTR("%sP=%.2f,"), systemVars.dChName[0], dataRecord.frame.din0_pCount);
	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256, PSTR("%sP=%.2f"), systemVars.dChName[1],dataRecord.frame.din1_pCount );

#ifdef CHANNELS_3
	// Bateria
	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256, PSTR(",bt=%.2f"),dataRecord.frame.batt );
#endif;

	pos += snprintf_P( &pvDATA_printfBuff[pos], CHAR256,PSTR("}\r\n\0") );

	// Imprimo
	switch (systemVars.wrkMode) {
	case WK_NORMAL:
		logPrintStr(LOG_INFO, pvDATA_printfBuff);
		break;
	case WK_SERVICE:
	case WK_MONITOR:
		logPrintStr(LOG_NONE, pvDATA_printfBuff);
		break;
	}

	// SAVE FRAME
	if ( firstCicle == TRUE ) {
		// En el primer ciclo no guardo los datos.
		firstCicle = FALSE;
		saveFlag = FALSE;
	}

	if ( systemVars.wrkMode != WK_NORMAL ) {
		// Si no estoy en modo normal no guardo los datos.
		saveFlag = FALSE;
	}

	if ( saveFlag == TRUE ) {
		retS = BD_put( &dataRecord.frame);
		if (!retS) {
			snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD08->BDSaveFrame ERROR.\r\n\0"));
			logPrintStr(LOG_NONE, pvDATA_printfBuff);
		} else {
			snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::trD08->BDSaveFrame OK:  wrPtr=%d, rdPtr=%d, usedRec=%d, freeRecs=%d\r\n\0"), BD_getWRptr(), BD_getRDptr(), BD_getRcsUsed(), BD_getRcsFree() );
			//sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff );
			logPrintStr(LOG_INFO, pvDATA_printfBuff);
		}
	}

	pvDATA_printExitMsg(8);
	return(tkdST_DATA);

}
/*------------------------------------------------------------------------------------*/
static int trD09_fn(void)
{
	// tkdST_IDLE->tkdST_AWAIT01

	// Init timerPoll
	pvDATA_loadTimer( &timerPoll, 1);

	pvDATA_printExitMsg(9);
	return(tkdST_AWAIT01);
}
/*------------------------------------------------------------------------------------*/
static void pvDATA_updateTimers(void)
{

u32 tick;

	tick = getSystemTicks();

	if ( timerPoll.timer > 0) {
		if ( (tick - timerPoll.tickReference) >= configTICK_RATE_HZ ) {
			timerPoll.tickReference += configTICK_RATE_HZ;
			timerPoll.timer--;
		}
	}

	if ( timer1.timer > 0) {
		if ( (tick - timer1.tickReference) >= configTICK_RATE_HZ ) {
			timer1.tickReference += configTICK_RATE_HZ;
			timer1.timer--;
		}
	}
}
/*------------------------------------------------------------------------------------*/
u16 getTimeToNextPoll(void)
{
	// Funcion publica para acceder al tiempo restante para el proximo poleo.
	return ( timerPoll.timer);
}
/*------------------------------------------------------------------------------------*/
void SIGNAL_tkDataRefresh(void)
{
	fl_tkDataReloadConfigFlag = TRUE;
}
/*------------------------------------------------------------------------------------*/
void SIGNAL_tkDataReadFrame(void)
{
	fl_tkDataReadFrameFlag = TRUE;
}
/*------------------------------------------------------------------------------------*/
void getDataFrame( frameDataType* rdRec )
{
	// Devuelve el frame con los ultimos datos validos.
	memcpy( rdRec, &dataRecord.frame, sizeof(frameDataType));
}
/*------------------------------------------------------------------------------------*/
static void pvDATA_loadTimer( t_DataTimerStruct *sTimer, s16 value)
{
	sTimer->timer = value;
	sTimer->tickReference = getSystemTicks();
}
/*------------------------------------------------------------------------------------*/
static void pvDATA_printExitMsg(u08 code)
{
	memset( &pvDATA_printfBuff, '\0', sizeof(pvDATA_printfBuff));
	snprintf_P( pvDATA_printfBuff,CHAR256,PSTR("tkData::TR%02d:\r\n\0"), code);
	sp5K_debugPrintStr( D_DATA, pvDATA_printfBuff);
}
/*------------------------------------------------------------------------------------*/
void getStatusDataSM( u08 *state)
{
	*state = tkData_state;
}
/*------------------------------------------------------------------------------------*/



/*
 * sp5K_tkGprs2.c
 *
 *  Created on: 21/05/2014
 *      Author: root
 *
 *  Creo una maquina de estados con 4 estados. c/u es una nueva maquina de estados.
 *  Los eventos los evaluo de acuerdo al estado.
 *
 *  V2.0.10 @ 2014-11-28
 *  Monitoreo del SQE.
 *  - Agrego un nuevo valor al wrkMode, MONITOR.
 *    Al pasar a este modo, tkData es lo mismo que service, pero tkGprs NO.
 *
 *
 */
#include "sp5K.h"

char pvGPRS_printfBuff[CHAR384];

s08 fl_tkGprsReloadConfigFlag;
static s08 fl_gprsAllowsSleep = FALSE;

// Estados
typedef enum {// gST_INIT = 0,
				gST_OFF = 1,
				gST_ONoffline = 2,
				gST_ONInit = 3,
				gST_ONData = 4
} t_tkGprs_state;

// Subestados.
typedef enum {
				// Estado OFF
				gSST_OFF_Entry = 0,
	            gSST_OFF_Init = 1,
	            gSST_OFF_Idle = 2,
	            gSST_OFF_Standby = 3,
	            gSST_OFF_await01 = 4,
	            gSST_OFF_await02 = 5,
	            gSST_OFF_await03 = 6,
	            gSST_OFF_dummy01 = 7,
	            gSST_OFF_dummy02 = 8,
	            gSST_OFF_dummy03 = 9,

	            // Estado ONoffline
	            gSST_ONoffline_Entry = 10,
	            gSST_ONoffline_cpin = 11,
	            gSST_ONoffline_await01 = 12,
	            gSST_ONoffline_dummy01 = 13,
	            gSST_ONoffline_net = 14,
	            gSST_ONoffline_dummy02 = 15,
	            gSST_ONoffline_dummy03 = 16,
	            gSST_ONoffline_sqe01 = 17,
	            gSST_ONoffline_sqe02 = 18,
	            gSST_ONoffline_sqe03 = 19,
	            gSST_ONoffline_sqe04 = 20,
	            gSST_ONoffline_apn = 21,
	            gSST_ONoffline_ip = 22,
	            gSST_ONoffline_dummy04 = 23,
	            gSST_ONoffline_dummy05 = 24,
	            gSST_ONoffline_dummy06 = 25,

	            // Estado ONInit
	            gSST_ONinit_Entry = 26,
	            gSST_ONinit_socket = 27,
	            gSST_ONinit_dummyS01 = 28,
	            gSST_ONinit_dummyS02 = 29,
	            gSST_ONinit_dummyS03 = 30,
	            gSST_ONinit_initFrame = 31,
	            gSST_ONinit_dummyF01 = 32,
	            gSST_ONinit_dummyF02 = 33,
	            gSST_ONinit_dummyF03 = 34,
	            gSST_ONinit_dummyF04 = 35,
	            gSST_ONinit_dummyF05 = 36,
	            gSST_ONinit_await01 = 37,

	            // Estado ONData
	            gSST_ONdata_Entry = 38,
	            gSST_ONdata_data = 39,
	            gSST_ONdata_dummy01 = 40,
	            gSST_ONdata_dummy02 = 41,
	            gSST_ONdata_await01 = 42,
	            gSST_ONdata_socket = 43,
	            gSST_ONdata_dummyS01 = 44,
	            gSST_ONdata_dummyS02 = 45,
	            gSST_ONdata_dummyS03 = 46,
	            gSST_ONdata_frame = 47,
	            gSST_ONdata_dummyF01 = 48,
	            gSST_ONdata_dummyF02 = 49,
	            gSST_ONdata_dummyF03 = 50,
	            gSST_ONdata_dummyF04 = 51,
	            gSST_ONdata_dummyF05 = 52,
	            gSST_ONdata_await02 = 53

} t_tkGprs_subState;

// Eventos
typedef enum {

	gEV00 = 0, 	// Init
	gEV01 = 1,	// ReloadConfig
	gEV02 = 2,	// (wrkMode == NORMAL) || (wrkMode == MONITOR )
	gEV03 = 3,	// timer1 expired ( timer1 == 0 )
	gEV04 = 4,	// timerDial expired ( timerDial == 0 )
	gEV05 = 5,	// GPRS ATrsp == OK
	gEV06 = 6,	// SWtryes_NOT_0
	gEV07 = 7,	// HWtryes_NOT_0

	gEV08 = 8,	// GPRS CPINrsp == READY
	gEV09 = 9,	// GPRS CREGrsp == +CREG 0,1
	gEV10 = 10, // GPRS E2IPArsp == OK
	gEV11 = 11, // pTryes_NOT_0
	gEV12 = 12, // IPtryes_NOT_0
	gEV13 = 13, // tryes_NOT_0

	gEV14 = 14, // SKTryes_NOT_0
	gEV15 = 15, // socket_IS_OPEN
	gEV16 = 16, // INITFRAMEStryes_NOT_0
	gEV17 = 17, // Server INITrsp_OK

	gEV18 = 18, // BDempty ? ( BD_getRcsToTx() == 0 )
	gEV19 = 19, // pwrMode == CONT
	gEV20 = 20,	// BULKtres == MAX
	gEV21 = 21,	// serverResponse == RX_OK
	gEV22 = 22,	// f_sendTail == TRUE

	gEV23 = 23, // (wrkMode == MONITOR)


} t_tkGprs_eventos;

#define gEVENT_COUNT		24

s08 gEventos[gEVENT_COUNT];

typedef enum { RSP_NONE = 0,
	             RSP_OK = 1,
	             RSP_READY = 2,
	             RSP_CREG = 3,
	             RSP_APN = 4,
	             RSP_CONNECT = 5,
	             RSP_HTML = 6,
	             RSP_INIT = 7,
	             RSP_RXOK = 8,
	             RSP_IPOK = 9

} t_tkGprs_responses;

t_tkGprs_responses GPRSrsp;

// Acciones
static int gacRELOAD_CONFIG_fn(const char *s);

// Estado OFF
static int gac00_fn(void);
static int gac01_fn(void);
static int gac02_fn(void);
static int gac03_fn(void);
static int gac04_fn(void);
static int gac05_fn(void);
static int gac06_fn(void);
static int gac07_fn(void);
static int gac08_fn(void);
static int gac09_fn(void);
static int gac10_fn(void);
static int gac11_fn(void);
static int gac12_fn(void);
static int gac13_fn(void);

// Estado ONoffline
static int gac20_fn(void);
static int gac21_fn(void);
static int gac22_fn(void);
static int gac23_fn(void);
static int gac24_fn(void);
static int gac25_fn(void);
static int gac26_fn(void);
static int gac27_fn(void);
static int gac28_fn(void);
static int gac29_fn(void);
static int gac30_fn(void);
static int gac31_fn(void);
static int gac32_fn(void);
static int gac33_fn(void);
static int gac34_fn(void);
static int gac35_fn(void);
static int gac36_fn(void);
static int gac37_fn(void);
static int gac38_fn(void);
static int gac39_fn(void);
static int gac40_fn(void);
static int gac41_fn(void);
static int gac42_fn(void);
static int gac43_fn(void);
static int gac44_fn(void);

// Estado ONInit
static int gac50_fn(void);
static int gac51_fn(void);
static int gac52_fn(void);
static int gac53_fn(void);
static int gac54_fn(void);
static int gac55_fn(void);
static int gac56_fn(void);
static int gac57_fn(void);
static int gac58_fn(void);
static int gac59_fn(void);
static int gac60_fn(void);
static int gac61_fn(void);
static int gac62_fn(void);
static int gac63_fn(void);
static int gac64_fn(void);
static int gac65_fn(void);
static int gac66_fn(void);
static int gac67_fn(void);
static int gac68_fn(void);
static int gac69_fn(void);

// Estado ONData
static int gac70_fn(void);
static int gac71_fn(void);
static int gac72_fn(void);
static int gac73_fn(void);
static int gac74_fn(void);
static int gac75_fn(void);
static int gac76_fn(void);
static int gac77_fn(void);
static int gac78_fn(void);
static int gac79_fn(void);
static int gac80_fn(void);
static int gac81_fn(void);
static int gac82_fn(void);
static int gac83_fn(void);
static int gac84_fn(void);
static int gac85_fn(void);
static int gac86_fn(void);
static int gac87_fn(void);
static int gac88_fn(void);
static int gac89_fn(void);
static int gac90_fn(void);
static int gac91_fn(void);
static int gac92_fn(void);
static int gac93_fn(void);
static int gac94_fn(void);
static int gac95_fn(void);
static int gac96_fn(void);

// Local timers
typedef struct {
	s16 timer;
	u32 tickReference;
} t_GprsTimerStruct;

t_GprsTimerStruct timer1, timerToDial;

u08 tkGprs_state, tkGprs_subState;

// Funciones generales
static void GPRSGetNextEvent(u08 state);
static void pvGPRS_updateTimers(void);
static void pvGPRS_loadTimer( t_GprsTimerStruct *sTimer, s16 value);
static s08 gprsRspIs(const char *rsp);
static u08 pv_tkGprsProcessPwrMode(void);
static u08 pv_tkGprsProcessTimerDial(void);
static u08 pv_tkGprsProcessTimerPoll(void);
static void pv_tkGprsProcessServerClock(void);
static void pv_tkGprs_openSocket(void);
static s08 qySocketIsOpen(void);
static void sendInitFrame(void);
void debugPrintModemBuffer(void);
static void  pv_tkGprs_printExitMsg(u08 code);
static u08 pv_tkGprsProcessAnalogChannels(u08 channel);
static u08 pv_tkGprsProcessDigitalChannels(u08 channel);
static u08 pv_tkGprsProcessConsignas(void);
static void pv_tkGprs_closeSocket(void);

// Prototipos de las state-machines
static void SM_off(void);
static void SM_onOffline(void);
static void SM_onInit(void);
static void SM_onData(void);

static s08 initStatus = FALSE;
static u08 tryes, pTryes;
static s08 arranqueFlag;
static t_modemStatus modemStatus;

frameDataType rdFrame;

static u32 counterCtlState;
char lbuff[CHAR128];

//-------------------------------------------------------------------------------------
#define MAXTIMEINASTATE 1800 // 3 minutos ( de a 100ms)

#define MAX_HWTRYES		3
#define MAX_SWTRYES		3
static u08 HWtryes;
static u08 SWtryes;

#define MAXTRYES		3

#define MAX_IPTRYES			3
#define MAX_LOOPTRYES		4
static u08 IPtryes;
static u08 LOOPtryes;

#define MAX_SOCKTRYES		3
#define MAX_BULKTRYES		4
#define MAX_BDRCDCOUNTER	12
static u08 SOCKTryes;
static u08 BULKtryes;
static u08 BDRCDcounter;
static s08 f_sendTail;

/*------------------------------------------------------------------------------------*/
void tkGprs(void * pvParameters)
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

	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::Task starting...\r\n\0"));
	logPrintStr(LOG_NONE, &pvGPRS_printfBuff);

	tkGprs_state = gST_OFF;
	tkGprs_subState = gSST_OFF_Entry;
	arranqueFlag = TRUE;
	counterCtlState = 30;
	fl_gprsAllowsSleep = FALSE;
	//
	for( ;; )
	{

		// Para medir el uso del stack
		tkGprs_uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		// Ejecuto cada 100ms. ( 10 veces por segundo ).
		vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );
		clearWdg(WDG_GPRS);
		pvGPRS_updateTimers();
		GPRSGetNextEvent(tkGprs_state);

		// Control del tiempo que esta activo c/estado.
		counterCtlState--;
		if ( counterCtlState == 0 ) {
			snprintf_P( pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR exceso de tiempo en un estado (%02d/%02d): \r\n\0"),tkGprs_state,tkGprs_subState);
			logPrintStr(LOG_NONE, &pvGPRS_printfBuff);

			tkGprs_state = gST_OFF;
			tkGprs_subState = gSST_OFF_Entry;
		}

		// El manejar la FSM con un switch por estado y no por transicion me permite
		// priorizar las transiciones.
		// Luego de c/transicion debe venir un break así solo evaluo de a 1 transicion por loop.
		//
		switch ( tkGprs_state ) {
		case gST_OFF:
			SM_off();
			break;
		case gST_ONoffline:
			SM_onOffline();
			break;
		case gST_ONInit:
			SM_onInit();
			break;
		case gST_ONData:
			SM_onData();
			break;
		default:
			snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR state NOT DEFINED\r\n\0"));
			logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
			tkGprs_state = gST_OFF;
			tkGprs_subState = gSST_OFF_Entry;
			break;
		}
	}
}
/*------------------------------------------------------------------------------------*/

static void GPRSGetNextEvent(u08 state)
{
// Evaluo todas las condiciones que generan los eventos que disparan las transiciones.
// Tenemos un array de eventos y todos se evaluan.
// Las condiciones que evaluo dependen del estado ya que no todas se deben dar siempre


u08 i;

	// Inicializo la lista de eventos.
	for ( i=0; i < gEVENT_COUNT; i++ ) {
		gEventos[i] = FALSE;
	}

	switch (state) {
	case gST_OFF:
		// Evaluo solo los eventos del estado OFF.
		// EV00: initStatus = TRUE
		//if ( initStatus == TRUE ) { gEventos[gEV00] = TRUE; }
		// EV01: ReloadConfig
		if ( fl_tkGprsReloadConfigFlag == TRUE ) { gEventos[gEV01] = TRUE; }
		// EV02: (wrkMode == NORMAL) || (wrkMode == MONITOR )
		if ( ( systemVars.wrkMode == WK_NORMAL ) || ( systemVars.wrkMode == WK_MONITOR ) ) { gEventos[gEV02] = TRUE; }
		// EV03: timer1 expired ( timer1 == 0 )
		if ( timer1.timer == 0 ) { gEventos[gEV03] = TRUE; }
		// EV04: timerDial expired ( timerDial == 0 )
		if ( timerToDial.timer == 0 ) { gEventos[gEV04] = TRUE; }
		// EV05: GPRS ATrsp == OK
		if ( GPRSrsp == RSP_OK ) { gEventos[gEV05] = TRUE; }
		// EV06: SWtryes_NOT_0()
		if ( SWtryes > 0 ) { gEventos[gEV06] = TRUE; }
		// EV07: HWtryes_NOT_0()
		if ( HWtryes > 0 ) { gEventos[gEV07] = TRUE; }
		break;

	case gST_ONoffline:
		// Evaluo solo los eventos del estado ONBoffline
		// EV01: ReloadConfig
		if ( fl_tkGprsReloadConfigFlag == TRUE ) { gEventos[gEV01] = TRUE; }
		// EV03: timer1 == 0
		if ( timer1.timer == 0 ) { gEventos[gEV03] = TRUE; }
		// EV08: CPIN ready
		if ( GPRSrsp == RSP_READY ) { gEventos[gEV08] = TRUE; }
		// EV09: CREG (NET available)
		if ( GPRSrsp == RSP_CREG ) { gEventos[gEV09] = TRUE; }
		// EV10: IP_OK.
		if ( GPRSrsp == RSP_IPOK ) { gEventos[gEV10] = TRUE; }
		// EV11: pTryes NOT 0
		if ( pTryes > 0 ) { gEventos[gEV11] = TRUE; }
		// EV12: IPtryes NOT 0
		if ( IPtryes > 0 ) { gEventos[gEV12] = TRUE; }
		// EV13: tryes NOT 0
		if ( tryes > 0 ) { gEventos[gEV13] = TRUE; }
		// EV23: (wrkMode == MONITOR)
		if ( systemVars.wrkMode == WK_MONITOR  ) { gEventos[gEV23] = TRUE; }
		break;

	case gST_ONInit:
		// Estado ONready
		// EV01: ReloadConfig
		if ( fl_tkGprsReloadConfigFlag == TRUE ) { gEventos[gEV01] = TRUE; }
		// EV03: timer1 == 0
		if ( timer1.timer == 0 ) { gEventos[gEV03] = TRUE; }
		// EV11: pTryes NOT 0
		if ( pTryes > 0 ) { gEventos[gEV11] = TRUE; }
		// EV14: SOCKTryes NOT 0
		//if ( SOCKTryes > 0 ) { gEventos[gEV14] = TRUE; }
		// EV15: socket == OPEN
		if ( qySocketIsOpen() ) { gEventos[gEV15] = TRUE; }
		// EV16: LOOPtryes NOT 0
		if ( LOOPtryes > 0 ) { gEventos[gEV16] = TRUE; }
		// EV17: serverResponse == INIT_OK
		if ( GPRSrsp == RSP_INIT ) { gEventos[gEV17] = TRUE; }
		break;

	case gST_ONData:
		// Estado ONData
		// EV01: ReloadConfig
		if ( fl_tkGprsReloadConfigFlag == TRUE ) { gEventos[gEV01] = TRUE; }
		// EV03: timer1 == 0
		if ( timer1.timer == 0 ) { gEventos[gEV03] = TRUE; }
		// EV11: pTryes NOT 0
		if ( pTryes > 0 ) { gEventos[gEV11] = TRUE; }
		// EV14: SOCKTryes NOT 0
		if ( SOCKTryes > 0 ) { gEventos[gEV14] = TRUE; }
		// EV15: socket == OPEN
		if ( qySocketIsOpen() ) { gEventos[gEV15] = TRUE; }
		// EV18: BD empty ?
		if ( BD_getRcsUsed() == 0 ) { gEventos[gEV18] = TRUE; }
		// EV19: pwrMode == CONT
		if ( systemVars.pwrMode == PWR_CONTINUO ) { gEventos[gEV19] = TRUE; }
		// EV20: BULKtryes NOT 0
		if ( BULKtryes > 0 ) { gEventos[gEV20] = TRUE; }
		// EV21: serverResponse == RX_OK
		if ( GPRSrsp == RSP_RXOK ) { gEventos[gEV21] = TRUE; }
		// EV22: f_sendTail == TRUE
		if ( f_sendTail == TRUE ) { gEventos[gEV22] = TRUE; }
		break;

	}

}
/*------------------------------------------------------------------------------------*/

static void SM_off(void)
{
	// Maquina de estados del estado OFF.( MODEM APAGADO)

	switch ( tkGprs_subState ) {
	case gSST_OFF_Entry:
		tkGprs_subState = gac00_fn();	// gTR00
		break;
	case gSST_OFF_Init:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_Init"); break; }
		if ( gEventos[gEV02]  ) {
			tkGprs_subState = gac03_fn(); // gTR03
		} else {
			tkGprs_subState = gac01_fn(); // gTR01
		}
		break;
	case gSST_OFF_Idle:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_Idle"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac02_fn(); break; }	// gTR02
		break;
	case gSST_OFF_Standby:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_Standby"); break; }
		if ( gEventos[gEV04] ) {	tkGprs_subState = gac04_fn(); break; }	// gTR04
		break;
	case gSST_OFF_await01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_await01"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac05_fn(); break; }	// gTR05
		break;
	case gSST_OFF_await02:
		if ( gEventos[gEV01]  ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_await02"); break; }
		if ( gEventos[gEV03]  ) {	tkGprs_subState = gac06_fn(); break; }	// gTR06
		break;
	case gSST_OFF_await03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_await03"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac07_fn(); break; }	// gTR07
		break;
	case gSST_OFF_dummy01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_dummy01"); break; }
		if ( gEventos[gEV05] ) {
			tkGprs_subState = gac08_fn(); 	// gTR08
		} else {
			tkGprs_subState = gac09_fn();	// gTR09
		}
		break;
	case gSST_OFF_dummy02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_dummy02"); break; }
		if ( gEventos[gEV06] ) {
			tkGprs_subState = gac10_fn(); // gTR10
		} else {
			tkGprs_subState = gac11_fn(); // gTR11
		}
		break;
	case gSST_OFF_dummy03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_OFF_dummy03"); break; }
		if ( gEventos[gEV07] ) {
			tkGprs_subState = gac12_fn();// gTR12
		} else {
			tkGprs_subState = gac13_fn();// gTR13
		}
		break;
	default:
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR sst_off: subState  (%d) NOT DEFINED\r\n\0"),tkGprs_subState);
		logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
		tkGprs_state = gST_OFF;
		tkGprs_subState = gSST_OFF_Init;
		break;
	}
}
/*------------------------------------------------------------------------------------*/

static void SM_onOffline(void)
{
	// Maquina de estados del estado ONoffline

	switch ( tkGprs_subState ) {
	case gSST_ONoffline_Entry:
		tkGprs_subState = gac20_fn();										// gTR20
		break;
	case gSST_ONoffline_cpin:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_Cpin"); break; }
		if ( gEventos[gEV08] ) {
			tkGprs_subState = gac21_fn(); // gTR21
		} else {
			tkGprs_subState = gac22_fn(); // gTR22
		}
		break;
	case gSST_ONoffline_await01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_await01"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac23_fn(); break; }				// gTR23
		break;
	case gSST_ONoffline_dummy01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy01"); break; }
		if ( gEventos[gEV13] ) {
			tkGprs_subState = gac24_fn(); 	// gTR24
		} else {
			tkGprs_subState = gac25_fn(); 	// gTR25
		}
		break;
	case gSST_ONoffline_net:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_net"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac26_fn(); break; }				// gTR26
		break;
	case gSST_ONoffline_dummy02:
		if ( gEventos[gEV01]  ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy02"); break; }
		if ( gEventos[gEV09]  ) {
			tkGprs_subState = gac30_fn();	// gTR30
		} else {
			tkGprs_subState = gac27_fn();	// gTR27
		}
		break;
	case gSST_ONoffline_dummy03:
		if ( gEventos[gEV01]  ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy03"); break; }
		if ( gEventos[gEV13]  ) {
			tkGprs_subState = gac28_fn(); 	// gTR28
		} else {
			tkGprs_subState = gac29_fn();	// gTR29
		}
		break;
	case gSST_ONoffline_sqe01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_sqe01"); break; }
		if ( gEventos[gEV23] ) {
			tkGprs_subState = gac41_fn();	// gTR41
		} else {
			tkGprs_subState = gac40_fn();	// gTR40
		}
		break;
	case gSST_ONoffline_sqe02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_sqe02"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac31_fn(); break; }				// gTR31
		break;
	case gSST_ONoffline_sqe03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_sqe03"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac42_fn(); break; }				// gTR42
		break;
	case gSST_ONoffline_sqe04:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_sqe04"); break; }
		if ( gEventos[gEV13] ) {
			tkGprs_subState = gac43_fn();	// gTR43
		} else {
			tkGprs_subState = gac44_fn();	// gTR44
		}
		break;
	case gSST_ONoffline_apn:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_apn"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac32_fn(); break; }				// gTR32
		break;
	case gSST_ONoffline_ip:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_ip"); break; }
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac33_fn(); break; }				// gTR33
		break;
	case gSST_ONoffline_dummy04:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy04"); break; }
		if ( gEventos[gEV10] ) {
			tkGprs_subState = gac39_fn(); // gTR39
		} else {
			tkGprs_subState = gac34_fn(); // gTR34
		}
		break;
	case gSST_ONoffline_dummy05:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy05"); break; }
		if ( gEventos[gEV11] ) {
			tkGprs_subState = gac35_fn();	// gTR35
		} else {
			tkGprs_subState = gac36_fn(); 	// gTR36
		}
		break;
	case gSST_ONoffline_dummy06:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONoffline_dummy06"); break; }
		if ( gEventos[gEV12] ) {
			tkGprs_subState = gac37_fn(); 	// gTR37
		} else {
			tkGprs_subState = gac38_fn(); 	// gTR38
		}
		break;
	default:
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR sst_onOffline: subState  (%d) NOT DEFINED\r\n\0"),tkGprs_subState);
		logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
		tkGprs_state = gST_OFF;
		tkGprs_subState = gSST_OFF_Init;
		break;
	}

}
/*------------------------------------------------------------------------------------*/

static void SM_onInit(void)
{

	// Maquina de estados del estado ONinit

	switch ( tkGprs_subState ) {
	case  gSST_ONinit_Entry:
		tkGprs_subState = gac50_fn();													// gTR50
		break;
	case gSST_ONinit_socket:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_socket"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac51_fn(); break; }				// gTR51
		break;
	case gSST_ONinit_dummyS01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyS01"); break; } // gTRacRCF
		if ( gEventos[gEV15]  ) {
			tkGprs_subState = gac57_fn(); 	// gTR57
		} else {
			tkGprs_subState = gac52_fn(); 	// gTR52
		}
		break;
	case gSST_ONinit_dummyS02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyS02"); break; } // gTRacRCF
		if ( gEventos[gEV11] ) {
			tkGprs_subState = gac53_fn(); 	// gTR53
		} else {
			tkGprs_subState = gac54_fn();	// gTR54
		}
		break;
	case gSST_ONinit_dummyS03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyS03"); break; } // gTRacRCF
		if ( gEventos[gEV16] ) {
			tkGprs_subState = gac55_fn();	// gTR55
		} else {
			tkGprs_subState = gac56_fn(); 	// gTR56
		}
		break;
	case gSST_ONinit_initFrame:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_initFrame"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac58_fn(); break; }				// gTR58
		break;
	case gSST_ONinit_dummyF01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyF01"); break; } // gTRacRCF
		if ( gEventos[gEV17] ) {	tkGprs_subState = gac59_fn(); break; }				// gTR59
		if ( !gEventos[gEV17] ) {	tkGprs_subState = gac60_fn(); break; }				// gTR60
		break;
	case gSST_ONinit_dummyF02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyF02"); break; } // gTRacRCF
		if ( gEventos[gEV15] ) {
			tkGprs_subState = gac61_fn(); 	// gTR61
		} else {
			tkGprs_subState = gac66_fn(); 	// gTR66
		}
		break;
	case gSST_ONinit_dummyF03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyF03"); break; } // gTRacRCF
		if ( gEventos[gEV11] ) {
			tkGprs_subState = gac62_fn(); 	// gTR62
		} else {
			tkGprs_subState = gac63_fn(); 	// gTR63
		}
		break;
	case gSST_ONinit_dummyF04:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyF04"); break; } // gTRacRCF
		if ( gEventos[gEV16] ) {
			tkGprs_subState = gac64_fn(); 	// gTR64
		} else {
			tkGprs_subState = gac65_fn(); 	// gTR65
		}
		break;
	case gSST_ONinit_dummyF05:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_dummyF05"); break; } // gTRacRCF
		if ( gEventos[gEV16] ) {
			tkGprs_subState = gac67_fn();	// gTR67
		} else {
			tkGprs_subState = gac68_fn(); 	// gTR68
		}
		break;
	case gSST_ONinit_await01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONinit_await01"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac69_fn(); break; }				// gTR69
		if ( ! gEventos[gEV15] ) {	tkGprs_subState = gac69_fn(); break; }				// gTR69
		break;
	default:
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR sst_onInit: subState (%d) NOT DEFINED\r\n\0"),tkGprs_subState);
		logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
		tkGprs_state = gST_OFF;
		tkGprs_subState = gSST_OFF_Init;
		break;
	}

}
/*------------------------------------------------------------------------------------*/

static void SM_onData(void)
{
	// Maquina de estados del estado ONData

	switch ( tkGprs_subState ) {
	case  gSST_ONdata_Entry:
		tkGprs_subState = gac70_fn();													// gTR70
		break;
	case gSST_ONdata_data:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_data"); break; } // gTRacRCF
		if ( gEventos[gEV18] ) {
			tkGprs_subState = gac72_fn(); 	// gTR72
		} else {
			tkGprs_subState = gac71_fn(); 	// gTR71
		}
		break;
	case gSST_ONdata_dummy01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummy01"); break; } // gTRacRCF
		if ( gEventos[gEV19] ) {
			tkGprs_subState = gac73_fn(); 	// gTR73
		} else {
			tkGprs_subState = gac74_fn(); 	// gTR74
		}
		break;
	case gSST_ONdata_await01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_await01"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac75_fn(); break; }				// gTR75
		break;
	case gSST_ONdata_dummy02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummy02"); break; } // gTRacRCF
		if ( gEventos[gEV15] ) {
			tkGprs_subState = gac84_fn(); 	// gTR84
		} else {
			tkGprs_subState = gac76_fn();   // gTR76
		}
		break;
	case gSST_ONdata_socket:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_socket"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac77_fn(); break; }				// gTR77
		break;
	case gSST_ONdata_dummyS01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyS01"); break; } // gTRacRCF
		if ( gEventos[gEV15] ) {
			tkGprs_subState = gac79_fn(); 	// gTR79
		} else {
			tkGprs_subState = gac78_fn(); 	// gTR78
		}
		break;
	case gSST_ONdata_dummyS02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyS02"); break; } // gTRacRCF
		if ( gEventos[gEV11] ) {
			tkGprs_subState = gac80_fn();	// gTR80
		} else {
			tkGprs_subState = gac81_fn();  	// gTR81
		}
		break;
	case gSST_ONdata_dummyS03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyS03"); break; } // gTRacRCF
		if ( gEventos[gEV14] ) {
			tkGprs_subState = gac82_fn(); 	// gTR82
		} else {
			tkGprs_subState = gac83_fn();  	// gTR83
		}
		break;
	case gSST_ONdata_frame:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_frame"); break; } // gTRacRCF
		if ( gEventos[gEV22] ) {
			tkGprs_subState = gac85_fn(); 	// gTR85
		} else {
			tkGprs_subState = gac86_fn();	// gTR86
		}
		break;
	case gSST_ONdata_dummyF01:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyF01"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac87_fn(); break; }				// gTR87
		break;
	case gSST_ONdata_dummyF02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyF02"); break; } // gTRacRCF
		if ( gEventos[gEV21] ) {
			tkGprs_subState = gac88_fn();  	// gTR88
		} else {
			tkGprs_subState = gac89_fn();  	// gTR89
		}
		break;
	case gSST_ONdata_dummyF03:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyF03"); break; } // gTRacRCF
		if ( gEventos[gEV15] ) {
			tkGprs_subState = gac90_fn();	// gTR90
		} else {
			tkGprs_subState = gac91_fn();  	// gTR91
		}
		break;
	case gSST_ONdata_dummyF04:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyF04"); break; } // gTRacRCF
		if ( gEventos[gEV20] ) {
			tkGprs_subState = gac94_fn(); 	// gTR94
		} else {
			tkGprs_subState = gac95_fn();	// gTR95
		}
		break;
	case gSST_ONdata_dummyF05:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_dummyF05"); break; } // gTRacRCF
		if ( gEventos[gEV11] ) {
			tkGprs_subState = gac92_fn();	// gTR92
		} else {
			tkGprs_subState = gac93_fn();  // gTR93
		}
		break;
	case gSST_ONdata_await02:
		if ( gEventos[gEV01] ) {	tkGprs_subState = gacRELOAD_CONFIG_fn("gSST_ONdata_await02"); break; } // gTRacRCF
		if ( gEventos[gEV03] ) {	tkGprs_subState = gac96_fn(); break; }				// gTR96
		if ( ! gEventos[gEV15] ) {	tkGprs_subState = gac96_fn(); break; }				// gTR96
		break;

	default:
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::ERROR sst_onInit: subState (%d) NOT DEFINED\r\n\0"),tkGprs_subState);
		logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
		tkGprs_state = gST_OFF;
		tkGprs_subState = gSST_OFF_Init;
		initStatus = TRUE;
		break;
	}

}
/*------------------------------------------------------------------------------------*/

static int gacRELOAD_CONFIG_fn(const char *s)
{


	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::RCONF:%s\r\n\0"),s);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	fl_tkGprsReloadConfigFlag = FALSE;
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/
/*
 *  FUNCIONES DEL ESTADO OFF:
 *  El modem esta apagado ya sea en standby o idle, y sale hasta quedar prendido
 *  Utilizamos 2 timers, timerToDial y timer1.
 *  Timer1 es general.
 *  TimerToDial es solo para la espera a un nuevo dial, ya que es consultado desde la
 *  tarea de cmd, y entonces siempre debe tener el tiempo para un nuevo dial.
 *
 */
/*------------------------------------------------------------------------------------*/

static int gac00_fn(void)
{

	// Evento inicial. Solo salta al primer estado operativo.
	// Inicializo el sistema aqui
	// gST_INIT -> gST_Off_Init

	strncpy_P(systemVars.dlgIp, PSTR("000.000.000.000\0"),16);
	systemVars.csq = 0;
	systemVars.dbm = 0;
	fl_tkGprsReloadConfigFlag = FALSE;

	pv_tkGprs_printExitMsg(0);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_Init);
}
/*------------------------------------------------------------------------------------*/

static int gac01_fn(void)
{
	// gST_Off_Init -> gST_Off_Idle

	s08 retS;

	// Apago el modem
	retS = MCP_setGprsPwr(0);
	// Activo el pwr del modem para que no consuma
	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );
	retS = MCP_setGprsPwr(1);

	modemStatus = M_OFF_IDLE;

	counterCtlState = 19000; // 1/2 hora = 30 * 60 * 10ticks/s
	pvGPRS_loadTimer( &timer1, 1800 );
	pv_tkGprs_printExitMsg(1);
	return(gSST_OFF_Idle);
}
/*------------------------------------------------------------------------------------*/

static int gac02_fn(void)
{

	// gST_Off_Idle -> gST_Off_Init

	// Si entre a este modo por modo service, al salir vuelvo al modo normal
	if ( systemVars.wrkMode == WK_SERVICE ) {
		systemVars.wrkMode = WK_NORMAL;
		// Se lo indico a la tarea de DATA
		SIGNAL_tkDataRefresh();
	}

	pv_tkGprs_printExitMsg(2);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_Init);
}
/*------------------------------------------------------------------------------------*/

static int gac03_fn(void)
{
	// gST_Off_Init -> gST_Off_Standby
	// Me preparo para generar la espera de timerDial.
	s08 retS;

	switch (systemVars.pwrMode ) {
	case PWR_CONTINUO:
		pvGPRS_loadTimer( &timerToDial, 90);
		break;
	case PWR_DISCRETO:
		pvGPRS_loadTimer( &timerToDial, systemVars.timerDial );
		break;
	default:
		pvGPRS_loadTimer( &timerToDial, 300);
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR03::ERROR pwrMode:\r\n\0"));
		logPrintStr(LOG_NONE, &pvGPRS_printfBuff);
		break;
	}

	// Al arranque disco a los 15s sin importar el modo.
	if ( arranqueFlag == TRUE) {
		arranqueFlag = FALSE;
		pvGPRS_loadTimer( &timerToDial, 15);
	}

	// Apago el modem
	retS = MCP_setGprsPwr(0);
	// necesario para que no prenda por ruido.
	retS = MCP_setGprsSw(0);

	modemStatus = M_OFF;
	fl_gprsAllowsSleep = TRUE;
	HWtryes = MAX_HWTRYES;

	// Activo el pwr del modem para que no consuma
	vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );
	retS = MCP_setGprsPwr(1);

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR03 Modem off(await=%d):\r\n\0"), timerToDial.timer);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);

	// Puedo esperar bastante...
	counterCtlState = 10 * systemVars.timerDial + 1000;
	pv_tkGprs_printExitMsg(3);
	return(gSST_OFF_Standby);
}
/*------------------------------------------------------------------------------------*/

static int gac04_fn(void)
{

	// gST_Off_Standby -> gST_Off_await01
	// Prendo el modem y espero el settle time de la fuente ( 5s)

	s08 retS;

	// Activo la fuente
	retS = MCP_setGprsPwr(1);

	pvGPRS_loadTimer( &timer1, 5 );
	fl_gprsAllowsSleep = FALSE;

//	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR04 (t=%d)(hw=%d):\r\n\0"),timer1.timer, intentosHW);
//	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);

	pv_tkGprs_printExitMsg(4);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_await01);
}
/*------------------------------------------------------------------------------------*/

static int gac05_fn(void)
{
	// gST_Off_await01 -> gST_Off_await02
	// Genero un pulso en el SW del modem para prenderlo

	s08 retS;


	retS = MCP_setGprsSw(0);
	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );
	retS = MCP_setGprsSw(1);

	// Inicializo el timer para esperar el pwr settle time
	// (que empieze a pizcar)
	pvGPRS_loadTimer( &timer1, 30 );

	SWtryes = MAX_SWTRYES;

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR05 (t=%d)(sw=%d)(hw=%d):\r\n\0"),timer1.timer,SWtryes,HWtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(5);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_await02);
}
/*------------------------------------------------------------------------------------*/

static int gac06_fn(void)
{
	// gST_Off_await02 -> gST_Off_await03

	// Envio AT
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT\r");

	// Inicializo el timer para esperar el cmdRsp
	pvGPRS_loadTimer( &timer1, 2 );

	pv_tkGprs_printExitMsg(6);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_await03);
}
/*------------------------------------------------------------------------------------*/

static int gac07_fn(void)
{
	// gST_Off_await03 -> gST_Off_dummy01

	// Evaluo la respuesta al comando AT
	if ( gprsRspIs("OK\0") == TRUE ) {
		GPRSrsp = RSP_OK;
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(7);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_dummy01);
}
/*------------------------------------------------------------------------------------*/

static int gac08_fn(void)
{
	// gST_Off_dummy01 -> gST_OnOffline

	pv_tkGprs_printExitMsg(8);

	// CAMBIO DE ESTADO:
	counterCtlState = MAXTIMEINASTATE;
	tkGprs_state = gST_ONoffline;
	return(gSST_ONoffline_Entry);
}
/*------------------------------------------------------------------------------------*/

static int gac09_fn(void)
{
	// gST_Off_dummy01 -> gST_Off_dummy02

	pv_tkGprs_printExitMsg(9);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_dummy02);
}
/*------------------------------------------------------------------------------------*/

static int gac10_fn(void)
{
	// gST_Off_dummy02 -> gST_Off_await02

	s08 retS;

	// Genero un pulso en el SW del modem para prenderlo
	retS = MCP_setGprsSw(0);
	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );
	retS = MCP_setGprsSw(1);

	// Inicializo el timer para esperar el pwr settle time
	pvGPRS_loadTimer( &timer1, 30 );

	if ( SWtryes > 0 ) { SWtryes--; }

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR10:(t=%d)(sw=%d)(hw=%d):\r\n\0"),timer1.timer,SWtryes,HWtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(10);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_await02);
}
/*------------------------------------------------------------------------------------*/

static int gac11_fn(void)
{
	// gST_Off_dummy02 -> gST_Off_dummy03

	pv_tkGprs_printExitMsg(11);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_dummy03);
}
/*------------------------------------------------------------------------------------*/

static int gac12_fn(void)
{
	// gST_Off_dummy03 -> gST_Off_standby

	s08 retS;

	// Apago y prendo la fuente HW
	retS = MCP_setGprsPwr(0);

	// Inicializo el timer de settle time
	pvGPRS_loadTimer( &timerToDial, 15 );

	if (HWtryes > 0 ) { HWtryes--; }

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR12:(t=%d)(sw=%d)(hw=%d):\r\n\0"),timer1.timer,SWtryes,HWtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(12);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_Standby);
}
/*------------------------------------------------------------------------------------*/

static int gac13_fn(void)
{
	// gST_Off_dummy03 -> gST_Off_init

	pv_tkGprs_printExitMsg(13);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_OFF_Init);
}
/*------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------*/
/*
 *  FUNCIONES DEL ESTADO ONoffline:
 *  El modem esta prendido pero requiere de congiguracion, APN, IP.
 *
 */
/*------------------------------------------------------------------------------------*/

static int gac20_fn(void)
{
	// stOFF::gSST_ONoffline_Entry -> gSST_ONoffline_cpin

	// init_tryes
	tryes = MAXTRYES;
	modemStatus = M_ON_CONFIG;

	// query CPIN
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT+CPIN?\r");

	vTaskDelay( (portTickType)(1000 / portTICK_RATE_MS) );

	// Evaluo la respuesta al comando AT
	if ( gprsRspIs("READY") == TRUE )  {
		GPRSrsp = RSP_READY;
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(20);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_cpin);
}
/*------------------------------------------------------------------------------------*/

static int gac21_fn(void)
{
	// gSST_ONoffline_cpin -> gSST_ONoffline_net

	// Send CONFIG
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT&D0&C1\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// Configuro la secuencia de escape +++AT
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2IPS=2,8,2,1020,0,15\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );


	// SMS Envio: Los envio en modo texto
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CMGF=1\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// SMS Recepcion: No indico al TE ni le paso el mensaje
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CNMI=1,0\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// SMS indicacion: Bajando el RI por 100ms.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2SMSRI=100\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// Borro todos los SMS de la memoria
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CMGD=0,4\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// Deshabilito los mensajes
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2IPEV=0,0\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );

	// Query NET available
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CREG?\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	// init_timer
	pvGPRS_loadTimer( &timer1, 5 );

	// init tryes
	tryes = MAXTRYES;

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR21: NET register (%d)\r\n\0"),tryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(21);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_net);
}
/*------------------------------------------------------------------------------------*/

static int gac22_fn(void)
{
	// gST_Off_cpin -> gST_Off_await01

	// Inicializo el timer para esperar el cmdRsp
	pvGPRS_loadTimer( &timer1, 10 );

	pv_tkGprs_printExitMsg(22);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_await01);
}
/*------------------------------------------------------------------------------------*/

static int gac23_fn(void)
{
	// gST_Off_await01 -> gST_Off_dummy01

	pv_tkGprs_printExitMsg(23);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy01);
}
/*------------------------------------------------------------------------------------*/

static int gac24_fn(void)
{
	// gST_Off_dummy01 -> gST_Off_cpin

	if ( tryes > 0 ) {
		tryes--;
	}

	// Set CPIN
/*	rprintfStr(GPRS_UART, "AT+CPIN=\"\0");
	rprintfStr(GPRS_UART, systemVars.cpin);
	rprintfStr(GPRS_UART, "\"\r");
	vTaskDelay( portTICK_RATE_MS * 500 );
*/
	// query CPIN
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT+CPIN?\r");

	vTaskDelay( (portTickType)(500 / portTICK_RATE_MS) );

	// Evaluo la respuesta al comando AT
	if ( gprsRspIs("READY") == TRUE )  {
		GPRSrsp = RSP_READY;
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(24);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_cpin);

}
/*------------------------------------------------------------------------------------*/

static int gac25_fn(void)
{
	// gST_Off_dummy01 -> gST_Off_EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(25);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac26_fn(void)
{
	// stOFF::gSST_ONoffline_net -> gSST_ONoffline_dummy02

	// Evaluo la respuesta al comando AT+CREG ( home network )
	if ( gprsRspIs("+CREG: 0,1") == TRUE ) {
		GPRSrsp = RSP_CREG;
		debugPrintModemBuffer();
	}

	// Evaluo la respuesta al comando AT+CREG ( roaming network )
//	if ( gprsRspIs("+CREG: 0,5") == TRUE ) {
//		GPRSrsp = RSP_CREG;
//		debugPrintModemBuffer();
//	}

	pv_tkGprs_printExitMsg(26);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy02);
}
/*------------------------------------------------------------------------------------*/

static int gac27_fn(void)
{
	// gST_Off_dummy02 -> gST_Off_dummy03

	pv_tkGprs_printExitMsg(27);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy03);
}
/*------------------------------------------------------------------------------------*/

static int gac28_fn(void)
{
	// gST_Off_dummy03 -> gST_Off_net

	if (tryes > 0) {
		tryes--;
	}

	pvGPRS_loadTimer( &timer1, 5 );

	// Query NET available
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CREG?\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR28: retry NET register (%d)\r\n\0"),tryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(28);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_net);
}
/*------------------------------------------------------------------------------------*/

static int gac29_fn(void)
{
	// gST_Off_dummy03 -> gST_Off_EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(29);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac30_fn(void)
{
	// gSST_ONoffline_dummy02 -> gSST_ONoffline_sqe01

	// Query Net response
	debugPrintModemBuffer();

	pv_tkGprs_printExitMsg(30);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_sqe01);

}
/*------------------------------------------------------------------------------------*/

static int gac31_fn(void)
{
	// gSST_ONoffline_sqe -> gSST_ONoffline_apn

	char *ts = NULL;

	// READ SQE
	debugPrintModemBuffer();

	ts = strchr(&xUart0RxedCharsBuffer.buffer, ':');
	ts++;
	systemVars.csq = atoi(ts);
	systemVars.dbm = 113 - 2 * systemVars.csq;

	// SET APN
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT+CGDCONT=1,\"IP\",\"%s\"\r\n"),systemVars.apn);
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	// init_timer
	pvGPRS_loadTimer( &timer1, 5 );

	pv_tkGprs_printExitMsg(31);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_apn);

}
/*------------------------------------------------------------------------------------*/

static int gac32_fn(void)
{
	// gSST_ONoffline_apn -> gSST_ONoffline_ip

	// Vamos a esperar en forma persistente hasta 30s. por la IP durante 3 intentos
	IPtryes = MAX_IPTRYES;
	pTryes = 6;

	pvGPRS_loadTimer( &timer1, 5 );

	// ASK IP.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2IPA=1,1\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR32: ASK IP (%d)\r\n\0"),IPtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(32);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_ip);

}
/*------------------------------------------------------------------------------------*/

static int gac33_fn(void)
{
	// stOFF::gSST_ONoffline_ip -> gSST_ONoffline_dummy04

	// Evaluo la respuesta al comando AT*E2IPA
	if ( gprsRspIs("OK\0") == TRUE ) {
		GPRSrsp = RSP_IPOK;
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(33);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy04);
}
/*------------------------------------------------------------------------------------*/

static int gac34_fn(void)
{
	// gST_Off_dummy04 -> gST_Off_dummy05

	pv_tkGprs_printExitMsg(34);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy05);
}
/*------------------------------------------------------------------------------------*/

static int gac35_fn(void)
{
	// gST_Off_dummy05 -> gST_Off_ip

	if ( pTryes > 0 ) {
		pTryes--;
	}

	pvGPRS_loadTimer( &timer1, 5 );

	pv_tkGprs_printExitMsg(35);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_ip);
}
/*------------------------------------------------------------------------------------*/

static int gac36_fn(void)
{
	// gST_Off_dummy05 -> gST_Off_dummy06

	pv_tkGprs_printExitMsg(36);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_dummy06);
}
/*------------------------------------------------------------------------------------*/

static int gac37_fn(void)
{
	// gSST_ONoffline_dummy06 -> gSST_ONoffline_ip

	if (IPtryes > 0 ) {
		IPtryes--;
	}
	pTryes = 6;
	pvGPRS_loadTimer( &timer1, 5 );

	// ASK IP.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2IPA=1,1\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR37: ASK IP (%d)\r\n\0"),IPtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(37);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_ip);

}
/*------------------------------------------------------------------------------------*/

static int gac38_fn(void)
{
	// gST_Off_dummy06 -> gST_Off_EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(38);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac39_fn(void)
{
	// gSST_ONoffline_dummy04 -> gSST_ON_INIT

	// Query IP address.
	char *ts = NULL;
	int i=0;
	char c;

	// Leo la respuesta del modem al comando anterior ( E2IPA=1,1)
	debugPrintModemBuffer();

	// ASK IP.
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT*E2IPI=0\r");

	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );

	// Leo la respuesta del modem
	debugPrintModemBuffer();

	// Extraigo la IP del token
	ts = strchr( &xUart0RxedCharsBuffer.buffer, '\"');
	ts++;
	while ( (c= *ts) != '\"') {
		systemVars.dlgIp[i++] = c;
		ts++;
	}
	systemVars.dlgIp[i++] = NULL;

	modemStatus = M_ON_READY;

	pv_tkGprs_printExitMsg(39);
	// CAMBIO DE ESTADO:
	counterCtlState = MAXTIMEINASTATE;
	tkGprs_state = gST_ONInit;
	return(gSST_ONinit_Entry);
}
/*------------------------------------------------------------------------------------*/

static int gac40_fn(void)
{
	// gSST_ONoffline_sqe01 -> gSST_ONoffline_sqe02

	// Query SQE
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT+CSQ\r");

	// init_timer
	pvGPRS_loadTimer( &timer1, 5 );

	pv_tkGprs_printExitMsg(40);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_sqe02);

}
/*------------------------------------------------------------------------------------*/

static int gac41_fn(void)
{
	// gSST_ONoffline_sqe01 -> gSST_ONoffline_sqe03

	// Query SQE
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT+CSQ\r");

	// init tryes
	tryes = 180;

	// init_timer
	pvGPRS_loadTimer( &timer1, 10 );

	pv_tkGprs_printExitMsg(41);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_sqe03);

}
/*------------------------------------------------------------------------------------*/

static int gac42_fn(void)
{
	// gSST_ONoffline_sqe03 -> gSST_ONoffline_sqe04

	char *ts = NULL;

	// READ SQE
	debugPrintModemBuffer();

	ts = strchr(&xUart0RxedCharsBuffer.buffer, ':');
	ts++;
	systemVars.csq = atoi(ts);
	systemVars.dbm = 113 - 2 * systemVars.csq;

	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGprs::SQE monitor: %d -> %d(dB)\r\n\0"),systemVars.csq, systemVars.dbm );
	logPrintStr(LOG_NONE, &pvGPRS_printfBuff);

	// dec_Tryes
	if (tryes > 0) {
		tryes--;
	}

	pv_tkGprs_printExitMsg(42);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_sqe04);

}
/*------------------------------------------------------------------------------------*/

static int gac43_fn(void)
{
	// gSST_ONoffline_sqe04 -> gSST_ONoffline_sqe03

	// Query SQE
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, "AT+CSQ\r");

	// init_timer
	pvGPRS_loadTimer( &timer1, 10 );


	pv_tkGprs_printExitMsg(43);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONoffline_sqe03);

}
/*------------------------------------------------------------------------------------*/

static int gac44_fn(void)
{
	// gSST_ONoffline_sqe04 -> gST_Off_EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(44);
	return(gSST_OFF_Entry);

}

/*------------------------------------------------------------------------------------*/
/*
 *  FUNCIONES DEL ESTADO ON_INIT:
 *  Es el estado en el cual el modem esta abriendo por primera vez un socket hacia
 *  el server, enviando un frame de INIT y esperando la respuesta para reconfigurarse
 */
/*------------------------------------------------------------------------------------*/

static int gac50_fn(void)
{

	// gSST_ONInitEntry -> gSST_ONInit_socket

	// Espero en forma persistente abrir el socket por 16s
	pTryes = 8;
	LOOPtryes = MAX_LOOPTRYES;
	pvGPRS_loadTimer( &timer1, 2 );

	// OPEN SOCKET.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, 128);
	pv_tkGprs_openSocket();

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR50: OPEN SOCKET (%d)\r\n\0"),LOOPtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(50);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac51_fn(void)
{
	// gSST_ONinit_socket -> gSST_ONinit_dummyS01

	// Evaluo la respuesta al comando AT*E2IPO ( open socket )
	if ( gprsRspIs("CONNECT") == TRUE ) {
		GPRSrsp = RSP_CONNECT;
		// El socket abrio: Muestro el mensaje del modem (CONNECT)
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(51);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyS01);

}
/*------------------------------------------------------------------------------------*/

static int gac52_fn(void)
{
	// gSST_ONinit_dummyS01 -> gSST_ONinit_dummyS02

	pv_tkGprs_printExitMsg(52);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyS02);

}
/*------------------------------------------------------------------------------------*/

static int gac53_fn(void)
{
	// gSST_ONinit_dummyS02 -> gSST_ONinit_socket

	if (pTryes > 0 ) {
		pTryes--;
	}
	pvGPRS_loadTimer( &timer1, 2 );

	pv_tkGprs_printExitMsg(53);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac54_fn(void)
{
	// gSST_ONinit_dummyS02 -> gSST_ONinit_dummyS03

	pv_tkGprs_printExitMsg(54);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyS03);

}
/*------------------------------------------------------------------------------------*/

static int gac55_fn(void)
{

	// gSST_ONInit_dummyS03 -> gSST_ONInit_socket

	// Espero en forma persistente abrir el socket por 16s
	if ( LOOPtryes > 0 ) {
		LOOPtryes--;
	}
	pTryes = 8;
	pvGPRS_loadTimer( &timer1, 2 );

	// OPEN SOCKET.
	pv_tkGprs_openSocket();

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR55: retry OPEN SOCKET (%d)\r\n\0"),LOOPtryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(55);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac56_fn(void)
{
	// gSST_ONInit_dummyS03 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(56);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac57_fn(void)
{
	// gSST_ONinit_dummy01 -> gSST_ONinit_initFrame

	// El socket abrio por lo que tengo que trasmitir el frame de init.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, 128);
	sendInitFrame();

	// No inicializo ya que el total de reintentos combinados no debe superar a LOOPTryes
	if ( LOOPtryes > 0 ) {
		LOOPtryes--;
	}
	// Vamos a esperar en forma persistente hasta 15s
	pTryes = 5;
	pvGPRS_loadTimer( &timer1, 3 );

	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR57 INIT FRAME {%d inits}\r\n\0"), LOOPtryes );
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//logPrintStr(LOG_INFO, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(57);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_initFrame);

}
/*------------------------------------------------------------------------------------*/

static int gac58_fn(void)
{
	// gSST_ONinit_initFrame -> gSST_ONinit_dummyF01

	// Evaluo si llego una respuesta del server al frame de INIT
	if ( gprsRspIs("INIT_OK") == TRUE ) {
		GPRSrsp = RSP_INIT;
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(58);
	counterCtlState = MAXTIMEINASTATE;
	return( gSST_ONinit_dummyF01);

}
/*------------------------------------------------------------------------------------*/

static int gac59_fn(void)
{
	// gSST_ONinit_dummyF01 -> gSST_ONData_Entry

char *ptr1 = NULL;
char *ptr2 = NULL;
u08 i;
char *ptr;
s16 nro;
u08 saveFlag = 0;
s08 retS = FALSE;

	// Analizo y proceso la respuesta del buffer.

	// aislo la linea con la respuesta y la parseo:
	memset( &lbuff, '\0', sizeof(lbuff));
	ptr1 = strstr( &xUart0RxedCharsBuffer.buffer, "<h1>");
	ptr2 = strstr( &xUart0RxedCharsBuffer.buffer, "</h1>");
	for ( i=0; ptr1 != ptr2; ptr1++, i++) {
		lbuff[i] = *ptr1;
	}

	if ( sp5K_debugControl(D_GPRS) ) {
		if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
			rprintfStr(CMD_UART, ".tkGPRS::gTR59: RXLINE=");
			rprintfStr(CMD_UART, &lbuff );
			rprintfStr(CMD_UART, "\r\n\0" );
			xSemaphoreGive( sem_CmdUart );
		}
	}

	// Lineas de respuesta al INIT: tag: INIT_OK:
	// Pueden contener parametros de reconfiguracion.
	saveFlag = 0;
	pv_tkGprsProcessServerClock();
	saveFlag = pv_tkGprsProcessPwrMode();
	saveFlag += pv_tkGprsProcessTimerPoll();
	saveFlag += pv_tkGprsProcessTimerDial();

	// Reconfiguro los canales.

#ifdef CHANNELS_3
	saveFlag += pv_tkGprsProcessAnalogChannels(0);
	saveFlag += pv_tkGprsProcessAnalogChannels(1);
	saveFlag += pv_tkGprsProcessAnalogChannels(2);

	saveFlag += pv_tkGprsProcessDigitalChannels(0);
	saveFlag += pv_tkGprsProcessDigitalChannels(1);

	// Consignas
	saveFlag += pv_tkGprsProcessConsignas();
#endif

	if ( saveFlag > 0 ) {
		for ( i=0; i<3; i++) {
			vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );
			retS = saveSystemParamsInEE( &systemVars );
			if ( retS ) {
				snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR59: SAVE OK\r\n\0"));
				break;
			} else {
				snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR59: SAVE FAIL(%d)\r\n\0"),i);
			}
		}
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	}

	pv_tkGprs_closeSocket();
	pvGPRS_loadTimer( &timer1, 10 );
	pv_tkGprs_printExitMsg(59);
	return(gSST_ONinit_await01);
}
/*------------------------------------------------------------------------------------*/

static int gac60_fn(void)
{
	// gSST_ONinit_dummyF01 -> gSST_ONinit_dummyF02

	pv_tkGprs_printExitMsg(60);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyF02);

}
/*------------------------------------------------------------------------------------*/

static int gac61_fn(void)
{
	// gSST_ONinit_dummyF02 -> gSST_ONinit_dummyF03

	pv_tkGprs_printExitMsg(61);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyF03);

}
/*------------------------------------------------------------------------------------*/

static int gac62_fn(void)
{
	// gSST_ONinit_dummyF03 -> gSST_ONinit_initFrame

	if (pTryes > 0 ) {
		pTryes--;
	}
	pvGPRS_loadTimer( &timer1, 3 );

	pv_tkGprs_printExitMsg(62);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_initFrame);

}
/*------------------------------------------------------------------------------------*/

static int gac63_fn(void)
{
	// gSST_ONinit_dummyF03 -> gSST_ONinit_dummyF04

	pv_tkGprs_printExitMsg(63);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyF04);

}
/*------------------------------------------------------------------------------------*/

static int gac64_fn(void)
{
	// gSST_ONinit_dummyF04 -> gSST_ONinit_initFrame

	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, 128);
	sendInitFrame();
	if ( LOOPtryes > 0 ) {
		LOOPtryes--;
	}
	// Vamos a esperar en forma persistente hasta 15s
	pTryes = 5;
	pvGPRS_loadTimer( &timer1, 3 );

	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR64 retry INIT FRAME {%d inits}\r\n\0"), LOOPtryes );
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//logPrintStr(LOG_INFO, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(64);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_initFrame);

}
/*------------------------------------------------------------------------------------*/

static int gac65_fn(void)
{
	// gSST_ONInit_dummyF04 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(65);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac66_fn(void)
{
	// gSST_ONinit_dummyF02 -> gSST_ONinit_dummyF05

	pv_tkGprs_printExitMsg(66);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_dummyF05);

}
/*------------------------------------------------------------------------------------*/

static int gac67_fn(void)
{

	// gSST_ONInit_dummy05 -> gSST_ONInit_socket

	// Espero en forma persistente abrir el socket por 30s
	if (LOOPtryes > 0 ) {
		LOOPtryes--;
	}
	pTryes = 15;

	pvGPRS_loadTimer( &timer1, 2 );

	// OPEN SOCKET.
	pv_tkGprs_openSocket();

	snprintf_P( pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR67: retry OPEN SOCKET (%d)\r\n\0"),LOOPtryes);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(66);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONinit_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac68_fn(void)
{
	// gSST_ONInit_dummyF05 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(68);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac69_fn(void)
{

	// CAMBIO DE ESTADO:
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(69);
	tkGprs_state = gST_ONData;
	return(gSST_ONdata_Entry);

}

/*------------------------------------------------------------------------------------*/
/*
 *  FUNCIONES DEL ESTADO ONData:
 *  El modem esta prendido y trasmitiendo datos al server.
 */
/*------------------------------------------------------------------------------------*/

static int gac70_fn(void)
{
    // gSST_ONdata_Entry ->  gSST_ONdata_data

	// Inicializo el contador de reintentos de envio del mismo frame
	BULKtryes = MAX_BULKTRYES;

	pv_tkGprs_printExitMsg(70);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_data);
}
/*------------------------------------------------------------------------------------*/

static int gac71_fn(void)
{
    // gSST_ONdata_Data ->  gSST_ONdata_dummy02

	pv_tkGprs_printExitMsg(71);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummy02);
}
/*------------------------------------------------------------------------------------*/

static int gac72_fn(void)
{
    // gSST_ONdata_Data ->  gSST_ONdata_dummy01

	pv_tkGprs_printExitMsg(72);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummy01);
}
/*------------------------------------------------------------------------------------*/

static int gac73_fn(void)
{
    // gSST_ONdata_dummy01 ->  gSST_ONdata_await01

	// Espero 30s para volver a chequear la BD.
	pvGPRS_loadTimer( &timer1, 30 );

	pv_tkGprs_printExitMsg(73);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_await01);
}
/*------------------------------------------------------------------------------------*/

static int gac74_fn(void)
{
	// gSST_ONdata_dummy01 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(74);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac75_fn(void)
{
    // gSST_ONdata_await01 ->  gSST_ONdata_data

	pv_tkGprs_printExitMsg(75);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_data);
}
/*------------------------------------------------------------------------------------*/

static int gac76_fn(void)
{

	// gSST_ONdata_dummy02 -> gSST_ONdata_socket

	// Espero en forma persistente abrir el socket por 30s
	SOCKTryes = MAX_SOCKTRYES;
	pTryes = 15;
	pvGPRS_loadTimer( &timer1, 2 );

	// OPEN SOCKET.
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, 128);
	pv_tkGprs_openSocket();

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR76: OPEN SOCKET (%d)\r\n\0"),SOCKTryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(76);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac77_fn(void)
{
	// gSST_ONdata_socket -> gSST_ONdata_dummyS01

	// Evaluo la respuesta al comando AT*E2IPO ( open socket )
	if ( gprsRspIs("CONNECT") == TRUE ) {
		GPRSrsp = RSP_CONNECT;
		// El socket abrio: Muestro el mensaje del modem (CONNECT)
		debugPrintModemBuffer();
	}

	pv_tkGprs_printExitMsg(77);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyS01);

}
/*------------------------------------------------------------------------------------*/

static int gac78_fn(void)
{
	// gSST_ONdata_dummyS01 -> gSST_ONdata_dummyS02

	pv_tkGprs_printExitMsg(78);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyS02);

}
/*------------------------------------------------------------------------------------*/

static int gac79_fn(void)
{
	// gSST_ONdata_dummyS01 -> gSST_ONdata_data

	// El socket abrio.
	pv_tkGprs_printExitMsg(79);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_data);

}
/*------------------------------------------------------------------------------------*/

static int gac80_fn(void)
{
	// gSST_ONdata_dummyS02 -> gSST_ONdata_socket

	if (pTryes > 0 ) {
		pTryes--;
	}
	pvGPRS_loadTimer( &timer1, 2 );

	pv_tkGprs_printExitMsg(80);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac81_fn(void)
{
	// gSST_ONdata_dummyS02 -> gSST_ONdata_dummyS03

	pv_tkGprs_printExitMsg(81);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyS03);

}
/*------------------------------------------------------------------------------------*/

static int gac82_fn(void)
{

	// gSST_ONdata_dummyS03 -> gSST_ONdata_socket

	// Espero en forma persistente abrir el socket por 30s
	if ( SOCKTryes > 0 ) {
		SOCKTryes--;
	}
	pTryes = 15;
	pvGPRS_loadTimer( &timer1, 2 );

	// OPEN SOCKET.
	pv_tkGprs_openSocket();

	snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR82: retry OPEN SOCKET (%d)\r\n\0"),SOCKTryes);
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(82);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_socket);

}
/*------------------------------------------------------------------------------------*/

static int gac83_fn(void)
{
	// gSST_ONdata_dummyS03 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(83);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac84_fn(void)
{
	// gSST_ONdata_dummy02 ->  gSST_ONdata_frame
	// El socket esta abierto.
	// Solo envio el header
	// Send Data Frame
	// GET /cgi-bin/sp5K/sp5K.pl?DLGID=SPY001&PASSWD=spymovil123

	u08 pos = 0;

	if ( BULKtryes > 0 ) {
		BULKtryes--;
	}
	BDRCDcounter = MAX_BDRCDCOUNTER;
	f_sendTail = FALSE;

	// HEADER:
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	pos = snprintf_P( pvGPRS_printfBuff,CHAR384,PSTR("GET " ));
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%s"), systemVars.serverScript );
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("?DLGID=%s"), systemVars.dlgId );
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&PASSWD=%s"), systemVars.passwd );

	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	// Debug la linea trasmitida.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);


	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR84(header)\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(84);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_frame);

}
/*------------------------------------------------------------------------------------*/

static int gac85_fn(void)
{
	// gSST_ONdata_frame ->  gSST_ONdata_dummyF01
	// Solo envio el Tail

	u08 pos = 0;

	// Vamos a esperar en forma persistente hasta 30s la respuesta del server al frame.
	pTryes = 6;
	pvGPRS_loadTimer( &timer1, 5 );

	// TAIL:
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	pos += snprintf_P( pvGPRS_printfBuff, CHAR384,PSTR(" HTTP/1.1\n") );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("Host: www.spymovil.com\n" ));
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\n\n" ));
	// Trasmito
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	snprintf_P( pvGPRS_printfBuff,CHAR384,PSTR("tkGPRS::gTR85(Tail)\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	//pv_tkGprs_printExitMsg(85);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF01);

}
/*------------------------------------------------------------------------------------*/

static int gac86_fn(void)
{
	// gSST_ONdata_frame ->  gSST_ONdata_frame
	// El socket esta abierto.
	// Leo la BD y trasmito el registro.
	// Evaluo la flag de envio del tail.

u08 pos;
u08 channel;
u08 bdGetStatus;
static u16 rcdIndex = 0;

	rcdIndex = (++rcdIndex == BD_getMaxRcds()) ?  0 : rcdIndex;
	if ( BDRCDcounter == MAX_BDRCDCOUNTER ) {
		//Inicializo.
		rcdIndex = BD_getRDptr();
	}

	// Leo el siguiente registro.
	bdGetStatus =  BD_get( &rdFrame, rcdIndex);
	switch (bdGetStatus) {
	case 0: // BD vacia
		snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR86 BD empty.\r\n\0") );
		// Condicion 1.
		f_sendTail = TRUE;
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
		goto quit86;
		break;
	case 1: // Lectura correcta
		break;
	case 2: // Lectura incorrecta
		snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR86 BD Read ERROR.\r\n\0") );
		// Condicion 2.
		f_sendTail = TRUE;
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
		// Borro el registro con errores.
		BD_delete(rcdIndex);
		goto quit86;
		break;
	case 3: // Puntero fuera de rango.
		snprintf_P( &pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR86 rcd out of scope.\r\n\0") );
		// Condicion 3.
		f_sendTail = TRUE;
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
		goto quit86;
		break;
	default:
		goto quit86;
		break;
	}

	if ( BDRCDcounter > 0 ) {
		BDRCDcounter--;
	}

	// Condicion 4.
	if ( BDRCDcounter == 0 ) {
		f_sendTail = TRUE;
	}

	// Lectura correcta: Trasmito
	// Data Frame:
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	pos += snprintf_P( pvGPRS_printfBuff,CHAR384,PSTR("&CTL=%d"), rcdIndex );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("&LINE=") );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%04d%02d%02d,"),rdFrame.rtc.year,rdFrame.rtc.month, rdFrame.rtc.day );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR( "%02d%02d%02d,"),rdFrame.rtc.hour,rdFrame.rtc.min, rdFrame.rtc.sec );

	// Valores analogicos
	for ( channel = 0; channel < NRO_CHANNELS; channel++) {
		pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%s>%.2f,"),systemVars.aChName[channel],rdFrame.analogIn[channel] );
	}

	// Valores digitales.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%sP>%.2f,"), systemVars.dChName[0], rdFrame.din0_pCount );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%sP>%.2f"), systemVars.dChName[1], rdFrame.din1_pCount );

#ifdef CHANNELS_3
	// Bateria.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR(",bt>%.2f%"),rdFrame.batt );
#endif

	// Trasmito
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	// Debug la linea trasmitida.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);

	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( pvGPRS_printfBuff,CHAR384,PSTR("tkGPRS::gTR86 tx (rcdNro=%d, s=%d )\r\n\0"), rcdIndex, BDRCDcounter );
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	logPrintStr(LOG_INFO, &pvGPRS_printfBuff);

quit86:

	pv_tkGprs_printExitMsg(86);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_frame);

}
/*------------------------------------------------------------------------------------*/

static int gac87_fn(void)
{
	// gSST_ONdata_dummyF01 ->  gSST_ONdata_dummyF02

	// Evaluo si llego una respuesta del server al frame de datos.
	if ( gprsRspIs("RX_OK") == TRUE ) {
		GPRSrsp = RSP_RXOK;
		logPrintStr(LOG_INFO, &xUart0RxedCharsBuffer.buffer );
	}

	pv_tkGprs_printExitMsg(87);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF02);

}
/*------------------------------------------------------------------------------------*/

static int gac88_fn(void)
{
	// gSST_ONdata_dummyF02 ->  gSST_ONdata_await02

char *ptr1 = NULL;
char *ptr2 = NULL;
u08 i;
char *ptr;
s16 nro;

	// aislo la linea con la respuesta:
	memset( &lbuff, '\0', sizeof(lbuff));
	ptr1 = strstr( &xUart0RxedCharsBuffer.buffer, "<h1>");
	ptr2 = strstr( &xUart0RxedCharsBuffer.buffer, "</h1>");
	for ( i=0; ptr1 != ptr2; ptr1++, i++) {
		lbuff[i] = *ptr1;
	}

	if ( sp5K_debugControl(D_GPRS) ) {
		if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
			rprintfStr(CMD_UART, ".tkGPRS::gTR88: RXLINE=");
			rprintfStr(CMD_UART, &lbuff );
			rprintfStr(CMD_UART, "\r\n\0" );
			xSemaphoreGive( sem_CmdUart );
		}
	}

	// ANALIZO:
	// Lineas comunes de datos: tag: RX_OK:,<h1>RX_OK:7</h1> busco el nro. de linea para su borrado.
	// Es la confirmacion que el frame llego OK al server.
	// Determino el nro.de linea ACK
	ptr = strchr(&lbuff, ':');
	ptr++;
	nro = atoi(ptr);
	// Lo borro.
	if (nro >= 0) {
		BD_delete(nro);
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR88: (recDelete till rec %d)\r\n\0"), nro);
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
		logPrintStr(LOG_INFO, &pvGPRS_printfBuff);

		// Operacion exitosa: reinicio el contador para habilitar mas operaciones.
		BULKtryes = MAX_BULKTRYES;
	}

	// Si el server mando un RESET, debo salir y volver a reconectarme para actualizar la configuracion
	if (strstr( &lbuff, "RESET") != NULL) {
		snprintf_P( &pvGPRS_printfBuff,CHAR64,PSTR("tkGPRS::gTR88: (RESETline)\r\n\0"));
		sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);

		// Reseteo el sistema
		cli();
		wdt_reset();
		/* Start timed equence */
		WDTCSR |= (1<<WDCE) | (1<<WDE);
		/* Set new prescaler(time-out) value = 64K cycles (~0.5 s) */
		WDTCSR = (1<<WDE) | (1<<WDP2) | (1<<WDP0);
		sei();

		wdt_enable(WDTO_30MS);
		while(1) {}
	}

	pv_tkGprs_closeSocket();
	pvGPRS_loadTimer( &timer1, 10 );
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(88);
	return(gSST_ONdata_await02);

}
/*------------------------------------------------------------------------------------*/

static int gac89_fn(void)
{
	// gSST_ONdata_dummyF02 -> gSST_ONdata_dummyF03

	pv_tkGprs_printExitMsg(89);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF03);

}
/*------------------------------------------------------------------------------------*/

static int gac90_fn(void)
{
	// gSST_ONdata_dummyF03 -> gSST_ONdata_dummyF05

	pv_tkGprs_printExitMsg(90);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF05);

}
/*------------------------------------------------------------------------------------*/

static int gac91_fn(void)
{
	// gSST_ONdata_dummyF03 -> gSST_ONdata_dummyF04

	pv_tkGprs_printExitMsg(91);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF04);

}
/*------------------------------------------------------------------------------------*/

static int gac92_fn(void)
{
	// gSST_ONdata_dummyF05 -> gSST_ONdata_dummyF01

	if (pTryes > 0 ) {
		pTryes--;
	}
	pvGPRS_loadTimer( &timer1, 5 );

	pv_tkGprs_printExitMsg(92);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummyF01);

}
/*------------------------------------------------------------------------------------*/

static int gac93_fn(void)
{
	// gSST_ONdata_dummyF05 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(93);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac94_fn(void)
{
	// gSST_ONdata_dummyF04 -> gSST_ONdata_dummy02

	pv_tkGprs_printExitMsg(94);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_dummy02);

}
/*------------------------------------------------------------------------------------*/

static int gac95_fn(void)
{
	// gSST_ONdata_dummyF05 -> EXIT

	tkGprs_state = gST_OFF;
	counterCtlState = MAXTIMEINASTATE;
	pv_tkGprs_printExitMsg(95);
	return(gSST_OFF_Entry);

}
/*------------------------------------------------------------------------------------*/

static int gac96_fn(void)
{
	// gSST_ONdata_await02 -> gSST_ONdata_data

	pv_tkGprs_printExitMsg(96);
	counterCtlState = MAXTIMEINASTATE;
	return(gSST_ONdata_data);

}

u16 getTimeToNextDial(void)
{
	return (timerToDial.timer);

}
/*------------------------------------------------------------------------------------*/

void DEBUGgettkGprsState( u08 *state, u08 *sState, u16 *timerA, u16 *timerB)
{
	*state = tkGprs_state;
	*sState = tkGprs_subState;
	*timerA = timer1.timer;
	*timerB = timerToDial.timer;
}
/*------------------------------------------------------------------------------------*/

s08 gprsAllowSleep(void)
{
	// DEBUG
	//return(TRUE);

	// TRUE si puede sleep.
	return(fl_gprsAllowsSleep);
}
/*------------------------------------------------------------------------------------*/

static void pvGPRS_updateTimers(void)
{

u32 tick;

	tick = getSystemTicks();

	// timer1
	if ( timer1.timer > 0) {
		if ( (tick - timer1.tickReference) >= configTICK_RATE_HZ ) {
			timer1.tickReference += configTICK_RATE_HZ;
			timer1.timer--;
		}
	}

	// timerToDial
	if ( timerToDial.timer > 0) {
		if ( (tick - timerToDial.tickReference) >= configTICK_RATE_HZ ) {
			timerToDial.tickReference += configTICK_RATE_HZ;
			timerToDial.timer--;
		}
	}
}
/*------------------------------------------------------------------------------------*/

void SIGNAL_tkGprsRefresh(void)
{

	arranqueFlag = TRUE;
	fl_tkGprsReloadConfigFlag = TRUE;

}
/*------------------------------------------------------------------------------------*/

static void pvGPRS_loadTimer( t_GprsTimerStruct *sTimer, s16 value)
{
	sTimer->timer = value;
	sTimer->tickReference = getSystemTicks();
}
/*------------------------------------------------------------------------------------*/

static s08 gprsRspIs(const char *rsp)
{
s08 retS = FALSE;

	if (strstr( &xUart0RxedCharsBuffer.buffer, rsp) != NULL) {
		retS = TRUE;
	}
	return(retS);
}
/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessPwrMode(void)
{

	switch ( systemVars.pwrMode ) {
	case PWR_CONTINUO:
		// Cambio de CONT a DISC
		if (strstr( &lbuff, "PWRM=DISC") != NULL ) {
			systemVars.pwrMode = PWR_DISCRETO;
			snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig PWRM=DISC\r\n\0"));
			sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
			return(1);
		}
		break;
	case PWR_DISCRETO:
		// Cambio de DISC a CONT
		if (strstr( &lbuff, "PWRM=CONT") != NULL ) {
			systemVars.pwrMode = PWR_CONTINUO;
			snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig PWRM=CONT\r\n\0"));
			sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
			return(1);
		}
	}

	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::WARN NO PWRM parameter\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	return(0);
}

/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessAnalogChannels(u08 channel)
{
//	La linea recibida es del tipo:
//	<h1>INIT_OK:CLOCK=1402251122:TPOLL=600:TDIAL=10300:PWRM=DISC:A0=pA,0,20,0,6:A1=pB,0,20,0,10:A2=pC,0,20,0,10:D0=q0,1.00:D1=q1,1.00</h1>

u08 ret = 0;
char tmp[20];
char *startPtr = NULL;
char *endPtr = NULL;
int length;

	switch (channel) {
	case 0:
		startPtr = strstr(&lbuff, "A0=");
		break;
	case 1:
		startPtr = strstr(&lbuff, "A1=");
		break;
	case 2:
		startPtr = strstr(&lbuff, "A2=");
		break;
	default:
		ret = 0;
		goto quit;
		break;
	}

	if ( startPtr == NULL ) {
		ret = 0;
		goto quit;
	}

	// Incremento para que apunte al str.con el valor..
	startPtr += 3;
	endPtr = strchr( startPtr, ':');
	length = endPtr - startPtr;
	memset(&tmp, '\0', sizeof(tmp));
	memcpy(&tmp, startPtr, (endPtr - startPtr));
	// Tengo el string con la configuracion en tmp.

	// Name
	startPtr = tmp;
	endPtr = strchr( tmp, ',');
	memset(&systemVars.aChName[channel], ' \0', PARAMNAME_LENGTH);
	memcpy(systemVars.aChName[channel], startPtr, (endPtr - startPtr));

	// iMin
	endPtr++;
	systemVars.Imin[channel] = atoi(endPtr);

	// iMax
	endPtr = strchr( endPtr, ',');
	endPtr++;
	systemVars.Imax[channel] = atoi(endPtr);

	// mMin
	endPtr = strchr( endPtr, ',');
	endPtr++;
	systemVars.Mmin[channel] = atoi(endPtr);

	// mMax
	endPtr = strchr( endPtr, ',');
	endPtr++;
	systemVars.Mmax[channel] = atoi(endPtr);

	ret = 1;
	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig A%d channel(%s)\r\n\0"), channel, tmp );
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

quit:

	return(ret);

}
/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessDigitalChannels(u08 channel)
{

//	La linea recibida es del tipo:
//	<h1>INIT_OK:CLOCK=1402251122:TPOLL=600:TDIAL=10300:PWRM=DISC:A0=pA,0,20,0,6:A1=pB,0,20,0,10:A2=pC,0,20,0,10:D0=q0,1.00:D1=q1,1.00</h1>

u08 ret = 0;
char tmp[20];
char *startPtr = NULL;
char *endPtr = NULL;
int length;

	switch (channel) {
	case 0:
		startPtr = strstr(&lbuff, "D0=");
		break;
	case 1:
		startPtr = strstr(&lbuff, "D1=");
		break;
	default:
		ret = 0;
		goto quit;
		break;
	}

	if ( startPtr == NULL ) {
		ret = 0;
		goto quit;
	}

	// Incremento para que apunte al str.con el valor..
	startPtr += 3;
	endPtr = strchr( startPtr, ':');
	length = endPtr - startPtr;
	memset(&tmp, '\0', sizeof(tmp));
	memcpy(&tmp, startPtr, (endPtr - startPtr));
	// Tengo el string con la configuracion en tmp.

	// Name
	startPtr = &tmp;
	endPtr = strchr( tmp, ',');
	memset(systemVars.dChName[channel], ' \0', PARAMNAME_LENGTH);
	memcpy(systemVars.dChName[channel], startPtr, (endPtr - startPtr));

	// magPp
	endPtr++;
	systemVars.magPP[channel] = atof(endPtr);

	ret = 1;
	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig D%d channel(%s)\r\n\0"), channel, tmp );
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

quit:

	return(ret);

}
/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessConsignas(void)
{

//	La linea recibida es del tipo:
//	<h1>INIT_OK:CLOCK=1411200949:CONS=0,02:00,05:00:</h1>

u08 ret = 0;
char *ptr = NULL;

	ptr = strstr(&lbuff, "CONS=");
	if ( ptr == NULL ) {
		ret = 0;
		goto quit;
	}

	// Incremento para que apunte al str.con el valor..
	ptr += 5;
	// Tipo de consigna.
	systemVars.consigna.tipo = atoi(ptr);
	// HH_A
	ptr = strchr( ptr, ',');
	ptr++;
	systemVars.consigna.hh_A = atoi(ptr);
	// MM_A
	ptr = strchr( ptr, ':');
	ptr++;
	systemVars.consigna.mm_A = atoi(ptr);
	// HH_B
	ptr = strchr( ptr, ',');
	ptr++;
	systemVars.consigna.hh_B = atoi(ptr);
	// MM_B
	ptr = strchr( ptr, ':');
	ptr++;
	systemVars.consigna.mm_B = atoi(ptr);

	ret = 1;
	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig Consignas (%d,%02d:%02d,%02d:%02d)\r\n\0"), systemVars.consigna.tipo, systemVars.consigna.hh_A,systemVars.consigna.mm_A,systemVars.consigna.hh_B,systemVars.consigna.mm_B );
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

quit:

	return(ret);

}
/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessTimerDial(void)
{

//	La linea recibida es del tipo: <h1>INIT_OK:CLOCK=1402251122:TPOLL=600:TDIAL=10300:PWRM=DISC:CD=1230:CN=0530</h1>

char *ptr = NULL;
long tdial = 0;
u08 ret = 0;

	ptr = strstr(&lbuff, "TDIAL");
	if ( ptr == NULL ) {
		ret = 0;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::WARN NO DIAL parameter\r\n\0"));
		goto quit;

	}
	// Incremento para que apunte al str.con el valor..
	ptr = ptr + 6;
	tdial = atol(ptr);
	if (tdial < 60 ) {
		tdial = 60;
	}

	if ( systemVars.timerDial != tdial ) {
		systemVars.timerDial = tdial;
		SIGNAL_tkGprsRefresh();
		ret = 1;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig TDIAL(%lu)\r\n\0"), tdial);
	} else {
		ret = 0;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::No reconfig TDIAL\r\n\0"));
	}

quit:
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	return(ret);

}
/*------------------------------------------------------------------------------------*/

static u08 pv_tkGprsProcessTimerPoll(void)
{

//	La linea recibida es del tipo: <h1>INIT_OK:CLOCK=1402251122:TPOLL=600:PWRM=DISC:</h1>

char *ptr = NULL;
s16 tpoll = 0;
u08 ret = 0;

	ptr = strstr(&lbuff, "TPOLL");
	if ( ptr == NULL ) {
		ret = 0;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::WARN NO TPOLL parameter\r\n\0"));
		goto quit;
	}

	// Incremento para que apunte al str.con el valor..
	ptr = ptr + 6;
	tpoll = atoi(ptr);
	if (tpoll < 15 ) {
		tpoll = 15;
	}

	if ( systemVars.timerPoll != tpoll ) {
		systemVars.timerPoll = tpoll;
		SIGNAL_tkDataRefresh();
		ret = 1;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::Reconfig TPOLL(%d)\r\n\0"), tpoll);
	} else {
		ret = 0;
		snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs::No reconfig TPOLL\r\n\0"));
	}

quit:
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	return(ret);
}
/*------------------------------------------------------------------------------------*/

static void pv_tkGprsProcessServerClock(void)
{
/* Extraigo el srv clock del string mandado por el server y si el drift con la hora loca
 * es mayor a 5 minutos, ajusto la hora local
 * La linea recibida es del tipo: <h1>INIT_OK:CLOCK=1402251122:PWRM=DISC:</h1>
 *
 */

char *ptr;
unsigned long serverDate, localDate;
char clkdata[12];
s08 retS = FALSE;
RtcTimeType clock;

	ptr = strstr(&lbuff, "CLOCK");
	if ( ptr == NULL ) {
		return;
	}
	// Incremento para que apunte al str.con la hora.
	ptr = ptr + 6;
	serverDate = atol(ptr);

	rtcGetTime(&clock);
	clock.year -= 2000;
	snprintf_P( &clkdata,CHAR128,PSTR("%02d%02d%02d%02d%02d\0"), clock.year,clock.month, clock.day,clock.hour,clock.min);
	localDate = atol(&clkdata);

	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs:: SrvTime: %lu, LocalTime: %lu \r\n\0"), serverDate, localDate);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	if ( abs( serverDate - localDate ) > 5 ) {
		clkdata[0] = *ptr++; clkdata[1] = *ptr++; clkdata[2] = '\0'; clock.year = atoi(clkdata);
		clkdata[0] = *ptr++; clkdata[1] = *ptr++; clkdata[2] = '\0'; clock.month = atoi(clkdata);
		clkdata[0] = *ptr++; clkdata[1] = *ptr++; clkdata[2] = '\0'; clock.day = atoi(clkdata);
		clkdata[0] = *ptr++; clkdata[1] = *ptr++; clkdata[2] = '\0'; clock.hour = atoi(clkdata);
		clkdata[0] = *ptr++; clkdata[1] = *ptr++; clkdata[2] = '\0'; clock.min = atoi(clkdata);
		clock.sec = 0;

		retS = rtcSetTime(&clock);
		if (retS ) {
			snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs:: Rtc update OK\r\n\0"));
		} else {
			snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGprs:: Rtc update FAIL\r\n\0"));
		}
		sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);
	}

}
/*------------------------------------------------------------------------------------*/

static void pv_tkGprs_openSocket(void)
{
	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);

	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("AT*E2IPO=1,\"%s\",%s\r\n"),systemVars.serverAddress,systemVars.serverPort);
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);

	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);



}
/*------------------------------------------------------------------------------------*/

u08 getModemStatus(void)
{
	// Funcion publica para acceder al estado del modem.

	return( modemStatus );

}
/*------------------------------------------------------------------------------------*/

static s08 qySocketIsOpen(void)
{
s08 socketStatus;

	if ( systemVars.dcd == 0 ) {
		// dcd=ON
		socketStatus = TRUE;
	} else {
		// dcd OFF
		socketStatus = FALSE;
	}

	return(socketStatus);
}
/*------------------------------------------------------------------------------------*/

static void sendInitFrame(void)
{
	// Send Data Frame
	// GET /cgi-bin/sp5K/sp5K.pl?DLGID=SPY001&PASSWD=spymovil123&CTL=68&LINE=20140301,141439,X>0.00,X>0.00,X>0.00,bt>11.92,X>0,XW>0,X>0,XW>0&CONS=1,12:34,04:30 HTTP/1.1
	// Host: www.spymovil.com
	// Connection: close

u16 pos = 0;
u08 i;

	xUartQueueFlush(GPRS_UART);
	// Trasmision: 1r.Parte.
	// HEADER:
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	pos = snprintf_P( pvGPRS_printfBuff,CHAR384,PSTR("GET " ));
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("%s"), systemVars.serverScript );
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("?DLGID=%s"), systemVars.dlgId );
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&PASSWD=%s"), systemVars.passwd );

	// Enviamos la version del firmware.
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&VER=%s"), SP5K_REV );

	// BODY :
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&INIT"));
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&TPOLL=%d"), systemVars.timerPoll);
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&TDIAL=%d"), systemVars.timerDial);
	if ( systemVars.pwrMode == PWR_CONTINUO) { pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&PWRM=CONT")); }
	if ( systemVars.pwrMode == PWR_DISCRETO) { pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&PWRM=DISC")); }
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&CSQ=%d"), systemVars.csq);

	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	// Debug la linea trasmitida.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	debugPrintModemBuffer();


	// Trasmision: 2a.Parte.
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	pos = 0;
	// Configuracion de canales analogicos
	for ( i = 0; i < NRO_CHANNELS; i++) {
		pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&A%d=%s,%d,%d,%d,%d"), i,systemVars.aChName[i],systemVars.Imin[i], systemVars.Imax[i], systemVars.Mmin[i], systemVars.Mmax[i]);
	}

	// Configuracion de canales digitales
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&D0=%s,%.2f"),systemVars.dChName[0],systemVars.magPP[0]);
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&D1=%s,%.2f"),systemVars.dChName[1],systemVars.magPP[1]);

	// Consigna
	pos += snprintf_P( &pvGPRS_printfBuff[pos],CHAR384,PSTR("&CONS=%d,%02d:%02d,%02d:%02d"), systemVars.consigna.tipo, systemVars.consigna.hh_A, systemVars.consigna.mm_A, systemVars.consigna.hh_B, systemVars.consigna.mm_B );

	// TAIL ( No mando el close) :
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR(" HTTP/1.1\n") );
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("Host: www.spymovil.com\n" ));
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\n\n" ));

	// Trasmito
	GPRSrsp = RSP_NONE;
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	// Debug la linea trasmitida.
	pos += snprintf_P( &pvGPRS_printfBuff[pos], CHAR384,PSTR("\r\n\0"));
	sp5K_debugPrintStr( D_GPRS, &pvGPRS_printfBuff);
	debugPrintModemBuffer();

}
/*------------------------------------------------------------------------------------*/

void debugPrintModemBuffer(void)
{
	if ( sp5K_debugControl(D_GPRS) ) {
		if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
			rprintfStr(CMD_UART, &xUart0RxedCharsBuffer.buffer );
			xSemaphoreGive( sem_CmdUart );
		}
	}
}
/*------------------------------------------------------------------------------------*/

void getStatusGprsSM( u08 *state, u08 *subState)
{
	*state = tkGprs_state;
	*subState = tkGprs_subState;

}
/*------------------------------------------------------------------------------------*/

static void  pv_tkGprs_printExitMsg(u08 code)
{
	memset( &pvGPRS_printfBuff, '\0', sizeof(pvGPRS_printfBuff));
	snprintf_P( pvGPRS_printfBuff,CHAR128,PSTR("tkGPRS::gTR%02d\r\n\0"), code);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

}
/*------------------------------------------------------------------------------------*/
static void pv_tkGprs_closeSocket(void)
{
	// Cierro el socket

	GPRSrsp = RSP_NONE;
	memset(pvGPRS_printfBuff, NULL, CHAR384);
	snprintf_P( &pvGPRS_printfBuff,CHAR384,PSTR("+++AT\r\n"));
	xUartQueueFlush(GPRS_UART);
	rprintfStr(GPRS_UART, &pvGPRS_printfBuff);
	sp5K_debugPrintStr( D_GPRS, pvGPRS_printfBuff);

	vTaskDelay( (portTickType)( 500 / portTICK_RATE_MS ) );

}
/*------------------------------------------------------------------------------------*/

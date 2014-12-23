/*
 * sp5K.h
 *
 * Created on: 27/12/2013
 *      Author: root
 */

#ifndef SP5K_H_
#define SP5K_H_

#include "sp5KFRTOS.h"

// DEFINICION DEL TIPO DE SISTEMA
//----------------------------------------------------------------------------
//#define MEMINFO
//#define EE_INTERNA
//#define CHANNELS_8

#define SP5K_REV "2.0.10"
#define SP5K_DATE "@ 20141212"

//----------------------------------------------------------------------------
// Nro de canales analogicos: 8 ( UTE) o 3 ( OSE ).

#ifdef CHANNELS_8
	#define NRO_CHANNELS	8
#else
	#define CHANNELS_3
	#define NRO_CHANNELS	3
#endif

//----------------------------------------------------------------------------
// Manejo de la memoria: INTERNA EE ( testing )  o EXTERNA ( 24C256 )
#ifdef EE_INTERNA
	#define EE_TYPE "INEE"
#else
	#define EE_EXTERNA
	#define EE_TYPE "EXEE"
#endif

#define SP5K_MODELO "sp5K HW:avr1284P R1.0"
#define SP5K_VERSION "FW:FRTOS"

//----------------------------------------------------------------------------
// Compilacion con informacion de memoria de tareas.

#ifdef MEMINFO
	#define FTYPE "M"
#else
	#define FTYPE "P"
#endif

#define LOGTAG '.'
//------------------------------------------------------------------------------------
// TASKS
/* Stack de las tareas */
#define tkCmd_STACK_SIZE		512
#define tkGprs_STACK_SIZE		512
#define tkDigitalIn_STACK_SIZE	256
#define tkData_STACK_SIZE		512
#define tkControl_STACK_SIZE	320

/* Prioridades de las tareas */
#define tkCmd_TASK_PRIORITY	 ( tskIDLE_PRIORITY + 1 )
#define tkDigitalIn_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define tkData_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define tkControl_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define tkGprs_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )

/* Prototipos de tareas */
void tkCmd(void * pvParameters);
void tkDigitalIn(void * pvParameters);
void tkData(void * pvParameters);
void tkControl(void * pvParameters);
void tkGprs(void * pvParameters);

xTaskHandle h_tkGprs,h_tkData, h_tkDigitalIn, h_tkControl;

// Semaforos sp5K
xSemaphoreHandle sem_systemVars;
#define MSTOTAKESYSTEMVARSSEMPH (( portTickType ) 10 )

xSemaphoreHandle sem_digitalIn;

xSemaphoreHandle sem_BD;
#define MSTOTAKEBDSEMPH (( portTickType ) 10 )

long tkControl_uxHighWaterMark, mintkControl_uxHighWaterMark;
long tkDigitalIn_uxHighWaterMark;
long tkCmd_uxHighWaterMark;
long tkData_uxHighWaterMark;
long tkGprs_uxHighWaterMark;

//------------------------------------------------------------------------------------

typedef enum { WK_IDLE = 0, WK_NORMAL = 1, WK_SERVICE = 2, WK_MONITOR = 3 } t_wrkMode;
typedef enum { PWR_CONTINUO = 0, PWR_DISCRETO = 1 } t_pwrMode;
typedef enum { CONS_NONE = 0, CONS_DOBLE = 1 } t_consigna;

//------------------------------------------------------------------------------------

#define DLGID_LENGTH		12
#define APN_LENGTH			32
#define PORT_LENGTH			7
#define IP_LENGTH			24
#define SCRIPT_LENGTH		64
#define PASSWD_LENGTH		15
#define PARAMNAME_LENGTH	5

#ifdef MEMINFO
long HE_I2Cbus;
long HE_CmdUart;
long HE_GprsUart;
long HE_digitalIn;
long HE_systemVars;
long HE_BD;

long HE_tkCmd;
long HE_tkDigitalIn;
long HE_tkData;
long HE_tkControl;
long HE_tkGprs;

long HE_Uart0TxQueue;
long HE_Uart1RxQueue;
long HE_Uart1TxQueue;
#endif

typedef struct {
	u08 tipo;	// 0-none, 1-doble
	u08 hh_A;
	u08 mm_A;
	u08 hh_B;
	u08 mm_B;

} consignaType;

typedef struct {
	// Variables de trabajo.
	// Tamanio: 332 bytes

	u08  initByte;
	// Status
	s08 initStatus;

	char dlgId[DLGID_LENGTH];

	char apn[APN_LENGTH];
	char serverPort[PORT_LENGTH];
	char serverAddress[IP_LENGTH];
	char serverIp[IP_LENGTH];
	char dlgIp[IP_LENGTH];
	char serverScript[SCRIPT_LENGTH];
	char passwd[PASSWD_LENGTH];

	u08 csq;
	u08 dbm;

	u08 dcd;
	u08 ri;
	u08 termsw;

	t_pwrMode pwrMode;
	t_wrkMode wrkMode;
	u16 timerPoll;
	u32 timerDial;

	u08 logLevel;		// Nivel de info que presentamos en display.
	u08 debugLevel;		// Indica que funciones debugear.

	// Nombre de los canales
	char aChName[NRO_CHANNELS][PARAMNAME_LENGTH];
	char dChName[2][PARAMNAME_LENGTH];

	// Configuracion de Canales analogicos
	u08 Imin[NRO_CHANNELS];				// Coeficientes de conversion de I->magnitud (presion)
	u08 Imax[NRO_CHANNELS];
	u08 Mmin[NRO_CHANNELS];
	u16 Mmax[NRO_CHANNELS];
	double offmmin[NRO_CHANNELS];		// offset del ADC en mmin.
	double offmmax[NRO_CHANNELS];		// offset del ADC en mmax.

	// Configuracion de canales digitales
	double magPP[2];

	consignaType consigna;

} systemVarsType;


typedef struct {

	// size = 7+4+4+4+3*4+1 = 32 bytes
	RtcTimeType rtc;	// 7
	float din0_pCount;	// 4
	float din1_pCount;	// 4
	double batt;		// 4 --> 19b
	double analogIn[NRO_CHANNELS];// 12
	u08 checksum;

} frameDataType;

#if ( defined (EE_INTERNA) )
	#define RECDSIZE	42
#else
	#define RECDSIZE	64
#endif

union UframeDataType {

	frameDataType frame;
	char chrFrame [RECDSIZE];

};

systemVarsType systemVars;

//------------------------------------------------------------------------------------

#define OK		TRUE
#define FAIL	FALSE

#define EEADDR_START_PARAMS		0

//------------------------------------------------------------------------------------
long freeRam ();
long memDataStart();
long memDataEnd ();
long memBssStart();
long memBssEnd ();
long memHeapStart ();
long memHeapEnd();
long memRamStart();
long memRamEnd();

s08 saveSystemParamsInEE( systemVarsType *sVars );
void loadDefaults( systemVarsType *sVars);

s08 setParamTimerPoll(u08 *s);
s08 setParamWrkMode(u08 *s0, u08 *s1);
s08 setParamPwrMode(u08 *s);
//s08 setParamMonitor(u08 *s);
s08 setParamDebugLevel(u08 *s);
s08 setParamLogLevel(u08 *s);

s08 setParamImin(u08 *s0, u08 *s1);
s08 setParamImax(u08 *s0, u08 *s1);
s08 setParamMmin(u08 *s0, u08 *s1);
s08 setParamMmax(u08 *s0, u08 *s1);
s08 setParamOffset(u08 tipo, u08 *s0, u08 *s1);
s08 setParamAname(u08 *s0, u08 *name);
s08 setParamDname(u08 *s0, u08 *name);
s08 setParamAnalogChannels(u08 *s);
s08 setParamMagP(u08 *s0, u08 *s1);
s08 setParamConsigna(u08 *s0, u08 *s1);

void sp5K_helpFunction(void);
void sp5K_resetFunction(void);
void sp5K_readFunction(void);
void sp5K_writeFunction(void);
void SP5K_statusFunction(void);
void sp5K_stackFunction(void);

void initBlueTooth(void);

typedef enum { LOG_NONE = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ALERT = 3 } t_logLevel;
void logPrintStr(t_logLevel logLevel, char *buff);

/*------------------------------------------------------------------------------------*/
// CONTROL

#define WDG_DATA		0x01
#define WDG_DIGITAL		0x02
#define WDG_CMD			0x04
#define WDG_CONTROL		0x08
#define WDG_GPRS		0x10

// secs. en los que expira el timer de la terminal activa.
#define TIMEOUT_TERMINAL	90

s16 houseKeepingTimer;
#define HKERESETTIME 1440

void clearWdg( u08 wdgId );
void restartTimerTerminal(void);
s08 terminalAllowSleep(void);
void SIGNAL_refreshConsignas(void);

/*------------------------------------------------------------------------------------*/
// DATA
void SIGNAL_tkDataRefresh(void);
void SIGNAL_tkDataReadFrame(void);
u16 getTimeToNextPoll(void);
void getDataFrame( frameDataType* rdRec );
void readDigitalCounters( float *count0, float *count1, s08 resetCounters );
void getStatusDataSM( u08 *state);

#define TIME_SLOT	15

/*------------------------------------------------------------------------------------*/
// VALVULAS
s08 setValvesPhase(char phase, u08 chip, u08 value);
s08 setValvesEnable(char phase, u08 chip, u08 value);
s08 setValvesReset(u08 value);
s08 setValvesSleep(u08 value);
s08 setValvesPulse(  char phase, u08 chip, u08 value, u16 sleepTime );
s08 setValvesOpen(char phase, u08 chip );
s08 setValvesClose(char phase, u08 chip );
s08 setConsignaDia(void);
s08 setConsignaNoche(void);

/*------------------------------------------------------------------------------------*/
// BD
// Direcciones de la EEprom.
#ifdef EE_INTERNA
	#define EEADDR_START_DATA		512
	#define EESIZE  				4
#endif

#ifdef EE_EXTERNA
	#define EEADDR_START_DATA		0
	#define EESIZE  				32   // Tamanio en KB de la eeprom externa.
#endif

#define BD_EMPTY_MARK 			'*'		// Caracter usado para marcar los registro vacios.

void BD_init(void);
u16 BD_getMaxRcds(void);
u16 BD_getRDptr(void);
u16 BD_getWRptr(void);
u16 BD_getRcsUsed(void);
u16 BD_getRcsFree(void);
void BD_drop(void);
u08 BD_get( frameDataType *rdDataRec, u16 index );
s08 BD_delete(s16 rcdNro );
s08 BD_put( frameDataType *wrDataRec);
u08 pvBD_checksum( char *buff, u08 limit  );

/*------------------------------------------------------------------------------------*/
// GPRS

typedef enum { GPRS_PRENDIDO = 0, GPRS_APAGADO = 1, GPRS_IDLE = 2 } t_gprsStatus;
typedef enum { M_OFF = 0, M_OFF_IDLE = 1, M_ON_CONFIG = 2, M_ON_READY = 3 } t_modemStatus;

void SIGNAL_tkGprsRefresh(void);
u16 getTimeToNextDial(void);
u08 getModemStatus(void);
s08 gprsAllowSleep(void);

void DEBUGgettkGprsState( u08 *state, u08 *sState, u16 *timerA, u16 *timerB);
void getStatusGprsSM( u08 *state, u08 *subState);

/*------------------------------------------------------------------------------------*/
// LEDS placa superior

#define LED_KA_PORT		PORTD
#define LED_KA_PIN		PIND
#define LED_KA_BIT		6
#define LED_KA_DDR		DDRD
#define LED_KA_MASK		0x40

#define LED_MODEM_PORT		PORTC
#define LED_MODEM_PIN		PINC
#define LED_MODEM_BIT		3
#define LED_MODEM_DDR		DDRC
#define LED_MODEM_MASK		0x04

#endif /* SP5K_H_ */

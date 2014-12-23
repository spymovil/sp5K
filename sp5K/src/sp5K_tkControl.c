/*
 * sp5K_tkControl.c
 *
 *  Created on: 24/01/2014
 *      Author: root
 */

#include "sp5K.h"

#define TERM_ON		TRUE
#define TERM_OFF	FALSE

u16 timerTerminal =  TIMEOUT_TERMINAL;
u08 taskWdg;
s08 scheduleMinutes, schedule15Minutes;

static s08 f_terminalPwrStatus = TERM_ON;		// La pwrTerm se activo en mcp_init.
static s08 f_flashLed = TRUE;
static s08 f_terminalAllowSleep = FALSE;
static s08 fl_reloadConsignasFlag = FALSE;

void pv_checkWatchdogs(void);
void pv_checkTerminalLedSleep(void);
void pv_checkResetHouseKeeping(void);
void pv_checkConsignas(void);
void pv_initConsignas( u08 tipo );

void prenderLedModem(void);
void apagarLedModem(void);
void prenderLedKA(void);
void apagarLedKA(void);

struct {
     u16 dayMinutes;
     u16 timeConsigna_A;
     u16 timeConsigna_B;

} consingasClock;

/*------------------------------------------------------------------------------------*/
void tkControl(void * pvParameters)
{

( void ) pvParameters;

s08 initStatus = FALSE;

	while (! initStatus ) {
		if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
			initStatus = systemVars.initStatus;
			xSemaphoreGive( sem_systemVars );
		}
		vTaskDelay( (portTickType)(100 / portTICK_RATE_MS) );
	}

	vTaskDelay((portTickType)(2000 / portTICK_RATE_MS));
	logPrintStr(LOG_NONE, "tkControl::Task starting...\r\n");

	// Prendo el watchdog.
	wdt_enable(WDTO_8S);
	wdt_reset();
	// Inicializo los watchdogs de c/tarea.
	taskWdg = 0x0;

	scheduleMinutes = 60;
	schedule15Minutes = 15;

	pv_initConsignas(1);	// Full init

	houseKeepingTimer = HKERESETTIME;

	//-----------------------------------
	// TESTING:: Apago el modem
	// DEBUG
	//MCP_setGprsPwr(0);
	//vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );
	//MCP_setGprsPwr(1);
	//-----------------------------------

	for( ;; )
	{
		// El loop se ejecuta c/segundo.
		// Para medir el uso del stack
		tkControl_uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		if ( f_flashLed ) {
			MCP_setLed (1);	// Prendo.
			prenderLedKA();
			if ( !gprsAllowSleep() ) { prenderLedModem(); }

		}
		vTaskDelay( (portTickType)( 50 / portTICK_RATE_MS ) );
		MCP_setLed (0);		// siempre lo apago.
		apagarLedKA();
		apagarLedModem();


		vTaskDelay( (portTickType)( 950 / portTICK_RATE_MS ) );

		// Se ejecutan 1 vez por segundo.
		// Apago el watchdog
		clearWdg(WDG_CONTROL);

		pv_checkWatchdogs();
		pv_checkTerminalLedSleep();

		// Tareas que se ejecutan 1 vez por minuto
		if ( scheduleMinutes-- <= 0 ) {
			scheduleMinutes = 60;
			pv_checkResetHouseKeeping();

#ifdef CHANNELS_3
			pv_checkConsignas();
#endif

		}
	}

}
/*------------------------------------------------------------------------------------*/
void pv_checkTerminalLedSleep(void)
{
	// Monitorea el pin al que esta conectado el TERMSW.
	MCP_queryTermPwrSw(&systemVars.termsw);

	switch (systemVars.wrkMode) {

		case WK_MONITOR:
			timerTerminal = TIMEOUT_TERMINAL;  	// restartTimerTerminal(); No apaga la terminal.
			f_flashLed = TRUE;					// Flashea el led
			f_terminalAllowSleep = FALSE; 		// No puede entrar en modo sleep por la terminal
			break;

		case WK_SERVICE:
			timerTerminal = TIMEOUT_TERMINAL;  	// restartTimerTerminal(); No apaga la terminal.
			f_flashLed = TRUE;					// Flashea el led
			f_terminalAllowSleep = FALSE; 		// No puede entrar en modo sleep por la terminal
			break;

		case WK_NORMAL:
			switch (systemVars.pwrMode) {

				case PWR_CONTINUO:
					timerTerminal = TIMEOUT_TERMINAL;  	//restartTimerTerminal(); No apaga la terminal.
					f_flashLed = TRUE;					// Flashea el led
					f_terminalAllowSleep = FALSE; 		// No puede entrar en modo sleep por la terminal
					break;

				case PWR_DISCRETO:
				default:
					if (( systemVars.termsw == 0 ) && (timerTerminal > 0 )) {	// Si el timer no expiro aun lo decremento.
						timerTerminal--;
					}

					// Prendo ??: La terminal debe estar apagada y llegar un flanco de subida del termsw
					if ( ( f_terminalPwrStatus == TERM_OFF) && ( systemVars.termsw == 1) ) {
						MCP_setTermPwr( 1 );				// Prendo fisicamente la fuente
						f_terminalPwrStatus = TERM_ON;		// Indico que la terminal esta prendida
						timerTerminal = TIMEOUT_TERMINAL;  	// restartTimerTerminal(); inicio el timer
						f_flashLed = TRUE;
						f_terminalAllowSleep = FALSE;		// no puedo entrar en modo sleep.
						//
						vTaskDelay( (portTickType)( 100 / portTICK_RATE_MS ) );
						initBlueTooth();

						logPrintStr(LOG_INFO, "tkControl::Terminal PWR ON...\r\n");
						vTaskDelay( (portTickType)( 50 / portTICK_RATE_MS ) );

					}

					// Apago ??: La terminal debe estar prendida y haber expirado el timer de la terminal.
					if ( ( f_terminalPwrStatus == TERM_ON) && ( timerTerminal == 0) ) {
						logPrintStr(LOG_INFO, "tkControl::Terminal PWR OFF...\r\n");
						vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ) );
						MCP_setTermPwr( 0 );				// apago fisicamente la fuente
						f_terminalPwrStatus = TERM_OFF;		// Indico que apague la terminal.
						f_flashLed = FALSE;					// no flasheo mas
						f_terminalAllowSleep = TRUE;		// si puedo entrar en modo sleep
					}
					break;
			}
			break;

		case WK_IDLE:
			break;
		default:
			break;
	}
}
/*------------------------------------------------------------------------------------*/
void pv_checkWatchdogs(void)
{
/*  Si todos los bit wdg de tareas estan 0, reseteo el wdg del sistema y armo
	nuevamente los wdg individuales poniendolos en 1.
	Si en 8secs. no se pusieron todos en 0, va a expirar y se resetea el sistema
*/

	if ( taskWdg == 0 ) {
		wdt_reset();
		// Armo los wdg individuales.
		// DEBUG
		taskWdg = WDG_DATA+WDG_DIGITAL+WDG_CMD+WDG_CONTROL+WDG_GPRS; // 0001 1111 = 0x1F = 31d
		//taskWdg = WDG_DATA+WDG_DIGITAL+WDG_CMD+WDG_CONTROL;
		//taskWdg = 0;
	}
}
/*------------------------------------------------------------------------------------*/
void pv_checkResetHouseKeeping(void)
{
	// Funcion que a las 12 hs de estar corriendo resetea al sistema
	// En caso que algo halla quedado trancado, saca al sistema de este estado.

	houseKeepingTimer--;

	if (houseKeepingTimer <= 0 ) {

		// Reseteo
		// Capturo el semaforo de la memoria para asegurarme que nadie este accediendo
		if (xSemaphoreTake( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) { }

		rprintfStr(CMD_UART, "tkControl::Housekeeping reset...\r\n");
		vTaskDelay( (portTickType)( 50 / portTICK_RATE_MS ) );
		wdt_reset();
		wdt_enable(WDTO_15MS);
		while(1) {}
	}

}
/*------------------------------------------------------------------------------------*/
void restartTimerTerminal(void)
{
	// Reinicia el timer de la terminal a 90s
	timerTerminal = TIMEOUT_TERMINAL;

}
/*------------------------------------------------------------------------------------*/
void clearWdg( u08 wdgId )
{
	// Pone el correspondiente bit del wdg en 0.
	taskWdg &= ~wdgId ;

}
/*------------------------------------------------------------------------------------*/
s08 terminalAllowSleep(void)
{
	return(f_terminalAllowSleep);
}
/*------------------------------------------------------------------------------------*/
void prenderLedModem(void)
{
	cbi(LED_MODEM_PORT, LED_MODEM_BIT);
}
//------------------------------------------------------------------------------------
void apagarLedModem(void)
{
	sbi(LED_MODEM_PORT, LED_MODEM_BIT);
}
//------------------------------------------------------------------------------------
void prenderLedKA(void)
{
	cbi(LED_KA_PORT, LED_KA_BIT);
}
//------------------------------------------------------------------------------------
void apagarLedKA(void)
{
	sbi(LED_KA_PORT, LED_KA_BIT);
}
//------------------------------------------------------------------------------------
void SIGNAL_refreshConsignas(void)
{
// Se manda cuando se reconfiguran las consignas.

	fl_reloadConsignasFlag = TRUE;

}
/*------------------------------------------------------------------------------------*/
void pv_initConsignas(u08 tipo )
{
	// Leo los parametros de las consignas A y B, y de acuerdo a la hora local
	// seteo la consigna que corresponda.
	// Tipo == 0: solo inicializa los timers
	//      == 1: (full) inicializa las fases

RtcTimeType rtcDateTime;

	if ( systemVars.consigna.tipo == CONS_NONE ) {
		return;
	}

	consingasClock.timeConsigna_A = ( 60 * systemVars.consigna.hh_A ) + systemVars.consigna.mm_A;
	consingasClock.timeConsigna_B = ( 60 * systemVars.consigna.hh_B ) + systemVars.consigna.mm_B;
	rtcGetTime(&rtcDateTime);
	consingasClock.dayMinutes = ( 60 * rtcDateTime.hour ) + rtcDateTime.min;

	//rprintf( CMD_UART, "DEBUG: tcA=%d, tcB=%d, T=%d\r\n",consingasClock.timeConsigna_A, consingasClock.timeConsigna_B, consingasClock.dayMinutes );

	if ( tipo == 0 ) { return; }	// Only timers

	// Seteo las consignas.
	if ( ( consingasClock.dayMinutes <= consingasClock.timeConsigna_A) || ( consingasClock.dayMinutes >= consingasClock.timeConsigna_B) ) {
		// CONSIGNA NOCHE
		setConsignaNoche();
		logPrintStr(LOG_NONE, "Init Consigna nocturna\r\n\0");
	} else {
		// CONSIGNA DIA
		setConsignaDia();
		logPrintStr(LOG_NONE, "Init Consigna diurna\r\n\0");
	}

}
/*------------------------------------------------------------------------------------*/
void pv_checkConsignas(void)
{
	if ( systemVars.consigna.tipo == CONS_NONE ) {
		return;
	}

	// Avanzo el pseudo rtc de los minutos de las consignas
	if ( ++consingasClock.dayMinutes >= 1440 ) {
		consingasClock.dayMinutes = 0;
	}

	// 1 vez por hora, chequeo la hora por si cambio
	if ( consingasClock.dayMinutes % 60 == 0 ) {
		pv_initConsignas(0); // init only timers
	}

	// Si cambiaron la configuracion de las consignas, renicio el subsistema.
	if ( fl_reloadConsignasFlag == TRUE ) {
		fl_reloadConsignasFlag = FALSE;
		pv_initConsignas(1);	// Full init
	}

	// Veo si debo aplicar las consignas
	if ( consingasClock.dayMinutes == consingasClock.timeConsigna_A ) {
		// CONSIGNA DIA
		setConsignaDia();
		logPrintStr(LOG_NONE, "Consigna diurna\r\n\0");

	}

	if ( consingasClock.dayMinutes == consingasClock.timeConsigna_B ) {
		// CONSIGNA NOCHE
		setConsignaNoche();
		logPrintStr(LOG_NONE, "Consigna nocturna\r\n\0");
	}
}
/*------------------------------------------------------------------------------------*/

/*
 * sp5KFRTOS_utils.c
 *
 *  Created on: 09/12/2013
 *      Author: root
 *
 *  Funciones generales de libreria modificadas para usarse con FRTOS.
 *  Debemos usar el semaforo de acceso al CMD_UART en todas las funciones.
 *
 */

#include "sp5KFRTOS.h"
#include "sp5K.h"

#define LOCALRTCBUFFER	15
static char localRtcBuffer[LOCALRTCBUFFER];

/*------------------------------------------------------------------------------------*/

 void makeargv(void)
{
// A partir de la linea de comando, genera un array de punteros a c/token
//
char *token = NULL;
char parseDelimiters[] = " ";
int i = 0;

	// Copio la lista de argumentos a un espacio temporal.
//	memset(&cmdLineArg, NULL , CMDLINE_BUFFERSIZE);
//	strncpy(&cmdLineArg,cmdlineGetArgStr(0), 30);
//	memcpy(&cmdLineArg,SP5K_GetCmdlinePtr(),CMDLINE_BUFFERSIZE);
//	SP5K_GetLineArg( &cmdLineArg, 30);
	memset(argv, NULL, 10);

	// Genero los tokens delimitados por ' '.
	token = strtok(SP5K_CmdlineBuffer, parseDelimiters);
	argv[i++] = token;
	while ( (token = strtok(NULL, parseDelimiters)) != NULL ) {
		argv[i++] = token;
		if (i == 9) break;
	}

	// Imprimo ( debug )
//	for (i = 0; i < 10 ; i++ ) {
//		if (argv[i] == NULL) break;
//		rprintf(CMD_UART,"[%d]->",i); rprintfStr(CMD_UART, argv[i]); rprintfCRLF(CMD_UART);
//	}

}
//------------------------------------------------------------------------------------

void sp5K_debugPrintStr(u08 modo, char *buff)
{

	long rticks;

	// Si no esta habilitado el debug salgo
	if ( !sp5K_debugControl (modo) )
		return;

	rticks = getSystemTicks();

	memset( &localRtcBuffer, '\0', LOCALRTCBUFFER);
	snprintf_P( localRtcBuffer,LOCALRTCBUFFER,PSTR(".%02d:%02d:%02d.%03d \0"), localRtc.hour, localRtc.min, localRtc.sec, localRtc.tick);

 	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
 		//rprintfChar(CMD_UART, LOGTAG );
 		//rprintfNum(CMD_UART, 10, 6, FALSE, '0', rticks);
 		//rprintf(CMD_UART,"%02d:%02d:%02d.%03d ", localRtc.hour, localRtc.min, localRtc.sec, localRtc.tick);
 		//rprintfChar(CMD_UART, ' ');
 		rprintfStr(CMD_UART, localRtcBuffer );
 		rprintfStr(CMD_UART, buff );
 		xSemaphoreGive( sem_CmdUart);
 	}

 }
 //------------------------------------------------------------------------------------

 void sp5K_printStr(u08 *buff)
 {
 	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
 		rprintfStr(CMD_UART, buff );
 		xSemaphoreGive( sem_CmdUart);
 	}
 }
 /*------------------------------------------------------------------------------------*/

void sp5K_uartsEnable(void)
{
	// Habilita el pin que activa el buffer L365
	cbi(UARTCTL_PORT, UARTCTL);

}
//------------------------------------------------------------------------------------

void sp5KFRTOS_sleep(int secs)
{

	while (secs > 0) {
	//	rprintfChar(CMD_UART, '.');
		vTaskDelay( (portTickType)( 1000 / portTICK_RATE_MS ));
		//_delay_ms(1000);
		secs--;
	}
	//rprintfCRLF(CMD_UART);
}
//------------------------------------------------------------------------------------

void sp5KFRTOS_clearScreen(void)
{
	if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
		vt100ClearScreen(CMD_UART);
		xSemaphoreGive( sem_CmdUart );
	}
}
//------------------------------------------------------------------------------------
const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}
//------------------------------------------------------------------------------------
void initDebugRtc(void)
{

RtcTimeType rtcDateTime;

	rtcGetTime(&rtcDateTime);
	localRtc.tick = 0;
	localRtc.sec = rtcDateTime.sec;
	localRtc.min = rtcDateTime.min;
	localRtc.hour = rtcDateTime.hour;

}
/*------------------------------------------------------------------------------------*/

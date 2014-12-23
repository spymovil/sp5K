/*
 * sp5K_bd.c
 *
 *  Created on: 27/01/2014
 *      Author: root
 *
 * Rutinas de manejo del sistema de almacenamiento de datos
 *
 * La primer alternativa que pensamos es tener una posicion fija de la EE para guardar los
 * punteros. Si tomamos un registro c/5 minutos, son 105000 actualizaciones al anio con lo
 * que llegamos al maximo permitido de endurance.
 * La segunda alternativa es tener una estructura autoreferenciada de unos 200 registos.
 * De este modo, al anio reescribimos c/byte solo 525 veces y el endurance llegamos en 200 anios.
 * Lo importante en este tipo de estructura es poder determinar al inicio cual es el puntero
 * de WR y cual es el de RD
 *
 * Funciones:
 * BDinit(): Inicializa los punteros
 * BDgetWRptr()
 * BDgetRDprt()
 * BDgetRcsUsed()
 * BDgetRcsFree()
 * BDdrop(): Borra toda la memoria.
 * BDpush(*recd) : Almacena el registro en la ultima posicion.
 *
 * BDshift(*recd): Retorna el primer elemento de la BD pero NO lo borra.
 * BDdelete(): Borra el primer elemento de la BD.
 *
 * --------------------------------------------------------------------------------------------------
 *  rdPtr apunta al primer registro ocupado.
 *  wrPtr apunta al primer registro libre.
 *
 * Push: escribo el registro indicado por el puntero wrPtr y lo avanzo
 * Shift: leo del registro indicado por el puntero rdPtr y lo avanzo
 *
 * Un registro vacio se indica por el primer byte de valor '*'.
 *
 * Inicializacion de la memoria:
 * 1) Memoria vacia: Todos lo registros estan con una marca de  '*'.
 *    En este caso popPtr = pushPtr = 0.
 *
 * 2) Memoria con datos:
 * El puntero pushPtr lo determina la transicion datos->vacio.
 * El puntero popPtr lo determina la trasnsicion vacio->datos.
 *
 * Siempre debe haber al menos un registro libre en el buffer.
 *
 *
 */

#include "sp5K.h"

//------------------------------------------------------------------------------------
//
typedef struct {
	u16 head;
	u16 tail;
	u16 count;
	u16 maxRecs;
} bd_struct;

bd_struct bd;

union UframeDataType bdFrame;

static char pv_BDprintfBuff[CHAR128];

//----------------------------------------------------------------------------------

void BD_init(void)
{
	/* Funcion que inicializa los punteros de la EE.
	 * Recorre toda la memoria buscando transiciones.
	 * DATA->VACIO: wrPtr
	 * VACIO->DATA: rdPtr
	 */

int i,j;
u16 eeAddress;
char dataA, dataB;

	// Asumimos que la memoria esta vacia
	memset(&bd, 0, sizeof(bd));

	// Determino la cantidad de registros.
	bd.maxRecs = (u16)(EESIZE * 1024 - EEADDR_START_DATA );
	bd.maxRecs = (u16)( bd.maxRecs / RECDSIZE);

	// Recorro la memoria
	if (xSemaphoreTake( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) {

		for ( i=0; i < bd.maxRecs; i++) {
			// Leo el primer byte del registro
			eeAddress = EEADDR_START_DATA + i * RECDSIZE;
#ifdef EE_INTERNA
			dataA = eeprom_read_byte ( eeAddress );
#endif
#ifdef EE_EXTERNA
			EE_read( eeAddress,1, &dataA );
#endif
			// Leo el siguiente registro
			j=i+1;
			if (j == bd.maxRecs) { j=0; }
			eeAddress = EEADDR_START_DATA + j * RECDSIZE;
#ifdef EE_INTERNA
			dataB = eeprom_read_byte ( eeAddress );
#endif
#ifdef EE_EXTERNA
			EE_read( eeAddress,1, &dataB);
#endif
			// Busco transiciones
			// VACIO->DATA: popPtr.( primer registro a leer)
			if ( (dataA == BD_EMPTY_MARK ) && (dataB != BD_EMPTY_MARK ) ) {
				bd.tail = j;
			}
			// DATA->VACIO: pushPtr. ( comienzo de registros libres para escritura)
			// El ultimo registro valido es el i.
			if ( (dataA != BD_EMPTY_MARK ) && (dataB == BD_EMPTY_MARK ) ) {
				bd.head = j;
			}

			// Imprimo realimentacion c/32 recs.
			if ( (i % 32) == 0 ) {
				clearWdg(WDG_CMD);
				logPrintStr(LOG_NONE, "." );
			}
		}

		// Calculo cuantos registros tengo.
		if ( bd.head > bd.tail) {
			bd.count = (bd.head - bd.tail);
		}
		if (bd.head == bd.tail) {
			bd.count = 0;
		}
		if (bd.head < bd.tail) {
			bd.count = ( bd.maxRecs - bd.tail + bd.head);
		}

		xSemaphoreGive( sem_BD );
	}

}
//----------------------------------------------------------------------------------

u16 BD_getMaxRcds(void)
{
	return( bd.maxRecs );
}
//----------------------------------------------------------------------------------

u16 BD_getRcsFree(void)
{
	return( bd.maxRecs - bd.count );
}
//----------------------------------------------------------------------------------

u16 BD_getRcsUsed(void)
{
	return (bd.count);
}
//----------------------------------------------------------------------------------

u16 BD_getWRptr(void)
{
	return ( bd.head );
}
//----------------------------------------------------------------------------------

u16 BD_getRDptr(void)
{
	return ( bd.tail );
}
//----------------------------------------------------------------------------------

void BD_drop(void)
{
// Borro toda la memoria grabando todos los registros con una marca.

u16 i;
u16 eeAddress;
s08 retS;

	logPrintStr(LOG_INFO, "Erasing all data memory !!\r\n" );
	snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("Borrando %d sectores\r\n\0"), bd.maxRecs );
	logPrintStr(LOG_NONE, pv_BDprintfBuff);

	// Borra la memoria marcando c/registro como libre
	if ( xSemaphoreTake ( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) {

		retS = FALSE;
		memset( &bdFrame.chrFrame, BD_EMPTY_MARK, RECDSIZE );
		for ( i=0; i < bd.maxRecs; i++) {
			eeAddress = EEADDR_START_DATA + i * RECDSIZE;
			// Lo borro
#ifdef EE_INTERNA
			retS = TRUE;
			eeprom_write_block (bdFrame.chrFrame, eeAddress , RECDSIZE);
#endif
#ifdef EE_EXTERNA
			retS = EE_pageWrite( eeAddress , RECDSIZE , bdFrame.chrFrame);
#endif
			vTaskDelay( (portTickType)( 50 / portTICK_RATE_MS ) );
			if ( !retS ) {
				snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BDdrop ERROR (%d/%d).\r\n\0"), i, eeAddress);
				logPrintStr(LOG_NONE, pv_BDprintfBuff);
			}

			// Imprimo realimentacion c/32 recs.
			if ( (i % 32) == 0 ) {
				if (xSemaphoreTake( sem_CmdUart, MSTOTAKECMDUARTSEMPH ) == pdTRUE ) {
					rprintfChar(CMD_UART, '.');
					xSemaphoreGive( sem_CmdUart);
				}
			}
		}

		// Actualiza los punteros.
		bd.head = 0;
		bd.tail = 0;
		bd.count = 0;

		xSemaphoreGive( sem_BD );
	}

	logPrintStr(LOG_INFO, "\r\nEE BD clean\r\n");

}
//----------------------------------------------------------------------------------

s08 BD_put( frameDataType *wrDataRec)
{

	/* Guardo un registro en la EE en modo circular.
	 * Guardo en la posicion indicada por el puntero wrPtr y lo avanzo
	 * Marco la nueva posicion como vacia ya que siempre debe haber un slot
	 * vacio para que al inicio se pueda detectar el ppio. y el fin del buffer.
	 *
	 */

u16 eeAddress;
s08 retS = FALSE;
u08 checksum;

	memset( &pv_BDprintfBuff, '\0', sizeof(pv_BDprintfBuff));

	if (xSemaphoreTake( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) {

		memset( &bdFrame.chrFrame, '\0', RECDSIZE );
		memcpy (&bdFrame.frame, wrDataRec, sizeof(frameDataType) );
		bdFrame.frame.checksum = '\0';
		checksum = pvBD_checksum( bdFrame.chrFrame,  ( sizeof(frameDataType) - 1) );
		bdFrame.frame.checksum = checksum;

		// Pass1:Calculo la direccion fisica donde comenzar a grabar
		eeAddress = EEADDR_START_DATA + bd.head * RECDSIZE;
		// Pass2:Grabo el registro ( OJO que cambia la cantidad de bytes que escribo ya que tambien genero
		// la marca en el siguiente.
#ifdef EE_INTERNA
		retS = TRUE;
		eeprom_write_block (bdFrame.chrFrame, eeAddress , RECDSIZE);
#endif
#ifdef EE_EXTERNA
		//systemVars.debugLevel += D_I2C;
		retS = EE_pageWrite( eeAddress , RECDSIZE , bdFrame.chrFrame);
		//systemVars.debugLevel -= D_I2C;
#endif
		if ( retS) {
			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_put::eeWR OK [0x0%x],%d/%d\r\n\0"),checksum,bd.head,eeAddress);
		} else {
			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_put::eeWR FAIL [0x0%x],%d/%d\r\n\0"),checksum,bd.head,eeAddress);
		}

		// Pass3: Avanzo el puntero a la siguiente posicion en forma circular
		// y marco como borrado.
		bd.count++;
		bd.head = (++bd.head == bd.maxRecs) ?  0 : bd.head;
		// overrrun ?
		if ( bd.head ==  bd.tail ) {
			bd.tail = (++bd.tail == bd.maxRecs) ?  0 : bd.tail;
			bd.count--;
			memset( &bdFrame.chrFrame, BD_EMPTY_MARK, RECDSIZE );
			eeAddress = EEADDR_START_DATA + bd.head * RECDSIZE;
#ifdef EE_INTERNA
			eeprom_write_block (bdFrame.chrFrame, eeAddress , RECDSIZE);
#endif
#ifdef EE_EXTERNA
			//systemVars.debugLevel += D_I2C;
			retS = EE_pageWrite( eeAddress , RECDSIZE , bdFrame.chrFrame);
			//systemVars.debugLevel -= D_I2C;
#endif
		}
		//
		xSemaphoreGive( sem_BD );
	}

	sp5K_debugPrintStr(D_BD, pv_BDprintfBuff );
	return(retS);

}
//----------------------------------------------------------------------------------

s08 BD_delete( s16 rcdNro )
{
/* Borro todos los registros desde el actual hasta el recNro.
 * Si es -1, solo borro el actual
 * Verifico que el rcdNro este en el bloque de datos activos ( sin borrarse ).
 */

u16 eeAddress;
s08 retS;
s08 exitFlag = FALSE;

	memset( &pv_BDprintfBuff, '\0', sizeof(pv_BDprintfBuff));

	if (xSemaphoreTake( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) {

		if ( rcdNro >= 0) {
			// Caso1: head = tail: memoria ya vacia.
			if ( bd.head == bd.tail ) {
				snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BDdelete::BD Empty.r\n\0"));
				sp5K_debugPrintStr(D_BD, pv_BDprintfBuff );
				bd.count = 0;
				goto quit;
			}
			// Caso2: registro no corresponde a bloque valido.
			if ( bd.head > bd.tail) {
				if ( ( rcdNro < bd.tail) || ( rcdNro > bd.head) ) {
					snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BDdelete::rcd %d out of scope.r\n\0"),rcdNro);
					sp5K_debugPrintStr(D_BD, pv_BDprintfBuff );
					goto quit;
				}
			}
			if ( bd.head < bd.tail) {
				if ( ( rcdNro > bd.head) && ( rcdNro < bd.tail) ) {
					snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BDdelete::rcd %d out of scope.\r\n\0"),rcdNro);
					sp5K_debugPrintStr(D_BD, pv_BDprintfBuff );
					goto quit;
				}
			}
		}

		// Caso3: hay datos: ciclo de borrado
		exitFlag = FALSE;
		memset( &bdFrame.chrFrame, BD_EMPTY_MARK, RECDSIZE );
		while(!exitFlag) {
			// Calculo la direccion del registro a borrar
			eeAddress = EEADDR_START_DATA + bd.tail * RECDSIZE;
			// Lo borro
#ifdef EE_INTERNA
			retS = TRUE;
			eeprom_write_block (bdFrame.chrFrame, eeAddress , RECDSIZE);
#endif
#ifdef EE_EXTERNA
			retS = EE_pageWrite( eeAddress , RECDSIZE , bdFrame.chrFrame);
#endif

			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BDdelete::DEL OK (%d/%d).\r\n\0"), bd.tail, eeAddress);
			sp5K_debugPrintStr(D_BD, &pv_BDprintfBuff );
			//
			// Si solo debo borrar uno, salgo.
			if ( rcdNro == -1 ) {
				exitFlag = TRUE;
			}
			//
			// Memoria vacia:salgo
			if (bd.tail == bd.head) {
				exitFlag = TRUE;
			}
			//
			// Ultimo registro ?
			if (bd.tail == rcdNro) {
				exitFlag = TRUE;
			}
			//
			// C/u que borro avanzo tail
			bd.tail = (++bd.tail == bd.maxRecs) ?  0 : bd.tail;
			if (bd.count > 0 ) {
				bd.count--;
			}

		}

quit:
		xSemaphoreGive( sem_BD );
	}
	return(TRUE);
}
//----------------------------------------------------------------------------------

u08 BD_get( frameDataType *rdDataRec, u16 index )
{
// El parametro index indica el ordinal del registro a leer ( 0..maxRcs)
// Los codigos de retorno son:
// 0: Memoria vacia.
// 1: Lectura correcta
// 2: Error de lectura
// 3: index fuera del bloque de datos.

u16 eeAddress;
s08 retS;
u08 retVal;
u08 checksum;

	memset( &pv_BDprintfBuff, '\0', sizeof(pv_BDprintfBuff));

	if (xSemaphoreTake( sem_BD, MSTOTAKEBDSEMPH ) == pdTRUE ) {

		// Caso1: head = tail: memoria ya vacia.
		if ( bd.head == bd.tail ) {
			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::BD Empty.\r\n\0"));
			retVal = 0;
			rdDataRec = NULL;
			goto quit;
		}
		// Caso2: registro no corresponde a bloque valido.
		if ( bd.head > bd.tail) {
			if ( ( index < bd.tail) || ( index >= bd.head) ) {
				snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::rcd %d out of scope.\r\n\0"),index);
				retVal = 3;
				rdDataRec = NULL;
				goto quit;
			}
		}
		if ( bd.head < bd.tail) {
			if ( ( index >= bd.head) && ( index < bd.tail) ) {
				snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::rcd %d out of scope.r\n\0"),index);
				retVal = 3;
				rdDataRec = NULL;
				goto quit;
			}
		}

		// Leo de la posicion index
		memset( &bdFrame.chrFrame, '\0', RECDSIZE );
		eeAddress = EEADDR_START_DATA + index * RECDSIZE;
#ifdef EE_INTERNA
		retS = TRUE;
		eeprom_read_block ( bdFrame.chrFrame, eeAddress , RECDSIZE);
#endif
#ifdef EE_EXTERNA
		//systemVars.debugLevel += D_I2C;
		retS = EE_read( eeAddress , RECDSIZE, bdFrame.chrFrame );
		//systemVars.debugLevel -= D_I2C;
#endif

		// Calculo el checksum del frame y lo comparo con el leido.
//		checksum = pvBD_checksum( bdFrame.chrFrame, ( sizeof(frameDataType) - 1) );
//		if (checksum == bdFrame.frame.checksum) {
			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::RD OK [0x0%x],%d/%d\r\n\0"),bdFrame.frame.checksum,index,eeAddress);
			memcpy ( rdDataRec, &bdFrame.frame, sizeof(frameDataType) );
			retVal = 1;
			goto quit;
//		} else {
			// Error de checksum ??
//			snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::CKS FAIL [0x0%x][0x0%x],%d/%d\r\n\0"),checksum, bdFrame.frame.checksum,index,eeAddress);
//			if ( !retS) {
				// Error de lectura
//				snprintf_P( pv_BDprintfBuff,CHAR128,PSTR("BD_get::RD FAIL [0x0%x],%d/%d\r\n\0"), bdFrame.frame.checksum,index,eeAddress);
//			}
//			rdDataRec = NULL;
//			retVal = 2;
//			goto quit;
//		}

quit:

		xSemaphoreGive( sem_BD );
	}

	sp5K_debugPrintStr(D_BD, pv_BDprintfBuff );
	return(retVal);
}
//----------------------------------------------------------------------------------

u08 pvBD_checksum( char *buff, u08 limit )
{
u08 checksum = 0;
u08 i;

	for(i=0;i< (limit-1);i++)
		checksum += buff[i];
	checksum = ~checksum;
	return (checksum);
}
//----------------------------------------------------------------------------------

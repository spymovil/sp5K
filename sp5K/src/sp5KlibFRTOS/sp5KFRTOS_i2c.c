/*
 * sp5KFRTOS_i2c.c
 *
 *  Created on: 28/10/2013
 *      Author: root
 *
 *  Funciones de base de atencion al bus I2E modificadas para usarse con FRTOS.
 *  1- Las esperas las hago con taskYIELD() o xTaskDelay()
 *  2- El acceso lo hago por medio de un semaforo.
 *
 */

#include "sp5KFRTOS.h"

// Funciones privadas
static void i2C_setBitRate(int bitrateKHz);
static inline s08 i2c_sendStart(void);
static inline void i2c_sendStop(void);
static inline s08 i2c_writeByte(u08 data);
//static inline u08 i2c_status(void);
static inline s08 i2c_readByte(u08 ackFlag);
static inline void i2c_disable(void);
//
u08 timeout;
//
static char pv_i2CprintfBuff[CHAR64];

//------------------------------------------------------------------------------------

void FRTOSi2c_init(void)
{
	vSemaphoreCreateBinary( sem_I2Cbus );
}
//------------------------------------------------------------------------------------

void i2c_init(void)
{
	// set pull-up resistors on I2C bus pins
	//sbi(PORTC, 0);  // i2c SCL
	//sbi(PORTC, 1);  // i2c SDA
	PORTC |= 1 << SCL;
	PORTC |= 1 << SDA;
	// set i2c bit rate to 100KHz
	i2C_setBitRate(100);

}
//------------------------------------------------------------------------------------

static void i2C_setBitRate(int bitrateKHz)
{
int bitrate_div;

	// SCL freq = F_CPU/(16+2*TWBR*4^TWPS)

	// set TWPS to zero
	//cbi(TWSR, TWPS0);
	//cbi(TWSR, TWPS1);
	((TWSR) &= ~(1 << (TWPS0)));
	((TWSR) &= ~(1 << (TWPS1)));
	// SCL freq = F_CPU/(16+2*TWBR))

	// calculate bitrate division
	bitrate_div = ((F_CPU/1000l)/bitrateKHz);
	if(bitrate_div >= 16) {
		bitrate_div = (bitrate_div-16)/2;
	}

	TWBR = bitrate_div;
}
//------------------------------------------------------------------------------------

static inline s08 i2c_sendStart(void)
{

	// Genera la condicion de START en el bus I2C.
	// TWCR.TWEN = 1 : Habilita la interface TWI
	// TWCR.TWSTA = 1: Genera un START
	// TWCR.TWINT = 1: Borra la flag e inicia la operacion del TWI
	//
	// Cuando el bus termino el START, prende la flag TWCR.TWINT y el codigo
	// de status debe ser 0x08 o 0x10.
	//
	// Controlo por timeout la salida para no quedar bloqueado.
	//
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

	// Espero que la interface I2C termine su operacion hasta 10 ticks.
	timeout = 10;
	while ( (timeout-- > 0) && !(TWCR & (1<<TWINT) ) ) {
		//_delay_ms(1);
		taskYIELD();
	}

	if (timeout == 0) {
		return (FALSE);
	} else {
		// retieve current i2c status from i2c TWSR
		return( TWSR & 0xF8 );
	}

}
//------------------------------------------------------------------------------------

static inline s08 i2c_writeByte(u08 data)
{

	// Envia el data por el bus I2C.
	TWDR = data;
	// Habilita la trasmision y el envio de un ACK.
	TWCR = (1 << TWINT) | (1 << TWEN);

	// Espero que la interface I2C termine su operacion.
	timeout = 10;
	while ( (timeout-- > 0) && !(TWCR & (1<<TWINT) ) ) {
		//_delay_ms(1);
		taskYIELD();
	}

	if (timeout == 0) {
		return (FALSE);
	} else {
		// retieve current i2c status from i2c TWSR
		return( TWSR & 0xF8 );
	}

}
//------------------------------------------------------------------------------------

static inline s08 i2c_readByte(u08 ackFlag)
{
	// begin receive over i2c
	if( ackFlag )
	{
		// ackFlag = TRUE: ACK the recevied data
		TWCR = (1 << TWEA) | (1 << TWINT) | (1 << TWEN);
	}
	else
	{
		// ackFlag = FALSE: NACK the recevied data
		TWCR = (1 << TWINT) | (1 << TWEN);
	}

	// Espero que la interface I2C termine su operacion.
	timeout = 10;
	while ( (timeout-- > 0) && !(TWCR & (1<<TWINT) ) ) {
		//_delay_ms(1);
		taskYIELD();
	}

	if (timeout == 0) {
		return (FALSE);
	} else {
		// retieve current i2c status from i2c TWSR
		return( TWSR & 0xF8 );
	}

}
//------------------------------------------------------------------------------------

static inline void i2c_sendStop(void)
{
	// Genera la condicion STOP en el bus I2C.
	// !!! El TWINT NO ES SETEADO LUEGO DE UN STOP
	// por lo tanto no debo esperar que termine.

	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);

}
//-----------------------------------------------------------------------------------

//static inline u08 i2c_status(void)
//{
	// Retorna el status del modulo I2C por medio del registro TWSR
//	return( TWSR & 0xF8 );
//}
//------------------------------------------------------------------------------------
static inline void i2c_disable(void)
{
	// Deshabilito la interfase TWI
	TWCR &= ~(1 << TWEN);

}
//------------------------------------------------------------------------------------

s08 i2c_masterWrite (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, char *data)
{
	// Escribe en el dispositivo I2C 'length' byes, apuntados por *data
	// Todos los dispositivos I2C permite hacer esta escritura de bloque
	// que es mas rapida que ir escribiendo de a byte.
	// i2c_address: direccion del primer byte a escribir
	// dev_id:      identificador del dispositivo I2C ( eeprom: 0xA0, ds1340: 0xD0, buscontroller: 0x40, etc)
	// dev_addr:    direccion individual del dispositivo dentro del bus ( 0,1,2,...)
	// length:      cantidad de bytes a escribir
	// * data:      puntero a la posicion del primer byte a escribir.
	// Retorna -1 en caso de error o 1 en caso OK ( TRUE, FALSE).
	// En caso de error, imprime el mismo con su codigo en la salida de DEBUG.

	u08 i2c_status;
	char txbyte;
	u08 n = 0;
	s08 r_val = FALSE;
	s08 i2C_debugFlag = FALSE;
	u08 i;

	if ( sp5K_debugControl ( D_I2C ) ) {
		i2C_debugFlag = TRUE;
		memset( &pv_i2CprintfBuff, '\0', sizeof(pv_i2CprintfBuff));
	}

	if (xSemaphoreTake( sem_I2Cbus, MSTOTAKEI2CSEMPH ) == pdTRUE ) {

		// Control de tamanio de buffer
		if (length > EERDBUFLENGHT)
			length = EERDBUFLENGHT;

		i2c_retry:

			if (n++ >= I2C_MAXTRIES) goto i2c_quit;

			// Pass1) START: Debe retornar 0x08 (I2C_START) o 0x10 (I2C_REP_START) en condiciones normales
			i2c_status = i2c_sendStart();
			if ( i2c_status == TW_MT_ARB_LOST) goto i2c_retry;
			if ( (i2c_status != TW_START) && (i2c_status != TW_REP_START) ) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::sendStart ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::sendStart OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			// Pass2) (SLA_W) Send slave address. Debo recibir 0x18 ( SLA_ACK )
			txbyte = (dev_id & 0xF0) | ((dev_addr & 0x07) << 1) | TW_WRITE;
			i2c_status = i2c_writeByte(txbyte);
			// Check the TWSR status
			if ((i2c_status == TW_MT_SLA_NACK) || (i2c_status == TW_MT_ARB_LOST)) goto i2c_retry;
			if (i2c_status != TW_MT_SLA_ACK) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte SLA_W ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::i2c_wrByte SLA_W OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			// Pass3) Envio la direccion fisica donde comenzar a escribir.
			// En las memorias es una direccion de 16 bytes.
			// En el DS1344 o el BusController es de 1 byte
			switch (dev_type) {
			case EE24X:
				// La direccion fisica son 2 byte.
				// Envio primero el High 8 bit i2c address. Debo recibir 0x28 ( DATA_ACK)
				txbyte = i2c_address >> 8;
				i2c_status = i2c_writeByte(txbyte);
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte High 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte High 8 bit addr OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				// Envio el Low byte.
				txbyte = i2c_address;
				i2c_status = i2c_writeByte(txbyte);
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte Low 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
				break;
			case DS1340:
				// Envio una direccion de 1 byte.
				// Send the High 8-bit of I2C Address. Debo recibir 0x28 (DATA_ACK)
				txbyte = i2c_address;
				i2c_status = i2c_writeByte(txbyte);
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte Low 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
				break;
			case MPC23008:
				// Envio una direccion de 1 byte.
				// Send the High 8-bit of I2C Address. Debo recibir 0x28 (DATA_ACK)
				txbyte = i2c_address;
				i2c_status = i2c_writeByte(txbyte);
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte Low 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
				break;
			case ADS7828:
				// No hay address.
				break;
			default:
				break;
			}

			// Pass4) Trasmito todos los bytes del buffer. Debo recibir 0x28 (DATA_ACK)
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::DATA WR LOOP [%d]\r\n\0"), length);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			i = 0;

			while(length)
			{
				txbyte = *data++;
				i2c_status = i2c_writeByte( txbyte );
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::wrByte loop ERROR [%d][0x%03x][0x%03x]\r\n\0"),i2c_status,i2c_status,txbyte);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
#ifdef MEMINFO
				if ( (u08)(txbyte > 0x20) && ((u08)txbyte < 0x7F)) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_mW::wByte [%c][0x%03x]-[%02d] OK [%d][0x%03x]\r\n\0"),txbyte,txbyte,i,i2c_status,i2c_status);
				} else {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_mW::wByte [.][0x%03x]-[%02d] OK [%d][0x%03x]\r\n\0"),txbyte,i,i2c_status,i2c_status);
				}
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
#endif
				length--;
				i++;
			}

			// I2C transmit OK.
			r_val = TRUE;
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMW::END OK\r\n\0"));
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
		i2c_quit:

			// Pass5) STOP
			i2c_sendStop();
			vTaskDelay( (portTickType)( 20 / portTICK_RATE_MS ) );

			// En caso de error libero la interface.
			if (r_val == FALSE) i2c_disable();

			xSemaphoreGive( sem_I2Cbus );
		}

	return(r_val);

}
//------------------------------------------------------------------------------------

s08 i2c_masterRead (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, char *data)
{
	// Lee del dispositivo I2C 'length' byes y los retorna en *data
	// Todos los dispositivos I2C permite hacer esta lectura de bloque
	// que es mas rapida que ir escribiendo de a byte.
	// i2c_address: direccion del primer byte a escribir
	// dev_id:      identificador del dispositivo I2C ( eeprom: 0xA0, ds1340: 0xD0, buscontroller: 0x40, etc)
	// dev_addr:    direccion individual del dispositivo dentro del bus ( 0,1,2,...)
	// length:      cantidad de bytes a escribir
	// * data:      puntero a la posicion del primer byte a escribir.
	// Retorna -1 en caso de error o 1 en caso OK ( TRUE, FALSE).
	// En caso de error, imprime el mismo con su codigo en la salida de DEBUG.

	u08 i2c_status;
	char txbyte;
	u08 n = 0;
	s08 r_val = FALSE;
	s08 i2C_debugFlag = FALSE;
	u08 i;

	if ( sp5K_debugControl ( D_I2C ) ) {
		i2C_debugFlag = TRUE;
		memset( &pv_i2CprintfBuff, '\0', sizeof(pv_i2CprintfBuff));
	}

	if (xSemaphoreTake( sem_I2Cbus, MSTOTAKEI2CSEMPH ) == pdTRUE ) {

		// Control de tamanio de buffer
		if (length > EERDBUFLENGHT)
			length = EERDBUFLENGHT;

		i2c_retry:

			if (n++ >= I2C_MAXTRIES) goto i2c_quit;

			// Pass1) START: Debe retornar 0x08 (I2C_START) o 0x10 (I2C_REP_START) en condiciones normales
			i2c_status = i2c_sendStart();
			if (i2c_status == TW_MT_ARB_LOST) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::i2c_sendStart RETRY [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_retry;
			}
			if ( (i2c_status != TW_START) && (i2c_status != TW_REP_START) ) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::i2c_sendStart ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::i2c_sendStart OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			// Pass2) (SLA_W) Send slave address + WRITE
			txbyte = (dev_id & 0xF0) | ((dev_addr & 0x07) << 1) | TW_WRITE;
			i2c_status = i2c_writeByte(txbyte);
			// Check the TWSR status
			if ((i2c_status == TW_MT_SLA_NACK) || (i2c_status == TW_MT_ARB_LOST)) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_W RETRY [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_retry;
			}
			if (i2c_status != TW_MT_SLA_ACK) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_W ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_W OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			// Pass3) Envio la direccion fisica donde comenzar a leer.
			// En las memorias es una direccion de 16 bytes.
			// En el DS1344 o el BusController es de 1 byte
			switch (dev_type) {
			case EE24X:
				// La direccion fisica son 2 byte.
				// Envio primero el High 8 bit i2c address
				i2c_status = i2c_writeByte(i2c_address >> 8);
				if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte High 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte High 8 bit addr OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				break;
			case DS1340:
				break;
			case MPC23008:
				break;
			default:
				break;
			}
			// En todos los otros casos envio una direccion de 1 byte.
			// Send the Low 8-bit of I2C Address
			i2c_status = i2c_writeByte(i2c_address);
			if (i2c_status != TW_MT_DATA_ACK) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wreByte Low 8 bit addr ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte Low 8 bit addr OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif

			// Pass4) REPEATED START
			i2c_status = i2c_sendStart();
			if (i2c_status == TW_MT_ARB_LOST) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::sendRepStart RETRY [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_retry;
			}
			if ( (i2c_status != TW_START) && (i2c_status != TW_REP_START) ) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::sendRepStart ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::sendRepStart OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif

			// Pass5) (SLA_R) Send slave address + READ. Debo recibir un 0x40 ( SLA_R ACK)
			txbyte = (dev_id & 0xF0) | ((dev_addr << 1) & 0x0E) | TW_READ;
			i2c_status = i2c_writeByte(txbyte);
			// Check the TWSR status
			if ((i2c_status == TW_MR_SLA_NACK) || (i2c_status == TW_MR_ARB_LOST)) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_R RETRY [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_retry;
			}
			if (i2c_status != TW_MR_SLA_ACK) {
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_R ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				goto i2c_quit;
			}
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::wrByte SLA_R OK [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif

			// Pass6.1) Leo todos los bytes requeridos y respondo con ACK.
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::DATA RD LOOP [%d]\r\n\0"), length);
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif

			i = 0;
			while(length > 1)
			{
				// accept receive data and ack it
				// begin receive over i2c
				i2c_status = i2c_readByte(ACK);
				if (i2c_status != TW_MR_DATA_ACK) {
#ifdef MEMINFO
					if ( i2C_debugFlag ) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::data rcvd ERROR [%d][0x%03x]\r\n\0"),i2c_status,i2c_status);
						sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
					}
#endif
					goto i2c_quit;
				}
				*data++ = TWDR;
#ifdef MEMINFO
				if ( i2C_debugFlag ) {
					if ( (u08)(TWDR > 0x20) && ((u08)TWDR < 0x7F)) {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_MR::rdByte [%c][0x%03x]-[%02d] OK(ACK)\r\n\0"),TWDR,TWDR,i);
					} else {
						snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_MR::rdByte [.][0x%03x]-[%02d] OK(ACK)\r\n\0"),TWDR,i);
					}
					sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
				}
#endif
				// decrement length
				length--;
				i++;
			}

			// Pass6.2) Acepto el ultimo byte y respondo con NACK
			i2c_readByte(NACK);
			*data++ = TWDR;

#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				if ( (u08)(TWDR > 0x20) && ((u08)TWDR < 0x7F)) {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_MR::rdByte [%c][0x%03x]-[%02d] OK(NACK)\r\n\0"),TWDR,TWDR,i);
				} else {
					snprintf_P( pv_i2CprintfBuff,CHAR64,PSTR("i2c_MR::rdByte [.][0x%03x]-[%02d] OK(NACK)\r\n\0"),TWDR,i);
				}
				sp5K_debugPrintStr(D_I2C, pv_i2CprintfBuff);
			}
#endif
			// I2C read OK.
			r_val = TRUE;
#ifdef MEMINFO
			if ( i2C_debugFlag ) {
				snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("i2cMR::END OK\r\n\0"));
				sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
			}
#endif
		i2c_quit:

			// Pass5) STOP
			i2c_sendStop();

			// En caso de error libero la interface.
			if (r_val == FALSE) i2c_disable();

			xSemaphoreGive( sem_I2Cbus );
		}

	return(r_val);
}
//------------------------------------------------------------------------------------

s08 adc_masterRead (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, u08 *data)
{
	// Tomado de i2c_masterRead pero modificado para el ADC7828 en el cual el read
	// no manda la SLA+W.


	u08 i2c_status;
	u08 txbyte;
	u08 n = 0;
	s08 r_val = FALSE;
	s08 i2C_debugFlag = FALSE;

	if ( sp5K_debugControl ( D_I2C ) ) {
		i2C_debugFlag = TRUE;
		memset( &pv_i2CprintfBuff, '\0', sizeof(pv_i2CprintfBuff));
	}

	if (xSemaphoreTake( sem_I2Cbus, MSTOTAKEI2CSEMPH ) == pdTRUE ) {

		// Control de tamanio de buffer
		if (length > EERDBUFLENGHT)
			length = EERDBUFLENGHT;

		i2c_retry:

			if (n++ >= I2C_MAXTRIES) goto i2c_quit;

			// Pass1) START: Debe retornar 0x08 (I2C_START) o 0x10 (I2C_REP_START) en condiciones normales
			i2c_status = i2c_sendStart();
			if (i2c_status == TW_MT_ARB_LOST) {
				if ( i2C_debugFlag ) {
//					snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_sendStart RETRY [%d][%x]\r\n\0"),i2c_status,i2c_status);
//					sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
				}
				//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_sendStart RETRY [%d][%x]\r\n",i2c_status,i2c_status);
				goto i2c_retry;
			}
			if ( (i2c_status != TW_START) && (i2c_status != TW_REP_START) ) {
				if ( i2C_debugFlag ) {
//					snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_sendStart ERROR [%d][%x]\r\n\0"),i2c_status,i2c_status);
//					sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
				}
				//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_sendStart ERROR [%d][%x]\r\n",i2c_status,i2c_status);
				goto i2c_quit;
			}
			if ( i2C_debugFlag ) {
//				snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_sendStart OK [%d][%x]\r\n\0"),i2c_status,i2c_status);
//				sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
			}
			//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_sendStart OK [%d][%x]\r\n",i2c_status,i2c_status);



			// Pass5) (SLA_R) Send slave address + READ. Debo recibir un 0x40 ( SLA_R ACK)
			txbyte = (dev_id & 0xF0) | ((dev_addr << 1) & 0x0E) | TW_READ;
			i2c_status = i2c_writeByte(txbyte);
			// Check the TWSR status
			if ((i2c_status == TW_MR_SLA_NACK) || (i2c_status == TW_MR_ARB_LOST)) {
				if ( i2C_debugFlag ) {
//					snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_writeByte SLA_R RETRY [%d][%x]\r\n\0"),i2c_status,i2c_status);
//					sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
				}
				//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_writeByte SLA_R RETRY [%d][%x]\r\n",i2c_status,i2c_status);
				goto i2c_retry;
			}
			if (i2c_status != TW_MR_SLA_ACK) {
				if ( i2C_debugFlag ) {
//					snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_writeByte SLA_R ERROR [%d][%x]\r\n\0"),i2c_status,i2c_status);
//					sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
				}
				//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_writeByte SLA_R ERROR [%d][%x]\r\n",i2c_status,i2c_status);
				goto i2c_quit;
			}
			if ( i2C_debugFlag ) {
//				snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::i2c_writeByte SLA_R OK [%d][%x]\r\n\0"),i2c_status,i2c_status);
//				sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
			}
			//dRprintf(CMD_UART,I2C,"adc_masterRead:: i2c_writeByte SLA_R OK [%d][%x]\r\n",i2c_status,i2c_status);

			// Pass6.1) Leo todos los bytes requeridos y respondo con ACK.
			while(length > 1)
			{
				// accept receive data and ack it
				// begin receive over i2c
				i2c_status = i2c_readByte(ACK);
				if (i2c_status != TW_MR_DATA_ACK) {
					if ( i2C_debugFlag ) {
//						snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::data rcvd ERROR [%d][%x]\r\n\0"),i2c_status,i2c_status);
//						sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
					}
					//dRprintf(CMD_UART,I2C,"adc_masterRead:: data rcvd ERROR [%d][%x]\r\n",i2c_status,i2c_status);
					goto i2c_quit;
				}
				*data++ = TWDR;
				if ( i2C_debugFlag ) {
//					snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::data OK(ACK) [%c][%x][%d]\r\n\0"), TWDR, TWDR, length);
//					sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
				}
				//dRprintf(CMD_UART,I2C,"adc_masterRead:: data OK(ACK) [%c][%x][%d]\r\n", TWDR, TWDR, length);
				// decrement length
				length--;
			}

			// Pass6.2) Acepto el ultimo byte y respondo con NACK
			i2c_readByte(NACK);
			*data++ = TWDR;
			if ( i2C_debugFlag ) {
//				snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::data OK(NACK) [%c][%x][%d]\r\n\0"), TWDR, TWDR,length);
//				sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
			}
			//dRprintf(CMD_UART,I2C,"adc_masterRead:: data OK(NACK) [%c][%x][%d]\r\n", TWDR, TWDR,length);

			// I2C read OK.
			r_val = TRUE;
			if ( i2C_debugFlag ) {
//				snprintf_P( &pv_i2CprintfBuff,CHAR64,PSTR("adc_masterRead::END OK\r\n\0"));
//				sp5K_debugPrintStr(D_I2C, &pv_i2CprintfBuff);
			}
			//dRprintf(CMD_UART,I2C,"adc_masterRead:: OK\r\n");

		i2c_quit:

			// Pass5) STOP
			i2c_sendStop();

			// En caso de error libero la interface.
			if (r_val == FALSE) i2c_disable();

			xSemaphoreGive( sem_I2Cbus );
		}

	return(r_val);
}
//------------------------------------------------------------------------------------


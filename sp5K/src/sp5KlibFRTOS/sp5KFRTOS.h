/*
 * sp5000.h
 *
 *  Created on: 18/11/2013
 *      Author: root
 */

#ifndef SP5000_H_
#define SP5000_H_

#include <avr/io.h>			/* include I/O definitions (port names, pin names, etc) */
//#include <avr/signal.h>		/* include "signal" names (interrupt names) */
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <string.h>
#include <compat/deprecated.h>
#include <util/twi.h>
#include <util/delay.h>
#include <ctype.h>

#include "avrlibdefs.h"
#include "avrlibtypes.h"
#include "global.h"			// include our global settings
#include "uart2_sp5KFRTOS.h"
#include "rprintf_sp5KFRTOS.h"
#include "vt100_sp5K.h"
#include "cmdline.h"
#include "cmdlineconf.h"
//
/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "list.h"
#include "croutine.h"
#include "semphr.h"
#include "timers.h"
//
// FRTOS
// Semaforos
xSemaphoreHandle sem_I2Cbus;
#define MSTOTAKEI2CSEMPH (( portTickType ) 10 )
xSemaphoreHandle  sem_CmdUart;
#define MSTOTAKECMDUARTSEMPH (( portTickType ) 10 )
//
xSemaphoreHandle sem_GprsUart;

#define CHAR64		64
#define CHAR128	 	128
#define CHAR256	 	256
#define CHAR384		384

//------------------------------------------------------------------------------------
// UARTS

#define UART0	0
#define UART1	1

#define GPRS_UART	0
#define CMD_UART 	1

#define UARTCTL_PORT	PORTD
#define UARTCTL_PIN		PIND
#define UARTCTL			4
#define UARTCTL_DDR		DDRD
#define UARTCTL_MASK	0x10

void sp5K_uartsEnable(void);

//------------------------------------------------------------------------------------
// I2C

#define I2C_MAXTRIES	5
#define EERDBUFLENGHT	64

#define EE24X		0
#define DS1340		1
#define MPC23008	2
#define ADS7828		3
//
#define SCL		0
#define SDA		1
//
#define ACK 1
#define NACK 0
//
void FRTOSi2c_init(void);
void i2c_init(void);
s08 i2c_masterWrite (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, char *data);
s08 i2c_masterRead (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, char *data);
s08 adc_masterRead (u08 dev_type, u16 i2c_address, u08 dev_id, u08 dev_addr,u08 length, u08 *data);
//
//------------------------------------------------------------------------------------
// RTC

#define DS1340_ID   	0xD0
#define DS1340_ADDR  	0x00

typedef struct struct_RtcTime
{
	// Tamanio: 7 byte.
	// time of day
	u08 sec;
	u08 min;
	u08 hour;
	// date
	u08 day;
	u08 month;
	u16 year;

} RtcTimeType;

s08 RTC_read( char *data);
s08 RTC_write( char *data);

s08 rtcGetTime(RtcTimeType *rtc);
s08 rtcSetTime(RtcTimeType *rtc);

struct {
	u16 tick;
	u08 sec;
	u08 min;
	u08 hour;
} localRtc;

void initDebugRtc(void);

//------------------------------------------------------------------------------------
// EEPROM EXTERNA

#define EE_ID			0xA0
#define EE_ADDR			0x00
#define EEPAGESIZE			64

s08 EE_write( u16 i2c_address,u08 length, char *data);
s08 EE_read( u16 i2c_address,u08 length, char *data);
s08 EE_pageWrite( u16 i2c_address,u08 length, char *data);

//------------------------------------------------------------------------------------
// ADS7828

#define 	ADS7828_ID	  	0x91
#define 	ADS7828_ADDR	0x00

#define 	ADS7828_CMD_SD   0x80		//ADS7828 Single-ended/Differential Select bit.

#define 	ADS7828_CMD_PDMODE0   0x00	//ADS7828 Power-down Mode 0.
#define 	ADS7828_CMD_PDMODE1   0x04	//ADS7828 Power-down Mode 1.
#define 	ADS7828_CMD_PDMODE2   0x08	//ADS7828 Power-down Mode 2.
#define 	ADS7828_CMD_PDMODE3   0x0C	//ADS7828 Power-down Mode 3.

void ADS7828_init(void);
s08 ADS7828_convert(u08 channel, u16 *value);

//------------------------------------------------------------------------------------
// MCP23008

#define TERMSW_PORT		PORTD
#define TERMSW_PIN		PIND
#define TERMSW_INT		7
#define TERMSW_DDR		DDRD
#define TERMSW_MASK		0x80

#define MCP0_INTPORT	PORTD
#define MCP0_INTPIN		PIND
#define MCP0_INT		5
#define MCP0_INTDDR		DDRD
#define MCP0_INTMASK	0x20

#define MCP1_INTPORT	PORTB
#define MCP1_INTPIN		PINB
#define MCP1_INT		2
#define MCP1_INTDDR		DDRB
#define MCP1_INTMASK	0x04

#define MCP_ID				0x40
#define MCP_ADDR0			0x00	// MCP23008
#define MCP_ADDR1			0x01	// MCP23008
#define MCP_ADDR2			0x07	// MCP23018

#define MCPADDR_IODIR		0x00
#define MCPADDR_IPOL		0x01
#define MCPADDR_GPINTEN		0x02
#define MCPADDR_DEFVAL		0x03
#define MCPADDR_INTCON		0x04
#define MCPADDR_IOCON		0x05
#define MCPADDR_GPPU		0x06
#define MCPADDR_INTF		0x07
#define MCPADDR_INTCAP		0x08
#define MCPADDR_GPIO 		0x09
#define MCPADDR_OLAT 		0x0A

#define MCP_GPIO_IGPRSDCD			1	// IN
#define MCP_GPIO_IGPRSRI			2	// IN
#define MCP_GPIO_OGPRSSW			3	// OUT

#define MCP_GPIO_OTERMPWR			4
#define MCP_GPIO_OGPRSPWR			5
#define MCP_GPIO_OLED				6

#define MCP_GPIO_DIN0				2	// IN
#define MCP_GPIO_DIN1				3	// IN
#define MCP_GPIO_PWRSENSORS			6	// OUT

// MCP23018
#define MCP2_IODIRA					0x00
#define MCP2_IODIRB					0x01
#define MCP2_GPINTENA				0x04
#define MCP2_GPINTENB				0x05
#define MCP2_DEFVALA				0x06
#define MCP2_DEFVALB				0x07
#define MCP2_INTCONA				0x08
#define MCP2_INTCONB				0x09
#define MCP2_IOCON					0x0A
#define MCP2_GPPUA					0x0C
#define MCP2_GPPUB					0x0D
#define MCP2_INTCAPA				0x10
#define MCP2_INTCAPB				0x11
#define MCP2_GPIOA					0x12
#define MCP2_GPIOB					0x13
#define MCP2_OLATA					0x14
#define MCP2_OLATB					0x15

#define MCP2_ENA2						0
#define MCP2_ENB2						1
#define MCP2_PHB2						2
#define MCP2_PHB1						3
#define MCP2_ENB1						4
#define MCP2_ENA1						5
#define MCP2_PHA1						6

#define MCP2_RESET						0
#define MCP2_SLEEP						1
#define MCP2_FAULT						2
#define MCP2_PHA2						3
#define MCP2_PWRSENSORS					4
#define MCP2_DIN0						5
#define MCP2_DIN1						6
#define MCP2_OANALOG					7

#define MCP2_GPIO_DIN0					6	// IN
#define MCP2_GPIO_DIN1					5	// IN
#define MCP2_GPIO_PWRSENSORS			4	// OUT
#define MCP2_GPIO_ANALOGPWR				7	// OUT

#define MCP2_GPIO_DIN0				6	// IN
#define MCP2_GPIO_DIN1				5	// IN
#define MCP2_GPIO_PWRSENSORS		4	// OUT
#define MCP2_GPIO_PWRANALOG			7	// OUT

#define MCP_NONE	0
#define MCP23008	1
#define MCP23018	2

u08 mcpDevice;

typedef enum { VOPEN = 0, VCLOSE = 1 } t_valvesActions;

void MCP_init(void);
s08 MCP_read( u16 i2c_address, u08 dev_addr, u08 length, u08 *data);
s08 MCP_write( u16 i2c_address, u08 dev_addr, u08 length, u08 *data);
//
s08 MCP_queryDcd( u08 *pin);
s08 MCP_queryRi( u08 *pin);
s08 MCP_queryDin0( u08 *pin);
s08 MCP_queryDin1( u08 *pin);
s08 MCP_queryTermPwrSw( u08 *pin);
//
s08 MCP_setLed( u08 value );
s08 MCP_setGprsPwr( u08 value );
s08 MCP_setGprsSw( u08 value );
s08 MCP_setTermPwr( u08 value );
s08 MCP_setSensorPwr( u08 value );

s08 MCP_setVSleep( u08 value );
s08 MCP_setVReset( u08 value );
s08 MCP_setPH(  char phase, u08 chip, u08 value );
s08 MCP_setEN(  char phase, u08 chip, u08 value );

//------------------------------------------------------------------------------------
// UTILS.

char *argv[10];
u08 cmdLineArg[CMDLINE_BUFFERSIZE];

void makeargv(void);
u32 getSystemTicks(void);
void sp5K_debugPrint(u08 modo, u08 *buff);
void sp5K_printStr(u08 *buff);

void sp5KFRTOS_sleep(int secs);
void sp5KFRTOS_clearScreen(void);
void sp5K_debugPrintStr(u08 modo, char *buff);

const char *byte_to_binary(int x);

// DEBUG
typedef enum { D_NONE = 0, D_DATA = 1, D_GPRS = 2, D_BD = 4, D_I2C = 8, D_DIGITAL = 16, D_ALL = 128 } t_debug;

//------------------------------------------------------------------------------------
// GPRS

typedef struct {
	u08 buffer[uart0RXBUFFER_LEN];
	u16 head;
	u16 tail;
	u16 count;
} buffer_struct;

buffer_struct xUart0RxedCharsBuffer;

void GPRS_lineInputFunc(char cChar);
void GPRS_lineInputFlush(void);
//
//------------------------------------------------------------------------------------
#endif /* SP5000_H_ */

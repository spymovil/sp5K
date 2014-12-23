/*
 * sp5K_tkDigitalIn.c
 *
 *  Created on: 27/12/2013
 *      Author: root
 *
 *  Tarea que atiende a las entradas digitales.
 *  Tenemos 2 entradas. De las mismas medimos 2 cosas: los flancos de subida , y los ticks ( 10ms)
 *  que la señal está alta. ( duracion de los pulsos ).
 *  Debido al circuito, la senal esta normalmente low y cuando aparece un pulso esta sube.
 *  El expansor de bus dedicado al sistema analogico ( MCP01) tiene 2 entradas digitales y una
 *  salida para prender/apagar la fuente de los sensores.
 *  Lo configuramos para que en el caso de las entradas digitales se genere una interrupcion.
 *  Esta activa el pin PB2 del micro.
 *
 *  1) Configuracion del MCP.
 *  En el registro GPINTEN seteamos c/pin que queremos que genere una interrupcion on-change.
 *  bit =1: el pin genera una interrupcion.
 *  En el registro DEFVAL habilitamos a comparar contra un valor por defecto  (0 o 1). Un valor en el pin
 *  opuesto genera la interrupcion. Nosotros no usamos esta opcion.
 *  En el registro INTCON indicamos como queremos que el valor de pin se compare para generar la interrupcion.
 *  Si el bit esta en 1, el valor del pin de I/O se compara contra el valor asociado de DEFVAL.
 *  Si el bit esta en 0, el valor del pin de I/O se compara contra su valor anterior. ( detectamos cambios). Esta es la
 *  modalidad que vamos a usar. (default).
 *  IOCON: Lo modificamos ( interrupcion activa-high).
 *  GPPU: Si un pin esta configurado como input, y el correspondiente bit se pone en 1, se habilita un pull-up de 100K
 *  INTF: refleja cual pin fue el que genero la interrupcion.
 *
 *  Toda esta configuracion se hace en la funcion MCP_init().
 *
 *  Resultado: Cuando se genera una interrupcion al micro es porque alguno de los pines DIN0/1 cambio de estado.
 *
 *  2) Servicio de interrupcion del micro.
 *  EL pin de interrupcion del MCP se conecta al pin PB2 del micro. Este corresponde al PCINT10 ( grupo 1). Esto es, habilitando
 *  la interrupcion correspondiente, este pin va a generar la misma cuando cambie ( toggle ).
 *  Para habilitarlo, debo habilitar la interrupcion en general del grupo 1 de pines ( PCICR.PCIE1 = 1, pin change interrupt enable 1)
 *  y luego generar la mascara de los pines de grupo 1 que quiero que disparen la interrupcion ( PCMSK1.PCINT10 = 1).
 *  La interrupcion va a disparar el vector #6, address $000A, PCINT1_vect.
 *  Como se necesita un clock activo, el pedido debe mantenerse hasta que el micro despierte ( tick = 10ms).
 *  La interrupcion se va a generar cuando el pin INT se active como cuando se baje en consecuencia de haber leido el MCP. En este caso
 *  debemos desechar el request.
 *
 *  3) Aviso a la tarea que lea el MCP.
 *  La rutina de interrupcion le avisa a la tarea correspondiente solo cuando detecta que el pin INT genero un flanco de subida.
 *  Para esto lee el estado del pin y si es alto, avisa a la tarea.
 *  Si el estado es low, indica que ya se reconocio la int y se rearmo el sistema por lo tanto se descarta.
 *  La sincronizacion entre la rutina de ISR y la tarea la hago con un semaforo binario.
 *  Ta cual se indica en la API del FREERTOS, uno continuamente lo esta entregando al semaforo y el otro continuamente lo esta tomando, no
 *  siendo necesario que el mismo se entregue una vez tomado.
 *
 *
 */

#include "sp5K.h"

typedef struct struct_digitalIn {
	float pCount;	// contador de pulsos ( flancos positivos )
} digitalInType;

static digitalInType din0, din1;
static s08 intMcpDcd = FALSE;
static s08 intMcpQaudal = FALSE;

static char pv_digitalBuff[CHAR64];

void checkCaudalInputs(void);
void checkDcdInput(void);

//------------------------------------------------------------------------------------
ISR( PCINT1_vect )
{
/* Handler (ISR) de PCINT1.
 * El handler maneja la interrupcion de on-change del grupo de pines 8..15.
 * En particular tenemos generada una mascara para que solo interrumpa PB2.
 * Leo el pin PB2: Si es 1 debo avisar a la tarea que puede realizar su trabajo.
 * Para esto le libero el semaforo correspondiente.
 * Si el pin es 0, es porque se rearmo el sistema de la interrupcion por lo tanto
 * la ignoro.
 *
 * La interrupcion se genera 2 veces por pulso: una cuando sube y otra cuando baja.
 *
 */

u08 pb2;

	// Leo el pin B2
	pb2 = ( PINB & _BV(2) ) >> 2;
	// Si el pin es 0 salgo.
	if (pb2 == 1) {

		// El pin es 1: habilito a la tarea del caudalimetro a trabajar y que sea
		// la que determine si llego un pulso o no.
		/* Unblock the task by releasing the semaphore. */
		intMcpQaudal = TRUE;
		xSemaphoreGiveFromISR( sem_digitalIn, NULL );
		taskYIELD();
		//portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}

}
//-----------------------------------------------------------------------------------
ISR( PCINT3_vect )
{
// Handler (ISR) de PCINT3.
// Idem para PD5 que corresponde al MCP0 (DCD)

u08 pd5;

	// Leo el pin D5
	pd5 = ( PIND & _BV(5) ) >> 5;

	// Si el pin es 0 salgo.
	if (pd5 == 1) {
//		cbi (PCICR, PCIE3);
		intMcpDcd = TRUE;
		xSemaphoreGiveFromISR( sem_digitalIn, NULL );
		taskYIELD();
	}
}
/*------------------------------------------------------------------------------------*/
void tkDigitalIn(void * pvParameters)
{
// Esta tarea inicializa sus variables y luego queda en un loop infinito.
// En este, duerme y solo es depertada por un semaforo senalado por las rutinas de interrupcion o
// cada 500ms.
// Al despertar, determina si fue llamada por una interrupcion ( estas lo indican poniendo las
// variables intMcpX en TRUE) o no.
// Si fue llamada por una interrupcion, ejecuta la funcion correspondiente.
// Resetea el watchdog y vuelve al loop
// Podemos poner un debounce de 20ms por lo que la fmax. con que detecta pulsos queda en 50hz
// pero hay algunos caudalimetros cuyo pulso es de 5ms por lo que los perderiamos.
//

( void ) pvParameters;
s08 initStatus = FALSE;

	while (! initStatus ) {
		if (xSemaphoreTake( sem_systemVars, MSTOTAKESYSTEMVARSSEMPH ) == pdTRUE ) {
			initStatus = systemVars.initStatus;
			xSemaphoreGive( sem_systemVars );
		}
		vTaskDelay( 100 / portTICK_RATE_MS );
	}

	vTaskDelay(1000 / portTICK_RATE_MS);
	snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigitalIn::Task starting...\r\n"));
	logPrintStr(LOG_NONE, pv_digitalBuff );

	din0.pCount = 0;
	din1.pCount = 0;
	MCP_queryDcd(&systemVars.dcd);	// DCD status inicial

	for( ;; )
	{
		// Para medir el uso del stack
		tkDigitalIn_uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );

		// Apago el watchdog
		clearWdg(WDG_DIGITAL);

		// Block waiting for the semaphore to become available
		// Me bloqueo no mas de 500ms.
		if( xSemaphoreTake( sem_digitalIn, (portTickType)( 500 / portTICK_RATE_MS )  ) == pdTRUE )
		{
			// Debounce de 20ms ( fmax = 50hz)
			// Lo eliminamos porque hay caudalimetros que el pulso es de 5ms.
			//vTaskDelay( (portTickType)( 20 / portTICK_RATE_MS ) );

			// Caudalimetros.
			if ( intMcpQaudal ) {
				intMcpQaudal = FALSE;
				checkCaudalInputs();
			}

			// DCD
			if ( intMcpDcd ) {
				intMcpDcd = FALSE;
				checkDcdInput();
			}

		}
	}

}
/*------------------------------------------------------------------------------------*/
void checkDcdInput(void)
{
s08 retS;
u08 regValue;

	// Leo las entradas digitales del MCP0( con esto rearmo la interrupcion)
	// El DCD esta en el MCP de la placa logica que es un MCP23008 siempre.
	// Solo seteo la variable general systemVars.dcd para que el resto del sistema pueda usarla.
	//
	retS = MCP_read( MCPADDR_GPIO, MCP_ADDR0 , 1, &regValue);
	if (retS) {
		// Determino el nivel logico del pin correspondiente al DCD.
		systemVars.dcd = ( regValue & _BV(1) ) >> 1;		// bit1, mask = 0x02
		if (systemVars.dcd == 0 ) {
			snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigital::dcd ON(%d)\r\n\0"), systemVars.dcd);
		} else {
			snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigital::dcd OFF(%d)\r\n\0"), systemVars.dcd);
		}
	} else {
		// Si no pude leer el MCP, reseteo el bus I2C deshabilitando la interfase TWI
		TWCR &= ~(1 << TWEN);
		snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigital::INT3 FAIL\r\n\0"));
	}

	sp5K_debugPrintStr( D_DIGITAL, pv_digitalBuff );
	logPrintStr(LOG_INFO, pv_digitalBuff);

}
/*------------------------------------------------------------------------------------*/
void checkCaudalInputs(void)
{

	/* Me interesa detectar solo los flancos de subida. Para esto cuando se genera una interrupcion,
	 * leyendo INTCAP determino cual pin la disparo.
	 * Leyendo el GPIO, veo como esta el nivel de los pines. Si el nivel es 0, es que el flanco fue
	 * de bajada y lo descarto. Si esta en 1 fue de subida con lo que incremento el contador de pulsos.
	 * Dado que la interrupcion se genera 2 veces por pulso: una cuando sube y otra cuando baja
	 * podemos diferenciar ambos casos y medir el ancho de los pulsos.
	 *
	 * Dado que tenemos 2 modelos de placas de datos ( una con un MCP23008 y la nueva con un MCP23018),
	 * para leerlas debemos diferenciar cual tenemos.
	 * Luego determino el nivel logico de c/u.
	 * Las interrupciones generadas son del tipo on-change, de modo que se generan al pasar el pin de 0->1
	 * o de 1->0. Como a mi me interesan solo los flancos de subida ( 0->1), leo INTCAP ( me dice que pin
	 * genero la interrupcion) y GPIO ( me dice el nivel del pin). Cuando ambos (bits) son 1, es que tuve
	 * en el pin un flanco de subida; ahi lo cuento y habilito al debug.
	 *
	 */

s08 retS = FALSE;
u08 intValue, gpioValue;
u08 pos;
s08 debugQ = FALSE;
u08 pin;

	switch(mcpDevice) {
	case MCP23008:
		// Leo el INTCAP y con  esto rearmo la interrupcion. INTCAP me dice quien genero la interrupcion.
		retS = MCP_read( MCPADDR_INTCAP, MCP_ADDR1 , 1, &intValue);
		// Leo el GPIO para ver si el flanco es de subida o bajada.
		retS = MCP_read( MCPADDR_GPIO, MCP_ADDR1 , 1, &gpioValue);
		// Din 0
		pin = ( gpioValue & 0x04) >> 2;
		if ( pin == 1) { din0.pCount ++; debugQ = TRUE;	}
		// Din 1
		pin = ( gpioValue  & 0x08) >> 3;
		if ( pin == 1) { din1.pCount ++; debugQ = TRUE;	}
		break;
	case MCP23018:
		// Leo las entradas digitales del MCP2( con esto rearmo la interrupcion)
		retS = MCP_read( MCP2_INTCAPB, MCP_ADDR2 , 1, &intValue);
		//retS = MCP_read( MCP2_GPIOB, MCP_ADDR2 , 1, &gpioValue);
		// Din 0
		pin = ( intValue & 0x40) >> 6;
		if ( pin == 1) { din0.pCount ++; debugQ = TRUE;	}
		// Din 1
		pin = ( intValue  & 0x20) >> 5;
		if ( pin == 1) { din1.pCount ++; debugQ = TRUE;	}
		break;
	}

	if (retS) {
		pos = snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigital::din0=%.0f,din1=%.0f\r\n"), din0.pCount, din1.pCount);
	} else {
		// Error de lectura
		// i2c_disable
		TWCR &= ~(1 << TWEN);
		din0.pCount = -1;
		din1.pCount = -1;
		debugQ = TRUE;
		snprintf_P( pv_digitalBuff,CHAR64,PSTR("tkDigital::INT1 FAIL\r\n\0"));
	}

	if ( debugQ ){	sp5K_debugPrintStr( D_DIGITAL, pv_digitalBuff ); }

}
/*------------------------------------------------------------------------------------*/
void readDigitalCounters( float *count0, float *count1, s08 resetCounters )
{
	// Devulevo el valor de los contadores de pulsos.
	// Dependiendo de resetCounters los pongo a 0 o no.

	*count0 = din0.pCount;
	*count1 = din1.pCount;

	if (resetCounters) {
		din0.pCount = 0;
		din1.pCount = 0;
	}

}
/*------------------------------------------------------------------------------------*/

/*
 * Archivo: avlfuel02r2a_uart0handler.c
 * Fecha: 11-Abr-2008
 *
 * Handlers de interrupcion y funciones auxiliares para acceder
 * a la UART0.
 * Los modulos del UART0 son:
 *
 * - funcion de inicializacion del UART0
 * 	void xUart0Init(void )
 * - handler de trasmision:
 * 	SIGNAL( SIG_UART0_DATA )
 * - funcion para trasmitir accediendo al handler de Tx
 * 	s08 xUart0PutChar( u08 cOutChar, portTickType xBlockTime )
 * - handler de recepcion: Los datos lo deja en una cola
 * 	SIGNAL( SIG_UART0_RECV )
 * - funcion para recibir accediendo al handler de Rx.
 * 	s08 xUart0GetChar(u08 *pcRxedChar, portTickType xBlockTime )
 * - funcion de multiplexado del dispositivo asociado a la UART0
 * 	void muxSelect(u08 channel)
 *
 *
*/
/*------------------------------------------------------------------------------------*/
#include "sp5KFRTOS.h"
#include "sp5K.h"
/*------------------------------------------------------------------------------------*/

//xQueueHandle xUart0CharsForTxQueue, xUart0RxedCharsQueue;
xQueueHandle xUart0CharsForTxQueue;
xQueueHandle xUart1CharsForTxQueue, xUart1RxedCharsQueue;

#define RINGBUFFER 		0
#define LINEARBUFFER 	1

u08 bufferType = LINEARBUFFER;

void buffer_init ( buffer_struct *rb);
s08 buffer_empty ( buffer_struct *rb);
s08 buffer_full ( buffer_struct *rb);
char *ringBuffer_get ( buffer_struct *rb, u08 type);
void buffer_put ( buffer_struct *rb, const unsigned char c);
void buffer_flush ( buffer_struct *rb, const s08 clearBuffer);
s08 buffer_getLine (buffer_struct *rb, char *s);

/*------------------------------------------------------------------------------------*/

void buffer_init ( buffer_struct *rb)
{
	memset (rb, 0, sizeof(*rb) );
}
/*------------------------------------------------------------------------------------*/

s08 buffer_empty ( buffer_struct *rb)
{
	return ( 0 == rb->count );
}
/*------------------------------------------------------------------------------------*/

s08 buffer_full ( buffer_struct *rb)
{
	return ( rb->count >= uart0RXBUFFER_LEN );
}
/*------------------------------------------------------------------------------------*/

char *buffer_get ( buffer_struct *rb )
{

char c;

	// Si no hay datos retorno NULL.
    // Por esto no tengo que controlar el overrun.

	if (rb->count > 0 )
	{
		c = rb->buffer[ rb->tail ];
		--rb->count;
		rb->tail = rb->tail + 1;

		if ( bufferType == LINEARBUFFER ) {
			// Buffer lineal vacio ?
			if ( rb->tail == rb->head ) {
				rb->tail = 0;
				rb->head = 0;
			}
		} else {
			// Avanzo en modo circular
			if ( rb->tail >= uart0RXBUFFER_LEN ) {
				rb->tail = 0;
			}
		}
		return (c);
    }
    else {
      return(NULL);
    }

}
/*------------------------------------------------------------------------------------*/

void buffer_put ( buffer_struct *rb, const unsigned char c )
{
	// Si el buffer esta lleno, descarto el valor recibido
	// Guardo en el lugar apuntado por head y avanzo.

	if ( rb->count < uart0RXBUFFER_LEN )
    {
		rb->buffer[rb->head] = c;
		++rb->count;
		// Avanzo en modo circular.
		rb->head = rb->head + 1;

		if ( bufferType == RINGBUFFER ) {
			if ( rb->head >= uart0RXBUFFER_LEN ) {
				rb->head = 0;
			}
		}
    }
}
/*------------------------------------------------------------------------------------*/

void buffer_flush ( buffer_struct *rb, const s08 clearBuffer)
{
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;

	if (clearBuffer) {
		memset (rb->buffer, '\0', sizeof (rb->buffer));
	}
}
/*------------------------------------------------------------------------------------*/

s08 buffer_getLine (buffer_struct *rb, char *s )
{
	// Devuelve lineas enteras del buffer.
	// El s debe tener capacidad suficiente.

s08 res = FALSE;
char cChar;

	if ( buffer_empty ( &xUart0RxedCharsBuffer )) {
		// No hay datos.
		res = FALSE;
	} else {
		// Copio vaciando el ringBuffer hasta encontrar un CR.
		while(1) {
			cChar = buffer_get(&xUart0RxedCharsBuffer);
			*(s++) = cChar;
			if ( (cChar == '\n') || (cChar == NULL))
				break;
		}
		*(s++) = '\0';
		res = TRUE;
	}
	return (res);

}
/*------------------------------------------------------------------------------------*/

void xUartEnable(u08 nUart)
{
	if (nUart == 0) {
		UCSR0B |= BV(RXCIE0)| BV(RXEN0)| BV(TXEN0);
		return;
	}

	if (nUart == 1) {
		UCSR1B |= BV(RXCIE1)| BV(RXEN1)| BV(TXEN1);
		return;
	}
}
/*------------------------------------------------------------------------------------*/
void xUartDisable(u08 nUart)
{
	if (nUart == 0) {
		UCSR0B &= ~BV(RXCIE0) & ~BV(RXEN0) & ~BV(TXEN0);
		return;
	}

	if (nUart == 1) {
		UCSR1B &= ~BV(RXCIE1) & ~BV(RXEN1) & ~BV(TXEN1);
		return;
	}
}
/*------------------------------------------------------------------------------------*/
void xUartInit( u08 nUart, u32 baudrate)
{
/* Inicializo el UART para 9600N81.
 * y creo las colas de TX y RX las cuales van a ser usadas por los
 * handlers correspondientes.
 *
 * El valor del divisor se calcula para un clock de 8Mhz.
 * Usamos siempre el doblador de frecuencia por lo que U2Xn = 1;
 *
*/
u16 bauddiv;
s08 doubleSpeed = FALSE;

	portENTER_CRITICAL();

	switch (baudrate) {
	case 9600:
		bauddiv = 103;
		doubleSpeed = TRUE;
		break;
	case 115200:
		bauddiv = 8;
		doubleSpeed = TRUE;
		break;
	}

	switch (nUart) {
	case 0:
		//xUart0RxedCharringBuffer_emptysQueue = xQueueCreate( uart0RXBUFFER_LEN, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
		buffer_init(&xUart0RxedCharsBuffer);
		xUart0CharsForTxQueue = xQueueCreate( uart0TXBUFFER_LEN, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
#ifdef MEMINFO
	HE_Uart0TxQueue = memHeapEnd();
#endif

		//
		outb(UBRR0L, bauddiv);
		outb(UBRR0H, bauddiv>>8);
		// Enable the Rx interrupt.  The Tx interrupt will get enabled
		// later. Also enable the Rx and Tx.
		// enable RxD/TxD and interrupts
		outb(UCSR0B, BV(RXCIE0)| BV(RXEN0)|BV(TXEN0));

		// Set the data bits to 8N1.
		UCSR0C = ( serUCSRC_SELECT | serEIGHT_DATA_BITS );

		// 	DOUBLE SPEED
		if (doubleSpeed)
			outb(UCSR1A, BV(U2X0));

		break;
	case 1:
		xUart1RxedCharsQueue = xQueueCreate( uart1RXBUFFER_LEN, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
#ifdef MEMINFO
	HE_Uart1RxQueue = memHeapEnd();
#endif

		xUart1CharsForTxQueue = xQueueCreate( uart1TXBUFFER_LEN, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
#ifdef MEMINFO
	HE_Uart1TxQueue = memHeapEnd();
#endif

		//
		outb(UBRR1L, bauddiv);
		outb(UBRR1H, bauddiv>>8);
		// Enable the Rx interrupt.  The Tx interrupt will get enabled
		// later. Also enable the Rx and Tx.
		// enable RxD/TxD and interrupts
		outb(UCSR1B, BV(RXCIE1)| BV(RXEN1)|BV(TXEN1));

		// Set the data bits to 8N1.
		UCSR1C = ( serUCSRC_SELECT | serEIGHT_DATA_BITS );

		if (doubleSpeed)
			outb(UCSR1A, BV(U2X1));

		break;
	}

	portEXIT_CRITICAL();

	return;
}

/*------------------------------------------------------------------------------------*/
/*
 * Handler (ISR) de TX.
 *
*/
ISR( USART0_UDRE_vect )
{
/* El handler maneja una cola en la cual el resto del sistema debe escribir
 * los datos que quiere que se trasmitan.
 * Esto se hace con la funcion xUart0PutChar la cual llena la cola y prende la
 * flag de interrupcion.
 * La rutina de interrupcion ejecuta este handler (SIGNAL) en el cual si hay
 * datos en la cola los extrae y los trasmite.
 * Si la cola esta vacia (luego del ultimo) apaga la interrupcion.
*/

s08 cTaskWoken;
u08 cChar;

	if( xQueueReceiveFromISR( xUart0CharsForTxQueue, &cChar, &cTaskWoken ) == pdTRUE )
	{
		/* Send the next character queued for Tx. */
			outb (UDR0, cChar);
//-->			TXstatusToSleep = 2;

	} else {
		/* Queue empty, nothing to send. */
		vUart0InterruptOff();
//-->		TXstatusToSleep = 1;
	}
}

/*------------------------------------------------------------------------------------*/
/*
 * Handler (ISR) de TX.
 *
*/
ISR( USART1_UDRE_vect )
{
/* El handler maneja una cola en la cual el resto del sistema debe escribir
 * los datos que quiere que se trasmitan.
 * Esto se hace con la funcion xUart0PutChar la cual llena la cola y prende la
 * flag de interrupcion.
 * La rutina de interrupcion ejecuta este handler (SIGNAL) en el cual si hay
 * datos en la cola los extrae y los trasmite.
 * Si la cola esta vacia (luego del ultimo) apaga la interrupcion.
*/

s08 cTaskWoken;
u08 cChar;

	if( xQueueReceiveFromISR( xUart1CharsForTxQueue, &cChar, &cTaskWoken ) == pdTRUE )
	{
		/* Send the next character queued for Tx. */
			outb (UDR1, cChar);
//-->			TXstatusToSleep = 2;

	} else {
		/* Queue empty, nothing to send. */
		vUart1InterruptOff();
//-->		TXstatusToSleep = 1;
	}
}

/*------------------------------------------------------------------------------------*/

s08 xUartPutChar( u08 nUart, u08 cOutChar, u08 xBlockTime )
{
/* Esta funcion se usa para trasmitir por el puerto serial.
 * Si puede encolarlos lo hace y luego prende la flag de TXporInterrupcion para que
 * sea el handler de TX quien vacie la cola y saque los datos por el puerto UART0
*/
s08 sRet = pdFAIL;

	if (nUart == 0) {
		sRet = xUart0PutChar( cOutChar, xBlockTime );
	} else {
		sRet = xUart1PutChar( cOutChar, xBlockTime );
	}
	return(sRet);

}
/*------------------------------------------------------------------------------------*/

s08 xUart0PutChar( u08 cOutChar, u08 xBlockTime )
{
/* Esta funcion se usa para trasmitir por el puerto serial 0.
 * Si puede encolarlos lo hace y luego prende la flag de TXporInterrupcion para que
 * sea el handler de TX quien vacie la cola y saque los datos por el puerto UART0

*/
	// Controlo que halla espacio en la cola.
	while  ( uxQueueMessagesWaiting( xUart0CharsForTxQueue ) > ( uart0TXBUFFER_LEN - 2) )
		taskYIELD();

	//if ( uxQueueMessagesWaiting( xUart0CharsForTxQueue ) > ( uart0TXBUFFER_LEN - 2) )
	//	taskYIELD();


	if( xQueueSend( xUart0CharsForTxQueue, &cOutChar, xBlockTime ) != pdPASS ) {
		return pdFAIL;
	}
	vUart0InterruptOn();
	return pdPASS;
}
/*------------------------------------------------------------------------------------*/

s08 xUart1PutChar( u08 cOutChar, u08 xBlockTime )
{
/* Esta funcion se usa para trasmitir por el puerto serial 0.
 * Si puede encolarlos lo hace y luego prende la flag de TXporInterrupcion para que
 * sea el handler de TX quien vacie la cola y saque los datos por el puerto UART0

*/
	// Controlo que halla espacio en la cola.
	if ( uxQueueMessagesWaiting( xUart1CharsForTxQueue ) > ( uart1TXBUFFER_LEN - 2) )
		taskYIELD();

	if( xQueueSend( xUart1CharsForTxQueue, &cOutChar, xBlockTime ) != pdPASS ) {
		return pdFAIL;
	}
	vUart1InterruptOn();
	return pdPASS;
}
/*------------------------------------------------------------------------------------*/

s08 cmdUartPutChar( u08 cOutChar )
{
/* Esta funcion se usa para trasmitir por el puerto serial CMD. (UART1)
 * Si puede encolarlos lo hace y luego prende la flag de TXporInterrupcion para que
 * sea el handler de TX quien vacie la cola y saque los datos por el puerto UART0

*/
		if( xQueueSend( xUart1CharsForTxQueue, &cOutChar, 10 ) != pdPASS ) {
			return pdFAIL;
		}
		vUart1InterruptOn();
		return pdPASS;
}
/*------------------------------------------------------------------------------------*/

/*
 * Handler (ISR) de RX.
 *
*/
ISR( USART0_RX_vect )
{
/* Este handler se encarga de la recepcion de datos.
 * Cuando llega algun datos por el puerto serial lo recibe este handler y lo va
 * guardando en la cola de recepcion
*/

s08 cChar;

	/* Get the character and post it on the queue of Rxed characters.
	 * If the post causes a task to wake force a context switch as the woken task
	 * may have a higher priority than the task we have interrupted.
    */
	cChar = inb(UDR0);
	if (bufferType == RINGBUFFER ) {
		if ( xSemaphoreTakeFromISR ( sem_GprsUart, NULL) ) {
			buffer_put ( &xUart0RxedCharsBuffer, cChar);
			xSemaphoreGiveFromISR( sem_GprsUart, NULL);
		}
	} else {
		buffer_put ( &xUart0RxedCharsBuffer, cChar);
	}

}
/*------------------------------------------------------------------------------------*/
/*
 * Handler (ISR) de RX.
 *
*/
ISR( USART1_RX_vect )
{
/* Este handler se encarga de la recepcion de datos.
 * Cuando llega algun datos por el puerto serial lo recibe este handler y lo va
 * guardando en la cola de recepcion
*/

s08 cChar;

	/* Get the character and post it on the queue of Rxed characters.
	 * If the post causes a task to wake force a context switch as the woken task
	 * may have a higher priority than the task we have interrupted.
    */
	cChar = inb(UDR1);
//-->	resetRXsecsToSleep(SECSTORXGOSLEEP);

	if( xQueueSendFromISR( xUart1RxedCharsQueue, &cChar, pdFALSE ) )
	{
		taskYIELD();
	}
}
/*------------------------------------------------------------------------------------*/

s08 xUartGetChar(u08 nUart, u08 *pcRxedChar,u08 xBlockTime )
{
/* Es la funcion que usamos para leer los caracteres recibidos.
 * Los programas la usan bloqueandose hasta recibir un dato.
*/

	/* Get the next character from the buffer.  Return false if no characters
	are available, or arrive before xBlockTime expires. */
s08 res = FALSE;

	switch (nUart) {
	case 0:

		if (bufferType == RINGBUFFER ) {

			if ( xSemaphoreTake ( sem_GprsUart, xBlockTime ) ) {
				if ( buffer_empty ( &xUart0RxedCharsBuffer )) {
					res = FALSE;
				} else {
					*pcRxedChar = buffer_get(&xUart0RxedCharsBuffer);
					res = TRUE;
				}
				xSemaphoreGive( sem_GprsUart);
			}
		} else {
			if ( buffer_empty ( &xUart0RxedCharsBuffer )) {
				res = FALSE;
			} else {
				*pcRxedChar = buffer_get(&xUart0RxedCharsBuffer);
				res = TRUE;
			}
		}
		return (res);
		break;
	case 1:
		if( xQueueReceive( xUart1RxedCharsQueue, pcRxedChar, xBlockTime ) ) {
			return pdTRUE;
		} else	{
			return pdFALSE;
		}
		break;
	}

}

/*------------------------------------------------------------------------------------*/

s08 uartQueueIsEmpty(u08 nUart)
{

	switch(nUart) {
	case 0:
		if ( uxQueueMessagesWaiting(xUart0CharsForTxQueue) == 0)
			return (TRUE);
		else
			return(FALSE);

		break;
	case 1:
		if ( uxQueueMessagesWaiting(xUart1CharsForTxQueue) == 0)
			return (TRUE);
		else
			return(FALSE);

		break;
	}
}
/*------------------------------------------------------------------------------------*/

void xUartQueueFlush(u08 nUart)
{

	/* Vacio la cola de recepcion */
	if (nUart == 0) {

		if (bufferType == RINGBUFFER ) {

			if ( xSemaphoreTake ( sem_GprsUart, 10 ) ) {
				buffer_flush ( &xUart0RxedCharsBuffer, TRUE);
				xSemaphoreGive( sem_GprsUart);
			}
		} else {
			buffer_flush ( &xUart0RxedCharsBuffer, TRUE);
		}
		return;
	} else {
		xQueueReset(xUart1RxedCharsQueue);
		//while (xUartGetChar( 1, &cChar, ( portTickType ) 10 ) );
		return;
	}
}
/*------------------------------------------------------------------------------------*/

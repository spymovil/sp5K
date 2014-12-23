/*
 * sp5Klib_demo.c
 *
 * Todas las definiciones del 1284P esta en /usr/lib/avr/include/avr/iom1284P.h
 *
 *  Created on: 14/12/2013
 *      Author: root
 *
 *  sp5KlibFRTOS: Consumo de memoria.
 *  - Asigno 3000 bytes para el Heap y al compilar indica 3207 bytes.
 *  - Si ademas habilito los timers, suma 40 bytes. (3247).
 *
 * Heap:		   3000     // semaforos, tareas, colas.
 * FRTOS ( base):	247
 *                ------
 *                 3247
 *
 * sp5klib.rtc		  8
 * sp5klib..mcp		  0
 * sp5klib.eeprom     0
 * sp5klib.i2c        1
 * var.extras        13
 *                ------
 *                 3269
 *
 * uart2_sp5KFRTOS    8       // Los buffers son queues por lo tanto se crean en el heap y consumen 384 bytes de este
 *                ------
 *                 3277
 *
 * rprintf_sp5KFRTOS  2
 *                ------
 *                 3279
 *
 * avrlib.cmdline	370		// estructuras estaticas por lo que van en el area de .DATA
 *                ------
 *                 3649
 *
 * sp5klib.gprs     193     // estructuras estaticas por lo que van en el area de .DATA
 * main             107     // estructuras estaticas por lo que van en el area de .DATA
 *                ------
 *                 3949
 *
 * Si agrego en FRTOSconfig.h el soporte de TRACE, aumento 60 bytes.
 *
 *
 * HEAP:
 * - scheduler                           =  236
 * - semaforos:		           4 * (76+1)=  308
 * - tkCmd:                    64 + 1500 = 1564
 * - tkControl:                64 +  640 =  704
 * - tkDin:                    64 +  640 =  704
 * - tkData:		           64 + 1000 = 1064
 * - tkGprs:                   64 + 1500 = 1564
 * - uart queues 3*( 76 +128) + (76+640) = 1328
 *                                      ----------
 *                                         6756
 *
 *
 *  Modificaciones para migrar sp5Klib a sp5KlibFRTOS.
 *  1) Los delays los debemos hacer con la funcion vTaskDelay
 *  2) El acceso al bus I2C debe arbitrarse con un semaforo. Este va en
 *     las funciones de read y write del I2C.
 *     Aunque en MCP hay funciones de read/modify, no es problema ya que no
 *     se accede en forma concurrente.
 *  3) Creo una funcion FRTOSi2c_init donde inicio el semaforo.
 *  4) Creo la funcion dRprintf para manejar los debugs. El problema que me genera
 *     el flash del led lo elimino usando una flag que si tengo el debug prendido, no
 *     flashee el led.
 *  4) Idem el acceso a las uarts ( rprintf's)
 *  5) Las esperas debo hacerlas con taskYIELD();
 *  6) El modelo de memoria recomendado es el heap_3.c por lo tanto no se requiere setear
 *     la constante configTOTAL_HEAP_SIZE.

 *
 *  Arranque:
 *  Todas las tareas deben arrancar despues de tk_cmd que es la que inicializa el systemVars.
 *  Para eso uso la variable sVars.initStatus que arranca siempre FALSE y frena a las tareas de arrancar
 *  hasta que es seteado por tk_cmd.
 *
 *  Timers:
 *  Si bien podemos usar los timers del FRTOS, preferimos crear las rutinas necesarias para de este modo
 *  incluir ciertas facilidades propias ( leer el valor del contador en c/momento ).
 *  Si bien no van a ser de uso general, cumplen los requisitos que necesitamos.
 *
 *  FLOATS:
 *  Pasa usar floats y que no se muestren como ?, debemos compilar con la libreria prontf_flt.
 *  Para esto al configurar, en AVR C Linker-> General -> Other Arguments ponemos "-Wl,-u,vfprintf -lprintf_flt -lm"
 *  y no ponemos las librerias en forma individual.
 *  Ref: http://stackoverflow.com/questions/905928/using-floats-with-sprintf-in-embedded-c
 *  Ref: http://winavr.scienceprog.com/avr-gcc-tutorial/using-sprintf-function-for-float-numbers-in-avr-gcc.html
 *  Ref: http://www.nongnu.org/avr-libc/user-manual/group__avr__stdio.html#ga2b829d696b17dedbf181cd5dc4d7a31d
 *
 *  Analisis de memoria:
 *  Ejecutar  avr-nm -CnS sp5K.elf > a.out
 *
 *  Consumo:
 *  - Suponiendo que polea c/5mins y trasmite c/3hs.
 *  - El modem prendido consume 100mA => 1.66mA/h
 *  - En modo sleep consume 1.8mA => 1.8mA/h
 *  - Poleando asumimos que consume 20mA => 1.33mA/H
 *
 *  Consumo total = 1.66 + 1.8 + 1.33 = 4.7mA/h
 *  1Ah al 80% corresponde a 170hs
 *  90 dias = 2160hs = 12Amp/h.
 *
 *  Mejora:
 *  - Trasmitir 1vez/dia: modem => 0.2mAh
 *  - 10s duracion del poleo: => 0.665mAh
 *  Consumo = 0.2 + 1.8 + 0.665 = 2.66mAH ==> 1AH alcanza p/300hs
 *
 *
 *
 *
 *=======================================================================================================
 *  Pendiente:
 *  -Si en los primeros segs. escribo un comando, a veces se resetea.
 *  -No funciona el history de comandos.
 *
 * ******************************************************************************************************
 *  Version 2.0.10: @2014-12-12
 *  - Agrego la funcionalidad de monitorear el SQE.
 *  - Agrego la funcionalidad  de monitorear el frame.(modifico la tkData FSM )
 *  - Las pruebas de estabilidad de la medida dan un error inferior al 0.5% FS..
 *  - Cambio el ciclo de poleo a 3.
 *  - Utilizo el repositorio de GIT
 *
 *  Version 2.0.9: @2014-11-28
 *  - Agrego el manejo de consignas.
 *    Solo permito doble consigna.
 *  - Ajusto el counterState en tkGprs para que no de error a la 1/2 hora de estar en idle.
 *  - En base a pruebas de trasmision de datos, vemos que es mejor trasmitir de a 1 frame y cerrar el socket.
 *    Implemento este modo.
 *  - Utilizo una variable dummyCicle en tkData para que al arrancar, en el primer poleo a los 10s no lo guarde
 *    en memoria ya que en el caso del contador de pulsos, queda un valor falso.
 *  - Cambio la forma de mostrar la hora del modo debug
 *  - En el comando E2IPS pongo un timeout del socket de 15s ( bajo los 30 ) porque la VPN demora en arrancar la
 *    primera vez, pero no la segunda.
 *  - Arreglo un bug que hacia que nunca reintentara reabrir el socket.
 *
 *  Version 2.0.8: @2014-10-16
 *  - Modifico la tarea tkData ya que el poleo toma el doble de tiempo que lo que se configura.
 *
 *  Version 2.0.7: @2014-10-08
 *  - Arreglo la medida de pulsos digitales que no estaba funcionando.
 *  - Modifico la FSM de tkGPRS para hacer mas claro las transiciones.
 *  - Soluciono un bug en la medida de pulsos en placas con MCP23018.
 *  - Mejoro la estabilidad del conversor A/D agregando un ciclo dummy que prenda el ADC
 *    y le de tiempo de estabilizarse antes de comenzar las medidas.
 *
 *  Version 2.0.6: @2014-10-04
 *  - Agrego el uso de los leds de la placa superior para keepAlive y modem
 *  - Modifico el control de stack para que resetee al micro en caso de error
 *  - Agrego una funcion de comando ( debug ) que muestre el stack libre de c/tarea.
 *  - Agrego una funcion en la tarea de control que resetea al dlg cada 24hs.
 *  - Mejoro la funcion xUart0putChar para que espere hasta que halla lugar en la cola.
 *  - Aumento el tamanio del buffer de la tarea tkGprs para que entre en el INIT todo el frame de 8 canales
 *    y paso la variable pos a 16 bits. Sin perjuicio de esto trasmito el frame de INIT en varios tramos.
 *
 *	Version 2.0.4: (  BRANCH: OSE 2014-09-06)
 *	- Arreglos sobre la version 2.0.3 ( inestable). Presenta problemas de stackOverflow, desconexiones
 *	  y queda colgado con lo que no trasmite mas.
 *	- Emprolijado de codigo en varias funciones.
 *	- En vApplicationStackOverflowHook agrego que se resetee.
 *	- Modifico tkData para que quede mas claro el manejo de los timeSlots. Reescribo las rutinas de control
 *	  de sleep, led, terminal.
 *	- TkGprs: Agrego que al cerrar un socket espere 30s o hasta que caiga DTR.
 *	  Agrego un control de las veces que intente trasmitir el mismo bulk y al expirar asumo un error del server
 *	  y termino.
 *	- Otro problema presentado es que el server sp5K demora 2s/linea aprox. en procesarla por lo que en trasmisiones
 *	  bulk quedamos en el limite del timeout del socket. Esto se debe arreglar en el server. De nuestro lado aumentamos
 *	  el timer de espera luego del tail a 30s.
 *
 *
 *  Version 2.0.2 (BRANCH: OSE)
 *  - Cambio el nombre del proyecto a sp5K_3CH
 *  - Si al leer detecta un problema en la memoria, borra el registro.
 *  - Cambio el stack para que apuente la direccion inicial 892243093
 *
 *  Version 2.0.1 (BRANCH: OSE)
 *  - Elimino todo lo relativo a valvulas para llegar a un sistema estable.
 *  - Solo considero 3 canales analogicos.
 *  - Caudalimetros Krohne: el ancho de pulso que generan es de 5ms por lo que debo eliminar el
 *    debounce de 20ms para detectarlos.
 *  - Elimino todo lo referente a los pulsos excepto el contarlos.
 *  - Agrego en sendInitFrame() que mande la configuracion de los canales al inicio.
 *
 *    BRANCH: OSE

 *  Version 2.0.0  (17/08/2014)
 *  - Restauro el repositorio creando uno nuevo.

 *  Version 1.9:
 *  Branch sp5K_valvulas
 *  Consumos:
 *  - La placa LOGIC sola en modo sleep consume 0.7mA
 *  - Con la placa inferior el consumo total queda en 2mA.
 *  - Con solo el MCP23018 el consumo total es de 1mA
 *
 *  * V1.9.1:
 *  - Consignas:
 *    Considero trabajar con 2 modalides: doble | continua.
 *  - tkCmd: ingreso tipo de consigna/ valores de consigna.
 *  - tkGprs: -al enviar el frame de init, envio las consignas.
 *            -falta recibir del server las consignas
 *  - tkData: reescrita para contemplar slots adaptativos de muestreo.
 *  - bug en la deteccion del MCP.
 *
 *  * V1.9.0:
 *  - Incorporo cambios para manejar las valvulas. ( MCP23018 ).
 *    - Inicializo
 *    - Detecto que tipo de MCP tengo.
 *    - Funciones de MCP para c/linea
 *    - funciones en tkCmd.
 *
 *  Version 1.8:
 *  - Incorporo un subversionado para considerar los arreglos menores en una version.
 *
 *  * V1.8.8:
 *  - Permito que la direccion del server este en modo name o IP.
 *  - Elimino las funciones de sp5KlibFRTOS/gprs y las incorporo a la tkCmd.
 *  - Elimino el comando gprs e introduzco sus funciones en write.
 *  - Reduzco codigo poniendo el debug del modulo I2C dentro de #defines de MEMINFO
 *  - Para poder reconocer datos en el programa de configuracion, todos los mensajes de log/debug
 *    (descartan) llevan al ppio un '>' o un '-'.
 *
 *  * V1.8.7
 *  - Incorporo un modo de escritura en EE que es pageWrite.
 *  - Mejoro el tema de las versiones usando defines.
 *  - Para el caso de usar internaEE el tamano del registro es de 42 bytes para optimizar la memoria
 *  - Cuando uso external EE el registro es de 64 bytes: una pagina.
 *  - Incoporo logs en sp5K_i2c.
*
 *  * V1.8.6
 *  - El problema con la memoria externa es que al escribir en modo page, debo tener cuidado con las
 *    fronteras de pagina ya que esto es lo que me genera write-warp.
 *    Ajusto para usar un registro unico de 64 bytes.
 *
 *  * V1.8.5:
 *  - El tamanio del registro se determina por un #define con lo cual se debe recompilar para el caso de UTE.
 *    Se elimina el channel number.
 *  - Modifico la forma de acceder a memoria: incorporo un checksum y uso un union con 2 bytes mas que el
 *    framedatatype para escribir en estos los EMPTYMARKS y asi hacer un solo acceso a escritura en EE.
 *  - Incorporo un modo testing en el cual utilizo la EE interna. Con esto puedo determinar si los problemas
 *    de memoria son del algoritmo o de la memoria externa ( o su acceso ). Es este modo funciona correctamente.
 *    Usando la memoria externa presenta problemas al leerla.
 *
 *  * V1.8.4:
 *  - Arreglos menores de formatos de mensajes de debug.
 *  - En OSE los registros de BD son de 41 bytes por lo que si considero  registros de 42 bytes gano un 52%
 *    de capacidad (aprox 3 dias).
 *  - Si se apaga la termial, al prenderla no puede trabajar sin el iman. No reinicia el timer de la terminal.
 *
 *  * V1.8.3:
 *  - De la simulacion vemos que los sprintf y rprintf no consumen mas de 70 bytes del stack
 *    lo que nos permite ajustar los mismos de las tareas para optimizar.
 *  - En el borrado de la memoria marco todos los bytes del bloque.
 *  - Controlo en g90 que el numero de registro que pido avanze en modo circular.
 *
 *  * V1.8.2:
 *  - Arreglos los punteros de la BD ya que tengo el error (memory: wrPtr=314, rdPtr=315,
 *    usedRec=1334, freeRecs=-822 ).
 *
 *  * V1.8.1:
 *  - Incorporo que en el frame de init se envie la version de firmware.
 *  - Agrego un modo MEMINFO en el cual mido el heap luego de iniciar c/semaforo, c/queue, c/tarea y lo
 *    muestro al arrancar. Con esto puedo tener una vision mas exacta del uso de la memoria.
 *  - Controlo el stack de c/tarea: Para esto defino una variable que lleve la maxima posicion que alcanzo
 *    el stack de c/tarea. Modifico la macro de StackMacros.h para que la actualize y asi poder verla
 *    en la tarea de CMD.
 *
 *  * V1.8.0:
 *  Arreglos varios en la tarea de GPRS.
 *  - Trasmite en modo bulk de a 12 frames maximo
 *  - Genero una funcion para los mensajes de debug de modo de no reescribir las instrucciones en c/estado
 *  - Elimino los mensajes de debug del modulo I2C ( funciona bien).
 *  - Las respuestas del modem las paso a LOG_INFO y no D_GRPS lo que me permite tener apagado el debug pero
 *    poder ver siempre las respuestas del modem.
 *    Los mensajes informativos de tkGPRS los dejo en modo LOG.
 *  - Elimino de tkCmd los mensajes de help y ejemplo de c/comando. Solo dejo el comando HELP.
 *  - Elimino el modo UTE: quedan solo 2 modos, 3 y 8. Cambio el reset a default 8 con lo que pasa a 8 canales.
 *    En este reset inicializo todas sus variables.
 *  - Cambio los strcpy y memcpy de strings por strncpy_P para evitar overflows.
 *  - Modifico la rutina de conteo de pulsos para considerar cuando dura mas de 1 intervalo.
 *
 *  Version 1.13:
 *  - Modifico el script de conexion a sp5K.pl
 *  - 1.13c: Trasmito en modo bulk.
 *  - 1.13d: Borro todas las funciones de la libreria del manejo del GPRS. Solo dejo las necesarias.
 *  - Puede ser necesario usar la flag del compilador -fomit-frame-pointer.
 *
 *  Version 1.10:
 *  Arreglos en la conversion A/D.
 *  - Promedio en 10 medidas
 *  - En modo discreto, prendo la fuente 5s antes para tener mas tiempo de estabilizacion
 *  - Como tomo 10 medidas, las variables de adc son double.
 *  Espero 1s entre frames para no atorar al modem
 *  Agrego una funcion para salir a los 30mins automaticamente del modo service en tkData y tkGPRS.
 *  Arreglo el tema del consumo: si bien baja el consumo al entrar al modo idle, demora bastante luego
 *  de haberse apagado la terminal.
 *  Lo que hago es que inmediatamente que se apago la terminal, se pueda entrar en este modo.
 *  Modifico la forma de analizar las respuestas del modem solo haciendolo cuando se necestia y no en cada
 *  ciclo de eventos.
 *  Acorto los tiempos de espera de algunos comandos de 5s a 2s.
 *  Agrego una salida por reload config del estado bd01 para no repetir el frame de datos luego del inicio si
 *  requiere resetear la configuracion
 *
 *  Version 1.09:
 *  - Saco el watchdog al inicio; esto evita que se reinicie luego de reprogramarlo
 *  - Reescribo la tarea tkGprs usando una FSM.
 *
 *  Version 1.08:
 *  - El watchdog lo incio en main para evitar que quede colgado en los inicios.
 *    Agrego clearWdg(WDG_CMD) en bd_init.
 *  - La deteccion del pulso de la terminal no funciona bien por lo que cambio el hardware
 *    de modo de usar el pin PB7 y lo saco del MCPxxxxxx. La rutina de control la pongo en la
 *    tarea tkControl.
 *
 *  Version 1.07:
 *  - Agrego un help contextual en el WRITE PARAM
 *  - Agrego el nivel de las entradas digitales.
 *  - Modifico en magp que el valor entrado sea float, de modo de que en el caso de los pulsos
 *    de caudal, entremos en mts3/h y no en lts.
 *
 *  Version 1.06:
 *  - Agrego el comando "read frame".
 *  - Corrijo el bug de poner a 0 las variables antes de empezar a leer los datos
 *  - Agrego el uso de 2 offsets, en 0 (mmin) y en el span (mmax)
 *
 *  Version 1.05:
 *  - Modifico la tarea tkData para implementar un FSM diseñada con IAR.
 *
 *  Version 1.04:
 *  - Agrego en la tarea de Cmd, status, que muestre los ultimos valores registrados(frame).
 *  - Agrego un "reset default ute" de modo que configure los parametros de UTE.
 *  - Modifico la forma de guardar la configuracion del systemVars.
 *  - Al mostrar el status no muestro los valores de los contadores junto a los nombres ya que
 *    ahora estoy mostrando todo el ultimo frame.
 *  - Corrijo el bug al ingresar los nombres de las variables.
 *  - Modifico la forma de mostrar informacion en pantalla. Divido la importancia de los mensajes
 *    en 5 niveles.
 *    Elimino el timerdebug.
 *  - Bug: No borra toda la memoria al resetearla. La causa es que el maxRecords es diferente si se paso a
 *    8 canales por lo que borra de menos.
 *    El problema estaba en que la variable en bd_drop para controlar la cantidad de registros era u08.
 *    Ademas es necesario agregar un delay entre escrituras para que la ee la procese.
 *    Agrego ademas control de errores.
 *  - Bug: En modo continuo no entro en modo sleep ni apago la terminal.
 *  - Implemento el offset en tkData.
 *  - Modifico el offser a double.
 *  - Agrego el debuglevel DIGITAL y agrego debugs en la tarea tkDigital.
 *    Esto nos muestra que la senal RI es muy ruidosa por lo que la elimino del monitoreo del MCP23008
 *    Por otro lado vemos que no se estan detectando bien las senales. ( TERMSW ).
 *  - Bugs en la medida de los pulsos.
 *
 *  Version 1.03
 *  - Modifico la tarea tkData para hacer un uso mas claro de una maquina de estados, la cual
 *    se documenta en el esquema tkData.dia.
 *
 *  Version 1.02
 *  - Al recibir respuestas del server solo chequeaba 1 parametro y salia por returns.
 *    Los elimino para poder chequear todos los parametros.
 *    Agrego el codigo para resetearme.
 *  - Incorporo que el led siempre flashee en modo continuo.
 *
 *  Version 1.01
 *  - Mejoro la presentacion de los datos de status.
 *  - Modifico la estructura de bd eliminando campos innecesarios ( dummy/checksum )
 *  - En la lectura/escritura de la BD acomodo los tamanos de acuerdo al nro. de canales.
 *  - Al hacer un reset memory detengo todas las tareas. ( suspend ).
 *  - Al entrar en modo service, idem y deshabilito el wdg.
 *  - BUG: en modo 8 canales, tkdata imprime mal los datos.
 *  - Al configurar el GPRS, pongo en el E2IPS un socket timeout de 10s para acortar el tiempo
 *    de espera al server.
 *
 *  Version 1.0
 *  +++++++++++
 *  Version 0.20
 *  -Enviar varios frames en la misma trasmision.
 *   Para esto debemos poner el tail HTTP 1/1
 *   c/frame que enviamos nos responde por lo tanto debemos ir viendo las respuestas para confirmar
 *   los ACK de los frames.
 *   En caso de cierre del socket, salimos.
 *   El ultimo frame es el que lleva el "Connection close".
 *  - Agrego en tkCmd resetear el timer de la terminal c/caracter que llega por la uart.
 *  - Arreglo el bug que puede salir del algun comando el gprs (apn, csq) con el modem apagado y
 *    no se esta dando cuenta. Agrego control de errores en estos comandos.
 *
 *  Version 0.19
 *  -Low power ( sleep = 0.8mA ( con placa analogica ) / 2.3mA con placa analogica
 *   El consumo extra lo hacen el OPAMP y el conversor A/D.
 *   La solucion seria poner un control de fuente igual del del GPRS.
 *  -Bugs en la rutina tkData_idle.
 *  -Flash led con terminal on.
 *
 *
 *  Version 0.18
 *  - Reescribo tkGprs reordenando las funciones.
 *  - Arreglo el tema de la inicializacion de la BD. Faltaba calcular la cantidad de registros usados.
 *  - Paso el manejo de strigs a funciones tipo PSTR. // Los dejo en ram porque generan reset esporadicos.
 *  - La libreria sp5KFRTOS_gprs no maneja el watchdog en sus sleeps. La alternativa es cambiarla de modo
 *    que en c/funcion le pasemos como parametro con que funcion hacer el sleep.
 *  - Ajustes menores con la visualizacion del timerToDial.
 *
 *  Version 0.17
 *  - tkData:
 *       Elimino el modo service ya que es identico al modo idle.
 *       Modifico para que no use timers sino systemTicks.
 *       Agrego que use el watchdog.
 *  - Elimino los timers ( tkTimer ). Creo una nueva tarea para funciones de control ( tkControl )
 *    Esta se encarga de cuidar los watchdogs, debugTimer, led, etc.
 *  - Agrego las funciones para uso del watchdog.
 *  - En el servidor confirmo siempre que la configuracion se halla enviado al dlg. En caso
 *    que no, se envia un string "RESET" con lo que se fuerza al dlg a mandar un nuevo frame
 *    de init para cargar la configuracion del server.
 *  - Arreglos menores: En modo continuo poder ver el valor de timerdial.
 *  - Ajusto algunos bugs del modulo de BD.
 *
 *  Version 0.16:
 *  - Mejoro los algoritmos del manejo de la memoria.
 *  - Los frames trasmitidos terminan las lineas solo con \n.
 *  - Analizo respuestas del server.
 *    - En los datos normales, borro de la BD basados en la repuesta OK.
 *    - Respuestas de INIT: Reconfiguro los nuevos parametros/chequeo fecha y hora.
 *  - Trasmito frames HTTP1/1
 *
 *  Version 0.15:
 *  - Detecto las respuestas del server y parseo las que son </h1>
 *    ( El problema que habia es que las lineas  html terminan en \n (0A) y no en \r (0D).
 *  - Agrego la funcion que analiza las repuestas pv_analizeSrvRsp.
 *
 *  Version 0.14:
 *  - Las colas de TX tienen 128 bytes por lo tanto en la funcion xUartPutchar controlo que halla lugar en la cola
 *    antes de poner el byte. Si no hay lugar, espero con un taskYIELD.
 *  - La cola de RX del gprs ( uart1) es de 128 bytes pero vemos con wireshark que la pagina de devolucion tiene 330 bytes.
 *  Esto hace que se pierdan bytes.
 *  A 115200, se transmiten 11520 bytes/s, o sea que 350 bytes demoran en llegar aprox 30ms, o sea 3 ticks. Esto hace que no
 *  podamos controlarlo dentro de una tarea.
 *  Hay 2 soluciones: - agrandamos el RX queue / - controlamos en la rutina de interrupcion.
 *  La primer solucion ( mas sencilla ) es poner una RX queue de 512 bytes. El problema es que si es de mas de 128 byte
 *  el programa se cuelga ya que el parametro del tamanio de las colas es unsigned portBASE_TYPE que es char ( 1 byte) por
 *  lo tanto el tamanio maximo son 255 bytes.
 *  Solucion 2: usar un buffer manejado por la rutina de interrupcion de recepcion.
 *  ( OK, modificamos uart2_sp5KFRTOS.c implementando ringBuffers/linearBuffers-> lo determina una variable en uart2. )
 *  - Los strings de comandos de GPRs los pongo como PSTR.
 *
 *  - Respuestas del server.
 *  Vemos que a veces se sale de opensocket con un connect pero el DCD no lo indica ( queda en off), sin embargo el socket esta
 *  abierto y la trasmision es correcta.
 *  Agrego que a c/trasmision espero hasta 5s la respuesta y la imprimo.
 *
 *  Version 0.13
 *  - Al configurar el modem hago que el DCD siga al socket y que no importe el DTR.
 *  - El DCD solo lo maneja la interrupcion del MCP y actualiza el systemVars por lo que para saber su
 *    estado solo leo el systemVars.
 *  - Agrego un frame de init al comienzo.
 *
 *
 *  Version 0.12
 *  -Al reconfigurarme considero como si ingresara al ciclo gprs en modo inicial ( 15s)
 *  -Agrego el modemstatus al cmd status.
 *  -Cambios menores en formato de strings de cmd_status.
 *  -Agrego el D_GPRS(typedef/utils)
 *  -Elimino la flag sp5KlibDebugFlag.
 *
 *  Version 0.11
 *  -C/frame que transmito abro y cierro el socket.
 *
 *  Version 0.10
 *  -Elimino de la BD el control de los checksum en la escritura y la lectura.
 *
 */

#include "sp5K.h"

void initMPU(void);
u32 systemTicks;

//------------------------------------------------------------------------------------

int main(void)
{
unsigned int i,j;

	//----------------------------------------------------------------------------------------
	// Rutina NECESARIA para que al retornar de un reset por WDG no quede en infinitos resets.
	// Copiado de la hoja de datos.
	cli();
	wdt_reset();
	MCUSR &= ~(1<<WDRF);
	/* Write logical one to WDCE and WDE */
	/* Keep old prescaler setting to prevent unintentional time-out */
	WDTCSR |= (1<<WDCE) | (1<<WDE);
	/* Turn off WDT */
	WDTCSR = 0x00;
	sei();

	// Genero un delay de 1s. para permitir que el micro se estabilize.
	// Lleva tiempo a los osciladores estabilizarse.
	for (i=0; i<1000; i++)
		for (j=0; j<1000; j++)
				;

	//----------------------------------------------------------------------------------------

	// inicializo las actividades del micro que no requieren del
	// FRTOS iniciado.
	// Las que lo requieren las inicializo en una tarea ( tkCmd).
	initMPU();
	xUartInit(UART0, 115200);  // GPRS_UART
	xUartInit(UART1, 115200);  // CMD_UART
	i2c_init();		// enable bus I2C

	cmdlineInit();
	cmdlineSetOutputFunc(cmdUartPutChar);
	cmdlineAddCommand("cls", sp5KFRTOS_clearScreen );
	cmdlineAddCommand("help", sp5K_helpFunction);
	cmdlineAddCommand("reset", sp5K_resetFunction);
	cmdlineAddCommand("read", sp5K_readFunction);
	cmdlineAddCommand("write", sp5K_writeFunction);
	cmdlineAddCommand("status", SP5K_statusFunction);
	cmdlineAddCommand("debug", sp5K_stackFunction );

	/* Arranco el RTOS. */

	vSemaphoreCreateBinary( sem_I2Cbus );
#ifdef MEMINFO
	HE_I2Cbus = memHeapEnd();
#endif

	vSemaphoreCreateBinary( sem_CmdUart );
#ifdef MEMINFO
	 HE_CmdUart = memHeapEnd();
#endif

	vSemaphoreCreateBinary( sem_GprsUart );
#ifdef MEMINFO
	HE_GprsUart = memHeapEnd();
#endif

	vSemaphoreCreateBinary( sem_digitalIn );
#ifdef MEMINFO
	HE_digitalIn = memHeapEnd();
#endif

	vSemaphoreCreateBinary( sem_systemVars );
#ifdef MEMINFO
	HE_systemVars = memHeapEnd();
#endif

	vSemaphoreCreateBinary( sem_BD );
#ifdef MEMINFO
	HE_BD = memHeapEnd();
#endif

	/* Creo las tareas */
	xTaskCreate(tkCmd, "CMD", tkCmd_STACK_SIZE, NULL, tkCmd_TASK_PRIORITY, NULL);
#ifdef MEMINFO
	HE_tkCmd = memHeapEnd();
#endif

	xTaskCreate(tkDigitalIn, "DIN", tkDigitalIn_STACK_SIZE, NULL, tkDigitalIn_TASK_PRIORITY, &h_tkDigitalIn);
#ifdef MEMINFO
	HE_tkDigitalIn = memHeapEnd();
#endif

	xTaskCreate(tkData, "DATA", tkData_STACK_SIZE, NULL, tkData_TASK_PRIORITY, &h_tkData );
#ifdef MEMINFO
	HE_tkData = memHeapEnd();
#endif

	xTaskCreate(tkControl, "CTL", tkControl_STACK_SIZE, NULL, tkControl_TASK_PRIORITY, &h_tkControl );
#ifdef MEMINFO
	HE_tkControl = memHeapEnd();
#endif

// DEBUG
	xTaskCreate(tkGprs, "GPRS", tkGprs_STACK_SIZE, NULL, tkGprs_TASK_PRIORITY, &h_tkGprs );
#ifdef MEMINFO
	HE_tkGprs = memHeapEnd();
#endif

	/* Arranco el RTOS. */
	vTaskStartScheduler();

	return 0;
}
/*------------------------------------------------------------------------------------*/

void initMPU(void)
{
//	Inicializa el microcontrolador.
//  Otro tipo de inicializacion que requiere el RTOS corriendo se hacen al
//  iniciar la tkCmd.

	// Inicializacion de Pines
	// Pines Output
	sbi(UARTCTL_DDR, UARTCTL);

	// Leds
	sbi(LED_KA_DDR, LED_KA_BIT);
	sbi(LED_MODEM_DDR, LED_MODEM_BIT);

	// Inicicalmente apagados
	sbi(LED_KA_PORT, LED_KA_BIT);
	sbi(LED_MODEM_PORT, LED_MODEM_BIT);

 	// Pines Input
	cbi(MCP0_INTDDR, MCP0_INT);
	cbi(MCP1_INTDDR, MCP1_INT);
	cbi(TERMSW_DDR, TERMSW_INT);

	sp5K_uartsEnable();	// Habilito el buffer L365.

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// INICIALIZACION ESPECIFICA DEL SP5K
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// Habilito interrupciones del caudalimetro.
	sbi (PCICR, PCIE1);		// PCICR.PCIE1 = 1
	sbi (PCMSK1, PCINT10);	// PCMSK1.PCINT10 = 1

	// Habilito interrupciones del DCD/RI/TERMSW.
	sbi (PCICR, PCIE3);		// PCICR.PCIE3 = 1
	sbi (PCMSK3, PCINT29);	// PCMSK3.PCINT29 = 1

	// Configuro el modo de Sleep.
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);

}
/*------------------------------------------------------------------------------------*/

void vApplicationIdleHook( void )
{

	for(;;) {

		vCoRoutineSchedule();

		// Si tengo una terminal conectada no entro en modo sleep.
		// De otro modo entro.
		if ( ( systemVars.pwrMode == PWR_DISCRETO ) && terminalAllowSleep() && gprsAllowSleep() ) {
			sleep_mode();
		}
	}
}
/*------------------------------------------------------------------------------------*/

void vApplicationTickHook( void )
{
	// Se ejecuta c/tick o sea c/10ms.
	systemTicks++;

	localRtc.tick++;
	if (localRtc.tick == 100 ) {
		localRtc.tick = 0;
		localRtc.sec++;
		if (localRtc.sec == 60 ) {
			localRtc.sec = 0;
			localRtc.min++;
			if (localRtc.min == 60 ) {
				localRtc.min = 0;
				localRtc.hour++;
				if (localRtc.hour == 24) {
					localRtc.hour = 0;
				}
			}
		}
	}

}
/*------------------------------------------------------------------------------------*/

u32 getSystemTicks(void)
 {
	 return(systemTicks);

}
/*------------------------------------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName  )
{
	// Reseteo el micro poniendo el wdg en 30ms y quedandome en loop.

//	rprintf(CMD_UART, "S.OV[%d][%s]\r\n", xTask, pcTaskName);

	wdt_reset();
	wdt_enable(WDTO_15MS);
	while(1) {}
}

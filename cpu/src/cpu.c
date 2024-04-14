#include "cpu.h"

int main(int argc, char* argv[]) {
    
    iniciarLogs();

    iniciarConfig();
    leerConfig();

    iniciarServidoresCpu();
    iniciarConexionCpuMemoria();
    esperarClientes();

    crearHilos();
    
    terminarPrograma();

    return 0;
}

void iniciarLogs() {
    logger_obligatorio_cpu = log_create("logsObligatoriosCPU.log", "LOG_OBLIGATORIO_CPU", true, LOG_LEVEL_INFO);
    logger_aux_cpu = log_create("logsExtrasCPU.log", "LOG_EXTRA_CPU", true, LOG_LEVEL_INFO);
    // Comprobamos que los logs se hayan creado correctamente
    if (logger_aux_cpu == NULL || logger_obligatorio_cpu == NULL) {
        terminarPrograma();
        abort();
    }
}

void iniciarConfig() {
    configuracion_cpu = iniciar_config(rutaConfiguracionCpu);
    // Comprobamos que se haya creado correctamente
    if (configuracion_cpu == NULL) {
        terminarPrograma();
        abort();
    }
}

void leerConfig() {
    if (configuracion_cpu != NULL){
        IP_MEMORIA = config_get_string_value(configuracion_cpu, "IP_MEMORIA");
        PUERTO_MEMORIA = config_get_string_value(configuracion_cpu, "PUERTO_MEMORIA");
        PUERTO_ESCUCHA_DISPATCH = config_get_string_value(configuracion_cpu, "PUERTO_ESCUCHA_DISPATCH");
        PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(configuracion_cpu, "PUERTO_ESCUCHA_INTERRUPT");
        CANTIDAD_ENTRADAS_TLB = config_get_int_value(configuracion_cpu, "CANTIDAD_ENTRADAS_TLB");
        ALGORITMO_TLB = config_get_string_value(configuracion_cpu, "ALGORITMO_TLB");
    } else{
        terminarPrograma();
        abort();
    }        
}

void iniciarServidoresCpu() {
    fd_cpu_dispatch = iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, logger_aux_cpu);
    fd_cpu_interrupt = iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, logger_aux_cpu);
    // Comprobamos que se hayan creado correctamente
    if (fd_cpu_dispatch == -1 || fd_cpu_interrupt == -1) {
        terminarPrograma();
        abort();
    }
}

void iniciarConexionCpuMemoria() {
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, logger_aux_cpu);
    // Comprobamos que se haya creado correctamente
    if (fd_memoria == -1){
        terminarPrograma();
        abort();
    }
}

void esperarClientes() {
    fd_kernel_dispatch = esperar_cliente(fd_cpu_dispatch, logger_aux_cpu);
    fd_kernel_interrupt = esperar_cliente(fd_cpu_interrupt, logger_aux_cpu);
    if (fd_kernel_dispatch == -1 || fd_kernel_interrupt == -1){
        terminarPrograma();
        abort();
    }
}

void atenderKernelDispatch() {
    bool aux_control = 1;

    // While infinito mientras kernel dispatch este conectado al servidor
    // Sale del while cuando se desconecta o si se encuentra con una operacion desconocida
    while (aux_control) {
		int cod_op = recibir_operacion(fd_kernel_dispatch);
		switch (cod_op) {
		case MENSAJE:
			recibir_mensaje(fd_kernel_dispatch, logger_aux_cpu);
			break;
        // Case -1 para salir del while infinito
		case -1:
			log_error(logger_aux_cpu, "Desconexion de Kernel Modo Dispatch");
			// Al setear en 0 en la proxima iteracion ya no entra en el while y sigue ejecutandose el programa
            aux_control = 0;
            break;
		default:
			log_warning(logger_aux_cpu,"Operacion desconocida de Kernel Modo Dispatch");
			break;
		}
	}
}

void atenderKernelInterrupt() {
    bool aux_control = 1;

    // While infinito mientras kernel interrupt este conectado al servidor
    // Sale del while cuando se desconecta o si se encuentra con una operacion desconocida
    while (aux_control) {
		int cod_op = recibir_operacion(fd_kernel_interrupt);
		switch (cod_op) {
		case MENSAJE:
			recibir_mensaje(fd_kernel_interrupt, logger_aux_cpu);
			break;
        // Case -1 para salir del while infinito
		case -1:
			log_error(logger_aux_cpu, "Desconexion de Kernel Modo Interrupt");
			// Al setear en 0 en la proxima iteracion ya no entra en el while y sigue ejecutandose el programa
            aux_control = 0;
            break;
		default:
			log_warning(logger_aux_cpu,"Operacion desconocida de Kernel Modo Interrupt");
			break;
		}
	}
}

void atenderMemoria() {
    bool aux_control = 1;

    // While infinito mientras cpu este conectado a memoria
    // Sale del while cuando se desconecta o si se encuentra con una operacion desconocida
    while (aux_control) {
		int cod_op = recibir_operacion(fd_memoria);
		switch (cod_op) {
		case MENSAJE:
			recibir_mensaje(fd_memoria, logger_aux_cpu);
			break;
        // Case -1 para salir del while infinito
		case -1:
			log_error(logger_aux_cpu, "Desconexion de Memoria");
			// Al setear en 0 en la proxima iteracion ya no entra en el while y sigue ejecutandose el programa
            aux_control = 0;
            break;
		default:
			log_warning(logger_aux_cpu,"Operacion desconocida de Memoria");
			break;
		}
	}
}

void enviarMsjMemoria(){
    enviar_mensaje("Hola, soy CPU!", fd_memoria);
}

void crearHiloKernelDispatch() {
    // Creamos hilo y le decimos que ejecute atenderKernelDispatch en ese hilo (en el 3er argumento puntero a funcion)
    pthread_create(&hilo_kernel_dispatch_cpu, NULL, (void*)atenderKernelDispatch, NULL); // NULL en el segundo argumento para que se seteen los valores por defecto, y NULL en el ultimo argumento porque la funcion no recibe parametros
    // TO-DO Comprobamos que se haya creado correctamente

    // Hacemos que el hilo hilo_kernel_dispatch se desacople del hilo principal y que se siga ejecutando aparte para que el principal pueda seguir ejecutando
    pthread_detach(hilo_kernel_dispatch_cpu);
}

void crearHiloKernelInterrupt() {
    // Creamos hilo y le decimos que ejecute atenderKernelInterrupt en ese hilo (en el 3er argumento puntero a funcion)
    pthread_create(&hilo_kernel_interrumpt_cpu, NULL, (void*)atenderKernelInterrupt, NULL); // NULL en el segundo argumento para que se seteen los valores por defecto, y NULL en el ultimo argumento porque la funcion no recibe parametros
    // TO-DO Comprobamos que se haya creado correctamente

    // Hacemos que el hilo hilo_kernel_dispatch se desacople del hilo principal y que se siga ejecutando aparte para que el principal pueda seguir ejecutando
    pthread_detach(hilo_kernel_interrumpt_cpu);
}

void crearHilosMemoria() {
    // Creamos hilo y le decimos que ejecute enviarMsjMemoria
    pthread_create(&hilo_cpu_memoria, NULL, (void*)enviarMsjMemoria, NULL); // NULL en el segundo argumento para que se seteen los valores por defecto, y NULL en el ultimo argumento porque la funcion no recibe parametros
    // TO-DO Comprobamos que se haya creado correctamente
    
    // Hacemos que el hilo hilo_cpu_memoria se desacople del hilo principal y que se siga ejecutando aparte para que el principal pueda seguir ejecutando
    pthread_detach(hilo_cpu_memoria);

    // Creamos hilo y le decimos que ejecute atenderKernelInterrupt en ese hilo (en el 3er argumento puntero a funcion)
    pthread_create(&hilo_memoria_cpu, NULL, (void*)atenderMemoria, NULL); // NULL en el segundo argumento para que se seteen los valores por defecto, y NULL en el ultimo argumento porque la funcion no recibe parametros
    // TO-DO Comprobamos que se haya creado correctamente

    // Aca no hacemos detach porque no necesitamos que se desacople del principal por ser el ultimo proceso a llamarse
    // Lo que hacemos es decirle al hilo principal que frene aca y no siga hasta que el hilo memoria no termine
    pthread_join(hilo_memoria_cpu, NULL);
}

void crearHilos() {
    crearHiloKernelDispatch();
    crearHiloKernelInterrupt();
    crearHilosMemoria();
}

void terminarPrograma() {
    log_destroy(logger_obligatorio_cpu);
    log_destroy(logger_aux_cpu);
    config_destroy(configuracion_cpu);
    liberar_conexion(fd_cpu_dispatch);
    liberar_conexion(fd_cpu_interrupt);
    liberar_conexion(fd_memoria);
    liberar_conexion(fd_kernel_dispatch);
    liberar_conexion(fd_kernel_interrupt);
    //TO-DO: liberar de memoria los hilos? finalizarlos? pthread_exit()?
}
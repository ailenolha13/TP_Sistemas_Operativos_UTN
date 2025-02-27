#include "./kernel.h"

int main(int argc, char* argv[]){
    // Inicializar variables
    inicializarVariables();
    
    // Handshake
    enviar_handshake();

    // Inicializar consola interactiva
    iniciarConsolaInteractiva();

    // Inicia la planificacion
    iniciarPlanificacion();

    // Escucho las conexiones entrantes
    while(escucharServer(socket_servidor));

    // Liberar espacio de memoria
    terminarPrograma();
    
    return 0;
}

void iniciarPlanificacion() {
    planificacionLargoPlazo();
    planificacionCortoPlazo();
}

void planificacionCortoPlazo() {
    pthread_t CortoPlazoReady;
    pthread_t CortoPlazoBlocked;
    crearHiloDetach(&CortoPlazoReady, (void*) corto_plazo_ready, NULL, "Planificacion corto plazo READY", logs_auxiliares, logs_error);
    crearHiloDetach(&CortoPlazoBlocked, (void*) corto_plazo_blocked, NULL, "Planificacion corto plazo RUNNING", logs_auxiliares, logs_error);
}

bool buscarIO(void* interfaz) {
    interfazConectada* interfazConvertida = (interfazConectada*) interfaz;
    return strcmp(interfazConvertida->nombre, ioBuscada) == 0;
}

bool buscarRecurso(void* recurso) {
    recursoSistema* recursoConvertido = (recursoSistema*) recurso;
    return strcmp(recursoConvertido->nombre, recursoBuscado) == 0;
}

void corto_plazo_blocked() {
    while(1) {
        pthread_mutex_lock(&sem_planificacion);
        while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
            pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
        }
        pthread_mutex_unlock(&sem_planificacion);
        sem_wait(&semContadorColaBlocked);
        pthread_mutex_lock(&sem_cola_blocked_aux);
        t_pcb* pcbBloqueado = queue_pop(cola_blocked_aux);
        pthread_mutex_unlock(&sem_cola_blocked_aux);
        ioBuscada = pcbBloqueado->contexto_ejecucion.io_detail.nombre_io;
        interfazConectada* interfazEncontrada = NULL;
        switch (pcbBloqueado->contexto_ejecucion.io_detail.io_instruccion) {
        case IO_GEN_SLEEP:
            pthread_mutex_lock(&mutexInterfacesGenericas);        
            interfazEncontrada = list_find(interfacesGenericas, buscarIO);
            pthread_mutex_unlock(&mutexInterfacesGenericas); 
            break;
        case IO_STDIN_READ:
            pthread_mutex_lock(&mutexInterfacesSTDIN);        
            interfazEncontrada = list_find(interfacesSTDIN, buscarIO);
            pthread_mutex_unlock(&mutexInterfacesSTDIN);        
            break;
        case IO_STDOUT_WRITE:
            pthread_mutex_lock(&mutexInterfacesSTDOUT);        
            interfazEncontrada = list_find(interfacesSTDOUT, buscarIO);
            pthread_mutex_unlock(&mutexInterfacesSTDOUT);        
            break;
        default:
            pthread_mutex_lock(&mutexInterfacesFS);        
            interfazEncontrada = list_find(interfacesFS, buscarIO);
            pthread_mutex_unlock(&mutexInterfacesFS);        
            break;
        }
        if ( interfazEncontrada == NULL ) {
            log_warning(logs_auxiliares, "interfaz %s no encontrada", ioBuscada);
            agregarPcbCola(cola_exit, sem_cola_exit, pcbBloqueado);
            cambiarEstado(EXIT, pcbBloqueado);
            pcbBloqueado->contexto_ejecucion.motivoFinalizacion = INVALID_INTERFACE;
            sem_post(&semContadorColaExit);
            continue;
        }
        log_info(logs_auxiliares, "Interfaz encontrada %s para PID %d", interfazEncontrada->nombre, pcbBloqueado->contexto_ejecucion.pid);
        pthread_mutex_lock(&(interfazEncontrada->semaforoMutex));        
        queue_push(interfazEncontrada->colaEjecucion, &(pcbBloqueado->contexto_ejecucion.pid));
        pthread_mutex_unlock(&(interfazEncontrada->semaforoMutex));
        sem_post(&(interfazEncontrada->semaforoCantProcesos));
        pthread_mutex_lock(&sem_cola_blocked);  
        list_add(cola_blocked, pcbBloqueado);
        pthread_mutex_unlock(&sem_cola_blocked);
        log_info(logs_obligatorios, "PID: %d - Bloqueado por: %s", pcbBloqueado->contexto_ejecucion.pid, ioBuscada);
    } 
}

//cargamos en el contexto del proceso, el io_detail para las operaciones con las entradas y salidas.
void cargar_io_detail_en_context(t_pcb* pcb, t_list* contexto, int ultimo_indice) {

    ultimo_indice++;
    uint32_t cantidad_parametros_io_detail = *(uint32_t*)list_get(contexto, ultimo_indice);

    for (int i = 0; i < cantidad_parametros_io_detail; i++) {

        ultimo_indice++;
        tipo_de_dato tipo_de_dato_parametro_io = *(tipo_de_dato*) list_get(contexto, ultimo_indice);

        int size_value;
        t_params_io parametro_io_a_value;
        
        ultimo_indice++;
        void* valor_parametro_io_recibido = list_get(contexto, ultimo_indice);

        switch (tipo_de_dato_parametro_io)
        {
        case INT:
            parametro_io_a_value.valor = malloc(sizeof(int));
            *(int*)parametro_io_a_value.valor = *(int*)valor_parametro_io_recibido;
            size_value = sizeof(int);
            log_info(logs_auxiliares, "Se envia el parametro %d", *(int*)valor_parametro_io_recibido);
            break;
        case UINT32:
            parametro_io_a_value.valor = malloc(sizeof(uint32_t));
            *(uint32_t*)parametro_io_a_value.valor = *(uint32_t*)valor_parametro_io_recibido;
            size_value = sizeof(uint32_t);
            log_info(logs_auxiliares, "Se envia el parametro %d", *(uint32_t*)valor_parametro_io_recibido);
            break;
        case STRING:
            int size_parametro_a_guardar = string_length((char *)valor_parametro_io_recibido) + 1;
            parametro_io_a_value.valor = malloc(size_parametro_a_guardar);
            parametro_io_a_value.valor = (char *)valor_parametro_io_recibido;
            size_value = size_parametro_a_guardar;
            log_info(logs_auxiliares, "Se envia el parametro string %s", (char *)valor_parametro_io_recibido);
            break;
        default:
            log_error(logs_error, "Error tipo de dato enviado");
            break;
        }

        parametro_io_a_value.tipo_de_dato = tipo_de_dato_parametro_io; //almaceno el tipo de dato del parametro de la instruccion de io 

        t_params_io* parametro_io_a_guardar = malloc(sizeof(t_params_io));
        parametro_io_a_guardar->tipo_de_dato = parametro_io_a_value.tipo_de_dato;
        parametro_io_a_guardar->valor = malloc(size_value);
        memcpy(parametro_io_a_guardar->valor, parametro_io_a_value.valor, size_value);
        list_add_in_index(pcb->contexto_ejecucion.io_detail.parametros, i, parametro_io_a_guardar); //almaceno el parametro en la lista de parametros que usara kernel luego
    }
    ultimo_indice++;
    
    pcb->contexto_ejecucion.io_detail.nombre_io = (char *)list_get(contexto, ultimo_indice); //obtengo el nombre de la IO
        
    ultimo_indice++;
    pcb->contexto_ejecucion.io_detail.io_instruccion = *(t_nombre_instruccion *)list_get(contexto, ultimo_indice); //obtengo el nombre de la instruccion contra IO
}

void cargar_contexto_recibido(t_list* contexto, t_pcb* pcb) {  
    pcb->contexto_ejecucion.registro_estados = *(uint64_t*)list_get(contexto, 1);
    pcb->contexto_ejecucion.registros_cpu.pc = *(uint32_t*)list_get(contexto, 2);
    pcb->contexto_ejecucion.registros_cpu.ax = *(uint8_t*)list_get(contexto, 3);
    pcb->contexto_ejecucion.registros_cpu.bx = *(uint8_t*)list_get(contexto, 4);
    pcb->contexto_ejecucion.registros_cpu.cx = *(uint8_t*)list_get(contexto, 5);
    pcb->contexto_ejecucion.registros_cpu.dx = *(uint8_t*)list_get(contexto, 6);
    pcb->contexto_ejecucion.registros_cpu.eax = *(uint32_t*)list_get(contexto, 7);
    pcb->contexto_ejecucion.registros_cpu.ebx = *(uint32_t*)list_get(contexto, 8);
    pcb->contexto_ejecucion.registros_cpu.ecx = *(uint32_t*)list_get(contexto, 9);
    pcb->contexto_ejecucion.registros_cpu.edx = *(uint32_t*)list_get(contexto, 10);
    pcb->contexto_ejecucion.registros_cpu.si = *(uint32_t*)list_get(contexto, 11);
    pcb->contexto_ejecucion.registros_cpu.di = *(uint32_t*)list_get(contexto, 12);
    pcb->contexto_ejecucion.motivo_bloqueo = *(blocked_reason*) list_get(contexto, 13);
    pcb->contexto_ejecucion.motivoFinalizacion = *(motivo_finalizacion*) list_get(contexto, 14);
    cargar_io_detail_en_context(pcb, contexto, 14);
    log_info(logs_auxiliares, "AX: %d, BX: %d, CX: %d, DX: %d, EAX:%d, EBX:%d", pcb->contexto_ejecucion.registros_cpu.ax, pcb->contexto_ejecucion.registros_cpu.bx, pcb->contexto_ejecucion.registros_cpu.cx, pcb->contexto_ejecucion.registros_cpu.dx, pcb->contexto_ejecucion.registros_cpu.eax, pcb->contexto_ejecucion.registros_cpu.ebx);
}

t_pcb* quitarPcbCola(t_queue* cola, pthread_mutex_t semaforo) {
    pthread_mutex_lock(&semaforo);
    t_pcb* pcb = queue_pop(cola);
    pthread_mutex_unlock(&semaforo);
    return pcb;
}

void agregarPcbCola(t_queue* cola, pthread_mutex_t semaforo, t_pcb* pcb) {
    pthread_mutex_lock(&semaforo);
    queue_push(cola, pcb);
    pthread_mutex_unlock(&semaforo);
}

void comprobarContextoNuevo(t_pcb* pcb) {
    quitarPcbCola(cola_exec, sem_cola_exec);
    sem_post(&semContadorColaExec);
    switch (pcb->contexto_ejecucion.motivo_bloqueo) { 
    case INTERRUPCION_RELOJ:
        log_info(logs_obligatorios, "PID: %d - Desalojado por fin de Quantum", pcb->contexto_ejecucion.pid);
        agregarPcbCola(cola_ready, sem_cola_ready, pcb);
        cambiarEstado(READY, pcb);
        char* pids = obtenerPids(cola_ready, sem_cola_ready);
        log_info(logs_obligatorios, "Cola Ready: [%s]", pids);
        free(pids);
        sem_post(&semContadorColaReady);
        break;
    case LLAMADA_SISTEMA:
        agregarPcbCola(cola_blocked_aux, sem_cola_blocked_aux, pcb);
        cambiarEstado(BLOCKED, pcb);        
        sem_post(&semContadorColaBlocked);
        break;
    case INTERRUPCION_FIN_EVENTO:
        agregarPcbCola(cola_exit, sem_cola_exit, pcb);
        cambiarEstado(EXIT, pcb);
        sem_post(&semContadorColaExit);
        break;
    default:
        log_error(logs_error, "Error con estado recibido, no reconocido: %d", pcb->contexto_ejecucion.state);
        break;
    }
}

void empaquetar_registros_cpu(t_paquete* paquete, t_pcb* pcb) {
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.pc), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.ax), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.bx), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.cx), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.dx), sizeof(uint8_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.eax), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.ebx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.ecx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.edx), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.si), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registros_cpu.di), sizeof(uint32_t));
}

void empaquetar_contexto_ejecucion(t_paquete* paquete, t_pcb* pcb) {
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.pid), sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.registro_estados), sizeof(uint64_t));
    empaquetar_registros_cpu(paquete, pcb);
    blocked_reason motivo_vacio = NOTHING;
    agregar_a_paquete(paquete, &motivo_vacio, sizeof(int));
}

void mensaje_cpu_interrupt() {
    t_paquete* paquete = crear_paquete(INTERRUPCION);
    agregar_a_paquete(paquete, &(pcbADesalojar->contexto_ejecucion.pid), sizeof(uint32_t));
    enviar_paquete(paquete, fd_cpu_interrupt);
    eliminar_paquete(paquete);
}

void bloquearPCBPorRecurso(recursoSistema* recurso, t_pcb* pcb) {
    pthread_mutex_lock(&(recurso->mutexCola));
    queue_push(recurso->cola, &(pcb->contexto_ejecucion.pid));
    pthread_mutex_unlock(&(recurso->mutexCola));
    sem_post(&(recurso->semCola));
    pthread_mutex_lock(&sem_cola_blocked);
    list_add(cola_blocked, pcb);
    pthread_mutex_unlock(&sem_cola_blocked);
    log_info(logs_obligatorios, "PID: %d - Bloqueado por: %s", pcb->contexto_ejecucion.pid, recurso->nombre);
}

void corto_plazo_exec() {
    int tiempoMicroSegundos = pcbADesalojar->quantum_faltante * 1000;
    log_info(logs_auxiliares, "Temporizador inicializado en %ld", pcbADesalojar->quantum_faltante);
    usleep(tiempoMicroSegundos);
    mensaje_cpu_interrupt();
}

void mensaje_cpu_dispatch(op_codigo codigoOperacion, t_pcb* pcb) {
    t_paquete* paquete;
    uint64_t tiempoTranscurridoMilisegundos;
    t_temporal* tiempoQuantum;
    pthread_t cortoPlazoExec;
    pcbADesalojar = pcb;
    switch (codigoOperacion) {
    case CONTEXTO_EJECUCION:
        paquete = crear_paquete(CONTEXTO_EJECUCION);
        empaquetar_contexto_ejecucion(paquete, pcb);
        enviar_paquete(paquete, fd_cpu_dispatch);
        eliminar_paquete(paquete);
        if ( ALGORITMO_PLANIFICACION != FIFO ) {
            tiempoQuantum = temporal_create();
            crearHiloDetach(&cortoPlazoExec, (void*) corto_plazo_exec, NULL, "Planificacion corto plazo EXEC", logs_auxiliares, logs_error);
        }
        op_codigo codigoOperacion = recibir_operacion(fd_cpu_dispatch);
        if ( ALGORITMO_PLANIFICACION != FIFO ) {
            pthread_cancel(cortoPlazoExec);
            temporal_stop(tiempoQuantum);
            tiempoTranscurridoMilisegundos = temporal_gettime(tiempoQuantum);
            log_info(logs_auxiliares, "Tiempo a ejecutar: %ld - Tiempo ejecutado: %ld", pcb->quantum_faltante, tiempoTranscurridoMilisegundos);
            temporal_destroy(tiempoQuantum);
        }
        if ( codigoOperacion == OK_OPERACION ) {
            t_list* contextoNuevo = recibir_paquete(fd_cpu_dispatch);
            pthread_mutex_lock(&sem_cola_exec);
            if ( !queue_is_empty(cola_exec) ) {
                pthread_mutex_unlock(&sem_cola_exec);
                if ( ALGORITMO_PLANIFICACION == VRR ) {
                    if ( tiempoTranscurridoMilisegundos < pcb->quantum_faltante ) {
                        pcb->quantum_faltante -= tiempoTranscurridoMilisegundos;
                    } else {
                        pcb->quantum_faltante = QUANTUM;
                    }
                    log_info(logs_auxiliares, "PID: %d - Tiempo sobrante asignado: %ld", pcb->contexto_ejecucion.pid, pcb->quantum_faltante);
                }
                cargar_contexto_recibido(contextoNuevo, pcb);
                list_destroy(contextoNuevo);
                if ( pcb->contexto_ejecucion.io_detail.io_instruccion == SIGNAL ) {
                    recursoSistema* recursoEncontrado = NULL;
                    recursoBuscado = pcb->contexto_ejecucion.io_detail.nombre_io;
                    recursoEncontrado = list_find(listaRecursosSistema, buscarRecurso);
                    if ( recursoEncontrado != NULL ) {
                        log_info(logs_auxiliares, "Recurso encontrado %s", recursoEncontrado->nombre);
                        pthread_mutex_lock(&(recursoEncontrado->mutexCantidadInstancias));
                        sem_post(&(recursoEncontrado->semCantidadInstancias));
                        recursoEncontrado->cantidadInstancias++;
                        pthread_mutex_unlock(&(recursoEncontrado->mutexCantidadInstancias));
                        list_remove_element(pcb->recursosAsignados, recursoEncontrado->nombre);
                        log_info(logs_auxiliares, "Recurso %s devuelto por el PID %d", recursoEncontrado->nombre, pcb->contexto_ejecucion.pid);
                        pcb->contexto_ejecucion.io_detail.io_instruccion = NONE;
                        pcb->contexto_ejecucion.io_detail.nombre_io = "";
                        mensaje_cpu_dispatch(CONTEXTO_EJECUCION, pcb);
                    } else {
                        log_warning(logs_auxiliares, "Recurso %s no encontrado", recursoBuscado);
                        pcb->contexto_ejecucion.motivoFinalizacion = INVALID_RESOURCE;
                        agregarPcbCola(cola_exit, sem_cola_exit, pcb);
                        cambiarEstado(EXIT, pcb);
                        sem_post(&semContadorColaExit);
                    }
                    break;
                } else if ( pcb->contexto_ejecucion.io_detail.io_instruccion == WAIT ) {
                    recursoSistema* recursoEncontrado = NULL;
                    recursoBuscado = pcb->contexto_ejecucion.io_detail.nombre_io;
                    recursoEncontrado = list_find(listaRecursosSistema, buscarRecurso);
                    if ( recursoEncontrado != NULL ) {
                        log_info(logs_auxiliares, "Recurso encontrado %s", recursoEncontrado->nombre);
                        pthread_mutex_lock(&(recursoEncontrado->mutexCantidadInstancias));
                        if ( recursoEncontrado->cantidadInstancias > 0 ) {
                            sem_wait(&(recursoEncontrado->semCantidadInstancias));
                            recursoEncontrado->cantidadInstancias--;
                            list_add(pcb->recursosAsignados, recursoEncontrado->nombre);
                            pthread_mutex_unlock(&(recursoEncontrado->mutexCantidadInstancias));
                            log_info(logs_auxiliares, "Recurso %s asignado a PID %d", recursoEncontrado->nombre, pcb->contexto_ejecucion.pid);
                            pcb->contexto_ejecucion.io_detail.io_instruccion = NONE;
                            pcb->contexto_ejecucion.io_detail.nombre_io = "";
                            mensaje_cpu_dispatch(CONTEXTO_EJECUCION, pcb);
                            break;
                        }
                        pthread_mutex_unlock(&(recursoEncontrado->mutexCantidadInstancias));
                        cambiarEstado(BLOCKED, pcb);
                        bloquearPCBPorRecurso(recursoEncontrado, pcb);
                    } else {
                        log_warning(logs_auxiliares, "Recurso %s no encontrado", recursoBuscado);
                        pcb->contexto_ejecucion.motivoFinalizacion = INVALID_RESOURCE;
                        agregarPcbCola(cola_exit, sem_cola_exit, pcb);
                        cambiarEstado(EXIT, pcb);
                        sem_post(&semContadorColaExit);
                    }
                    quitarPcbCola(cola_exec, sem_cola_exec);
                    sem_post(&semContadorColaExec);
                    break;
                }
                comprobarContextoNuevo(pcb);
            } else {
                pthread_mutex_unlock(&sem_cola_exec);
                sem_post(&semContadorColaExec);
                list_destroy(contextoNuevo);
                enviarPCBExit(pcb);
            }
        } else {
            log_error(logs_error, "Problema con la operacion");
        }
        break;
    default:
        break;
    }
}

char* enumEstadoAString(process_state estado) {
    switch (estado) {
    case NEW:
        return "NEW";
    case READY:
        return "READY";
    case EXEC:
        return "EXEC";
    case BLOCKED:
        return "BLOCKED";
    case EXIT:
        return "EXIT";
    default:
        return "ERROR ESTADO";
    }
}

void cambiarEstado(process_state estadoNuevo, t_pcb* pcb) {
    process_state estadoViejo = pcb->contexto_ejecucion.state;
    log_info(logs_obligatorios, "PID: %d - Estado Anterior: %s - Estado Actual: %s",
            pcb->contexto_ejecucion.pid,
            enumEstadoAString(estadoViejo),
            enumEstadoAString(estadoNuevo)
            );
    switch (estadoNuevo) {
    case READY:
        pcb->contexto_ejecucion.state = READY;
        break;
    case EXEC:
        pcb->contexto_ejecucion.state = EXEC;
        break;
    case BLOCKED:
        pcb->contexto_ejecucion.state = BLOCKED;
        break;
    case EXIT:
        pcb->contexto_ejecucion.state = EXIT;
        break;
    default:
        break;
    }
}

void corto_plazo_ready() {
    t_pcb* pcb;
    while(1) {
        pthread_mutex_lock(&sem_planificacion);
        while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
            pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
        }
        pthread_mutex_unlock(&sem_planificacion);
        sem_wait(&semContadorColaReady);
        sem_wait(&semContadorColaExec);
        pthread_mutex_lock(&sem_cola_ready_aux);
        if ( ALGORITMO_PLANIFICACION == VRR && !queue_is_empty(cola_ready_aux) ) {
            pthread_mutex_unlock(&sem_cola_ready_aux);
            pcb = quitarPcbCola(cola_ready_aux, sem_cola_ready_aux);
            agregarPcbCola(cola_exec, sem_cola_exec, pcb);
            cambiarEstado(EXEC, pcb);
            mensaje_cpu_dispatch(CONTEXTO_EJECUCION, pcb);
            continue;
        }
        pthread_mutex_unlock(&sem_cola_ready_aux);
        pcb = quitarPcbCola(cola_ready, sem_cola_ready);
        agregarPcbCola(cola_exec, sem_cola_exec, pcb);
        cambiarEstado(EXEC, pcb);
        mensaje_cpu_dispatch(CONTEXTO_EJECUCION, pcb);
    }
}

void planificacionLargoPlazo() {
    pthread_t LargoPlazoNew;
    pthread_t LargoPlazoExit;
    crearHiloDetach(&LargoPlazoNew, (void*) largo_plazo_new, NULL, "Planificacion largo plazo NEW", logs_auxiliares, logs_error);
    crearHiloDetach(&LargoPlazoExit, (void*) largo_plazo_exit, NULL, "Planificacion largo plazo EXIT", logs_auxiliares, logs_error);
}

int elementosEnCola(t_queue* cola, pthread_mutex_t semaforo) {
    int cantidadProgramasEnCola;
    pthread_mutex_lock(&semaforo);
    cantidadProgramasEnCola = queue_size(cola);
    pthread_mutex_unlock(&semaforo);
    return cantidadProgramasEnCola;
}

void largo_plazo_new() {
    while(1) {
        pthread_mutex_lock(&sem_planificacion);
        while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
            pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
        }
        pthread_mutex_unlock(&sem_planificacion);
        sem_wait(&semContadorColaNew);
        pthread_mutex_lock(&mx_cantidad_ejecutandose);
        pthread_mutex_lock(&sem_grado_multiprogramacion);
        if ( cantidad_elementos_ejecutandose < GRADO_MULTIPROGRAMACION ) {
            cantidad_elementos_ejecutandose++;
            pthread_mutex_unlock(&mx_cantidad_ejecutandose);
            pthread_mutex_unlock(&sem_grado_multiprogramacion);
            t_pcb* pcb = quitarPcbCola(cola_new, sem_cola_new);
            mensaje_memoria(CREAR_PCB, pcb);
            agregarPcbCola(cola_ready, sem_cola_ready, pcb);
            char* pids = obtenerPids(cola_ready, sem_cola_ready);
            log_info(logs_obligatorios, "Cola Ready: [%s]", pids);
            free(pids);
            sem_post(&semContadorColaReady);
            cambiarEstado(READY, pcb);
            continue;
        }
        pthread_mutex_unlock(&mx_cantidad_ejecutandose);
        pthread_mutex_unlock(&sem_grado_multiprogramacion);
        sem_post(&semContadorColaNew);
    }
}

char* obtenerMotivo(motivo_finalizacion motivo) {
    switch (motivo)
    {
    case SUCCESS:
        return "SUCCESS"; 
    case INVALID_RESOURCE:
        return "INVALID_RESOURCE";
    case INVALID_INTERFACE:
        return "INVALID_INTERFACE";
    case OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case INTERRUPTED_BY_USER:
        return "INTERRUPTED_BY_USER"; 
    default:
        return "ERROR";
    }
}

void largo_plazo_exit() {
    while(1) {
        pthread_mutex_lock(&sem_planificacion);
        while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
            pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
        }
        pthread_mutex_unlock(&sem_planificacion);
        sem_wait(&semContadorColaExit);
        t_pcb* pcb = quitarPcbCola(cola_exit, sem_cola_exit);
        log_info(logs_obligatorios, "Finaliza el proceso %d - Motivo: %s", pcb->contexto_ejecucion.pid, obtenerMotivo(pcb->contexto_ejecucion.motivoFinalizacion));
        eliminar_pcb(pcb);
        pthread_mutex_lock(&mx_cantidad_ejecutandose);
        cantidad_elementos_ejecutandose--;
        pthread_mutex_unlock(&mx_cantidad_ejecutandose);
    }
}

void eliminarLista(void* parametroVoid) {
    t_params_io parametro = *(t_params_io*) parametroVoid;
    free(parametro.valor);
}

void eliminar_io_detail(t_pcb* pcb) {
    
    t_io_detail io_detail_de_contexto = pcb->contexto_ejecucion.io_detail;

    if (io_detail_de_contexto.parametros == NULL || io_detail_de_contexto.parametros->elements_count == 0) {
        list_destroy(io_detail_de_contexto.parametros);
        return;
    }
    list_destroy_and_destroy_elements(io_detail_de_contexto.parametros, (void*) eliminarLista);
}

void liberarRecursosPcb(t_pcb* pcb) {
    char* nombreRecurso;
    recursoSistema* recursoEncontrado;
    log_info(logs_auxiliares, "listsize: %d", list_size(pcb->recursosAsignados));
    for ( int i = 0; i < list_size(pcb->recursosAsignados); i++) {
        nombreRecurso = (char*) list_remove(pcb->recursosAsignados, 0);
        for( int j = 0; j < list_size(listaRecursosSistema); j++) {
            recursoEncontrado = (recursoSistema*) list_get(listaRecursosSistema, j);
            if ( recursoEncontrado->nombre == nombreRecurso ) {
                pthread_mutex_lock(&recursoEncontrado->mutexCantidadInstancias);
                recursoEncontrado->cantidadInstancias++;
                pthread_mutex_unlock(&recursoEncontrado->mutexCantidadInstancias);
                sem_post(&recursoEncontrado->semCantidadInstancias);
            }
        }
    }
    list_destroy(pcb->recursosAsignados);
}

void eliminar_pcb(t_pcb* pcb) {
    mensaje_memoria(ELIMINAR_PCB, pcb);
    eliminar_io_detail(pcb);
    liberarRecursosPcb(pcb);
    free(pcb);
}

void crear_pcb(char* pathArchivo) {
    t_pcb* pcb = malloc(sizeof(t_pcb));
    pcb->contexto_ejecucion.pid = pid_siguiente;
    pid_siguiente++;
    pcb->quantum_faltante = QUANTUM;
    pcb->contexto_ejecucion.motivo_bloqueo = NOTHING;
    pcb->path_archivo = pathArchivo;
    // SACAR
    pcb->contexto_ejecucion.registro_estados = 0;
    iniciarRegistrosCPU(pcb);
    pcb->contexto_ejecucion.state = NEW;
    pcb->contexto_ejecucion.io_detail.nombre_io = "";
    pcb->contexto_ejecucion.io_detail.parametros = list_create();
    pcb->contexto_ejecucion.io_detail.io_instruccion = NONE;
    pcb->recursosAsignados = list_create();
    pcb->contexto_ejecucion.motivoFinalizacion = NONEXISTENT;
    agregarPcbCola(cola_new, sem_cola_new, pcb);
    sem_post(&semContadorColaNew);
    log_info(logs_obligatorios, "Se crea el proceso %d en NEW", pcb->contexto_ejecucion.pid);
}

void iniciarRegistrosCPU(t_pcb* pcb) {
    pcb->contexto_ejecucion.registros_cpu.pc = 0;
    pcb->contexto_ejecucion.registros_cpu.ax = 0;
    pcb->contexto_ejecucion.registros_cpu.bx = 0;
    pcb->contexto_ejecucion.registros_cpu.cx = 0;
    pcb->contexto_ejecucion.registros_cpu.dx = 0;
    pcb->contexto_ejecucion.registros_cpu.eax = 0;
    pcb->contexto_ejecucion.registros_cpu.ebx = 0;
    pcb->contexto_ejecucion.registros_cpu.ecx = 0;
    pcb->contexto_ejecucion.registros_cpu.edx = 0;
    pcb->contexto_ejecucion.registros_cpu.si = 0;
    pcb->contexto_ejecucion.registros_cpu.di = 0;
}

bool evaluar_respuesta_de_operacion(int fd_cliente, char* nombre_modulo_server, op_codigo codigo_operacion) {
    op_codigo respuesta_recibida = recibir_operacion(fd_cliente);
    switch (respuesta_recibida) {
        case OK_OPERACION:
            log_info(logs_auxiliares, "Operacion OK desde %s", nombre_modulo_server);
            return true;
        default:
            log_error(logs_error, "Error al ejecutar operacion %s en %d", nombre_modulo_server, codigo_operacion);
            return false;
    }
}

void mensaje_memoria(op_codigo comandoMemoria, t_pcb* pcb) {
    t_paquete* paquete;
    switch (comandoMemoria) {
    case CREAR_PCB:
        paquete = crear_paquete(CREAR_PCB);
        agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.pid), sizeof(uint32_t));
        agregar_a_paquete(paquete, pcb->path_archivo, strlen(pcb->path_archivo) + 1);
        enviar_paquete(paquete, fd_memoria);
        if ( !evaluar_respuesta_de_operacion(fd_memoria, MEMORIA_SERVER, CREAR_PCB) ) {
            log_warning(logs_auxiliares, "Reintentando creacion pid: %d", pcb->contexto_ejecucion.pid);
            mensaje_memoria(comandoMemoria, pcb);
        }
        eliminar_paquete(paquete);
        break;
    case ELIMINAR_PCB:
        paquete = crear_paquete(ELIMINAR_PCB);
        agregar_a_paquete(paquete, &(pcb->contexto_ejecucion.pid), sizeof(uint32_t));
        enviar_paquete(paquete, fd_memoria);
        if ( !evaluar_respuesta_de_operacion(fd_memoria, MEMORIA_SERVER, ELIMINAR_PCB) ) {
            log_warning(logs_auxiliares, "Reintentando eliminacion pid: %d", pcb->contexto_ejecucion.pid);
            mensaje_memoria(comandoMemoria, pcb);
        }
        eliminar_paquete(paquete);
        break;
    default:
        log_error(logs_error, "Operacion desconocida");
        break;
    }
}

void inicializarColas() {
    cola_new = queue_create();
    cola_ready = queue_create();
    cola_blocked_aux = queue_create();
    cola_blocked = list_create();
    cola_exec = queue_create();
    cola_exit = queue_create();
    cola_ready_aux = queue_create();
}

void inicializarSemaforos() {
    pthread_mutex_init(&mutexListaPidsAFinalizar, NULL);
    pthread_mutex_init(&mutexEliminarProceso, NULL);
    pthread_mutex_init(&mutexpidAEnviarExit, NULL);
    pthread_mutex_init(&mutexInterfazAEliminar, NULL);
    pthread_mutex_init(&mx_cantidad_ejecutandose, NULL);
    pthread_mutex_init(&sem_grado_multiprogramacion, NULL);
    pthread_mutex_init(&sem_planificacion, NULL);
    pthread_cond_init(&condicion_planificacion, NULL);
    pthread_mutex_init(&sem_cola_ready, NULL);
    pthread_mutex_init(&sem_cola_ready_aux, NULL);
    pthread_mutex_init(&sem_cola_new, NULL);
    pthread_mutex_init(&sem_cola_blocked_aux, NULL);
    pthread_mutex_init(&sem_cola_blocked, NULL);
    pthread_mutex_init(&sem_cola_exec, NULL);
    pthread_mutex_init(&sem_cola_exit, NULL);
    sem_init(&semContadorColaNew, 0, 0);
    sem_init(&semContadorColaReady, 0, 0);
    sem_init(&semContadorColaReadyAux, 0, 0);
    sem_init(&semContadorColaExec, 0, 1);
    sem_init(&semContadorColaBlocked, 0, 0);
    sem_init(&semContadorColaExit, 0, 0);
}

void inicializarListasInterfaces() {
    interfacesGenericas = list_create();
    interfacesSTDIN = list_create();
    interfacesSTDOUT = list_create();
    interfacesFS = list_create();
    pthread_mutex_init(&mutexInterfacesGenericas, NULL);
    pthread_mutex_init(&mutexInterfacesSTDIN, NULL);
    pthread_mutex_init(&mutexInterfacesSTDOUT, NULL);
    pthread_mutex_init(&mutexInterfacesFS, NULL);
}

void atender_recurso(recursoSistema* dataRecurso) {
    uint32_t* pidDesbloqueado;
    t_pcb* pcbDesbloqueado;
    bool tomarOtroPcb;
    while(1) {
        do {
            pthread_mutex_lock(&sem_planificacion);
            while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
                pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
            }
            pthread_mutex_unlock(&sem_planificacion);
            
            sem_wait(&dataRecurso->semCola);
            sem_wait(&dataRecurso->semCantidadInstancias);
            
            pthread_mutex_lock(&dataRecurso->mutexCola);
            pidDesbloqueado = queue_pop(dataRecurso->cola);
            pthread_mutex_unlock(&dataRecurso->mutexCola);

            tomarOtroPcb = false;
            pcbDesbloqueado = comprobarSiSeDebeEliminar(pidDesbloqueado);
            if ( pcbDesbloqueado == NULL ) {
                sem_post(&dataRecurso->semCantidadInstancias);
                tomarOtroPcb = true;
            }
        } while( tomarOtroPcb );

        pthread_mutex_lock(&dataRecurso->mutexCantidadInstancias);
        dataRecurso->cantidadInstancias--;
        pthread_mutex_unlock(&dataRecurso->mutexCantidadInstancias);
        log_info(logs_auxiliares, "Recurso %s asignado a PID %d", dataRecurso->nombre, *pidDesbloqueado);

        list_add(pcbDesbloqueado->recursosAsignados, dataRecurso->nombre);
        pcbDesbloqueado->contexto_ejecucion.motivo_bloqueo = NOTHING;
        pcbDesbloqueado->contexto_ejecucion.io_detail.io_instruccion = NONE;
        pcbDesbloqueado->contexto_ejecucion.io_detail.nombre_io = "";
        pthread_mutex_lock(&sem_cola_blocked);
        list_remove_element(cola_blocked, pcbDesbloqueado);       
        pthread_mutex_unlock(&sem_cola_blocked);
        if ( ALGORITMO_PLANIFICACION == VRR && pcbDesbloqueado->quantum_faltante != QUANTUM ) {
            agregarPcbCola(cola_ready_aux, sem_cola_ready_aux, pcbDesbloqueado);
            char* pids = obtenerPids(cola_ready_aux, sem_cola_ready_aux);
            log_info(logs_obligatorios, "Cola Ready Prioridad: [%s]", pids);
            free(pids);
        } else {
            agregarPcbCola(cola_ready, sem_cola_ready, pcbDesbloqueado);
            char* pids = obtenerPids(cola_ready, sem_cola_ready);
            log_info(logs_obligatorios, "Cola Ready: [%s]", pids);
            free(pids);
        }
        cambiarEstado(READY, pcbDesbloqueado);
        sem_post(&semContadorColaReady);
    }
}

void inicializarRecursos() {
    listaRecursosSistema = list_create();
    for(int i = 0; RECURSOS[i] != NULL; i++) {
        recursoSistema* nuevoRecurso = malloc(sizeof(recursoSistema));
        nuevoRecurso -> nombre = RECURSOS[i];
        nuevoRecurso -> cola = queue_create();
        sem_t semaforoCola;
        sem_init(&semaforoCola, 0, 0);
        nuevoRecurso -> semCola = semaforoCola;
        sem_t semaforoCantidadRecurso;
        sem_init(&semaforoCantidadRecurso, 0, INSTANCIAS_RECURSOS[i]);
        nuevoRecurso -> semCantidadInstancias = semaforoCantidadRecurso;
        nuevoRecurso -> cantidadInstancias = INSTANCIAS_RECURSOS[i];
        pthread_mutex_t mutexCantidadInstancias;
        pthread_mutex_init(&mutexCantidadInstancias, NULL);
        nuevoRecurso -> mutexCantidadInstancias = mutexCantidadInstancias;
        pthread_mutex_t mutexCola;
        pthread_mutex_init(&mutexCola, NULL);
        nuevoRecurso -> mutexCola = mutexCola;
        list_add(listaRecursosSistema, nuevoRecurso);
        pthread_t thread_recurso;
        crearHiloDetach(&thread_recurso, (void*) atender_recurso, (recursoSistema*) nuevoRecurso, nuevoRecurso->nombre, logs_auxiliares, logs_error);
    }
}

void inicializarVariables() {
    // Creacion de logs
    crearLogs();

    // Leer y almacenar los datos de la configuracion
    iniciarConfig();

    // Inicializar semaforos
    inicializarSemaforos();

    // Inicializar estructura de recursos
    inicializarRecursos();
    
    // Inicializacion servidor
    socket_servidor = iniciar_servidor(PUERTO_ESCUCHA, logs_auxiliares, logs_error);

    // Crear las conexiones hacia cpu y memoria
    if ( crearConexiones() ) {
        log_info(logs_auxiliares, "Conexiones creadas correctamente");
    }
    // Inicializar colas
    inicializarColas();

    // Inicializar listas de interfaces
    inicializarListasInterfaces();

}

void iniciarConsolaInteractiva() {
    crearHiloDetach(&thread_consola_interactiva, (void*) atender_consola_interactiva, NULL, "Consola interactiva", logs_auxiliares, logs_error);
}

void atender_consola_interactiva() {
    char** arrayComando;
    char* leido;
    while(1) {
        leido = readline("COMANDO > ");
        add_history(leido);
        arrayComando = string_split(leido, " ");
        ejecutar_comando_consola(arrayComando);
    }
    free(arrayComando);
    free(leido);
}

char* obtenerPids (t_queue* cola, pthread_mutex_t semaforo) {
    pthread_mutex_lock(&semaforo);
    char* pids = string_new();
    for( int i = 0; i < list_size(cola->elements); i++ ) {
        t_pcb* pcb = list_get(cola->elements, i);
        if ( i < list_size(cola->elements) -1 ) {
            string_append_with_format(&pids, "%d, ", pcb->contexto_ejecucion.pid);
        } else {
            string_append_with_format(&pids, "%d", pcb->contexto_ejecucion.pid);
        }
    }
    pthread_mutex_unlock(&semaforo);
    return pids;
}

void ejecutar_script(char* pathScript) {
    FILE* archivoScript = fopen(pathScript, "r");
    char* lineaLeida = NULL;
    size_t longitud = 0;
    ssize_t cantLeida;
    if ( archivoScript == NULL ) {
        log_error(logs_error, "Error al abrir el archivo %s", pathScript);
        return;
    }
    while ((cantLeida = getline(&lineaLeida, &longitud, archivoScript)) != -1) {
        lineaLeida[strcspn(lineaLeida, "\n")] = 0;
        char** arrayComando = string_split(lineaLeida, " ");
        log_info(logs_auxiliares, "Comando: %s", arrayComando[0]);
        log_info(logs_auxiliares, "Path: %s", arrayComando[1]);
        ejecutar_comando_consola(arrayComando);
        free(arrayComando);
    }
    fclose(archivoScript);
    free(lineaLeida);
}

char* obtenerPidsBloqueados() {
    char* pids = string_new();
    pthread_mutex_lock(&sem_cola_blocked);
    pthread_mutex_lock(&sem_cola_blocked_aux);
    // Reviso cola bloqueados (lista)
    int i;
    int j;
    t_pcb* pcb;
    for( i = 0; i < list_size(cola_blocked); i++ ) {
        pcb = (t_pcb*) list_get(cola_blocked, i);
        if ( i == list_size(cola_blocked) - 1 && queue_size(cola_blocked_aux) == 0 ) {
            string_append_with_format(&pids, "%d", pcb->contexto_ejecucion.pid);
            log_info(logs_auxiliares, "Pid %d en lista ultimo", pcb->contexto_ejecucion.pid);
        } else {
            string_append_with_format(&pids, "%d, ", pcb->contexto_ejecucion.pid);
            log_info(logs_auxiliares, "Pid %d en lista", pcb->contexto_ejecucion.pid);
        }
    }
    // Reviso cola auxiliar
    for( j = i; j < list_size(cola_blocked_aux->elements); j++ ) {
        pcb = (t_pcb*) list_get(cola_blocked_aux->elements, j);
        if ( j < list_size(cola_blocked_aux->elements) -1 ) {
            string_append_with_format(&pids, "%d, ", pcb->contexto_ejecucion.pid);
            log_info(logs_auxiliares, "Pid %d en cola", pcb->contexto_ejecucion.pid);
        } else {
            string_append_with_format(&pids, "%d", pcb->contexto_ejecucion.pid);
            log_info(logs_auxiliares, "Pid %d en cola ultimo", pcb->contexto_ejecucion.pid);
        }
    }
    pthread_mutex_unlock(&sem_cola_blocked);
    pthread_mutex_unlock(&sem_cola_blocked_aux);
    return pids;
}

bool coincidePidAEliminar(void* pcbVoid) {
    t_pcb* pcbAComparar = (t_pcb*) pcbVoid;
    return pcbAComparar->contexto_ejecucion.pid == pidAEliminar;
}

t_pcb* buscarPidEnCola(t_queue* cola, pthread_mutex_t semaforo) {
    t_pcb* pcbEncontrado;
    pthread_mutex_lock(&semaforo);
    pcbEncontrado = list_remove_by_condition(cola->elements, coincidePidAEliminar);
    pthread_mutex_unlock(&semaforo);
    return pcbEncontrado;
}

void eliminarPid() {
    t_pcb* pcbAEliminar;
    if ( (pcbAEliminar = buscarPidEnCola(cola_new, sem_cola_new)) != NULL ) {
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        enviarPCBExit(pcbAEliminar);
        sem_wait(&semContadorColaNew);
    } else if ( (pcbAEliminar = buscarPidEnCola(cola_ready, sem_cola_ready)) != NULL ) {
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        enviarPCBExit(pcbAEliminar);
        sem_wait(&semContadorColaReady);
    } else if ( (pcbAEliminar = buscarPidEnCola(cola_ready_aux, sem_cola_ready_aux)) != NULL ) {
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        enviarPCBExit(pcbAEliminar);
        sem_wait(&semContadorColaReady);
    } else if ( (pcbAEliminar = buscarPidEnCola(cola_exec, sem_cola_exec)) != NULL ) {
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        mensaje_cpu_interrupt();
    } else if ( (pcbAEliminar = buscarPidEnCola(cola_blocked_aux, sem_cola_blocked_aux)) != NULL ){
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        enviarPCBExit(pcbAEliminar);
        sem_wait(&semContadorColaBlocked);
    } else {
        pthread_mutex_lock(&sem_cola_blocked);
        pcbAEliminar = list_remove_by_condition(cola_blocked, coincidePidAEliminar);
        pthread_mutex_unlock(&sem_cola_blocked);
        pcbAEliminar->contexto_ejecucion.motivoFinalizacion = INTERRUPTED_BY_USER;
        enviarPCBExit(pcbAEliminar);
    }
}
 
void ejecutar_comando_consola(char** arrayComando) {
    comando = transformarAOperacion(arrayComando[0]);
    switch (comando) {
    case EJECUTAR_SCRIPT:
        ejecutar_script(arrayComando[1]);
        log_info(logs_auxiliares, "Script de ' %s ' ejecutado", arrayComando[1]);
        break;
    case INICIAR_PROCESO:
        crear_pcb(arrayComando[1]);
        break;
    case FINALIZAR_PROCESO:
        pidAEliminar = atoi(arrayComando[1]);
        pthread_mutex_lock(&sem_planificacion);
        planificacionNoEjecutandosePorFinalizarProceso = true;
        pthread_mutex_unlock(&sem_planificacion);
        eliminarPid();
        log_info(logs_auxiliares, "Proceso %d finalizado", pidAEliminar);
        pthread_mutex_lock(&sem_planificacion);
        planificacionNoEjecutandosePorFinalizarProceso = false;
        pthread_mutex_unlock(&sem_planificacion);
        pthread_cond_broadcast(&condicion_planificacion);
        break;
    case DETENER_PLANIFICACION:
        pthread_mutex_lock(&sem_planificacion);
        planificacionEjecutandose = false;
        pthread_mutex_unlock(&sem_planificacion);
        log_info(logs_auxiliares, "Planificacion pausada");
        break;
    case INICIAR_PLANIFICACION:
        pthread_mutex_lock(&sem_planificacion);
        planificacionEjecutandose = true;
        pthread_mutex_unlock(&sem_planificacion);
        pthread_cond_broadcast(&condicion_planificacion);
        log_info(logs_auxiliares, "Planificacion ejecutandose");
        break;
    case MULTIPROGRAMACION:
        pthread_mutex_lock(&sem_grado_multiprogramacion);
        GRADO_MULTIPROGRAMACION = atoi(arrayComando[1]);
        pthread_mutex_unlock(&sem_grado_multiprogramacion);
        log_info(logs_auxiliares, "Grado de multiprogramacion cambiado a: %d", GRADO_MULTIPROGRAMACION);
        break;
    case PROCESO_ESTADO:
        char* pids_new = obtenerPids(cola_new, sem_cola_new);
        char* pids_ready = obtenerPids(cola_ready, sem_cola_ready);
        char* pids_exec = obtenerPids(cola_exec, sem_cola_exec);
        char* pids_blocked = obtenerPidsBloqueados();
        char* pids_exit = obtenerPids(cola_exit, sem_cola_exit);

        log_info(logs_obligatorios, "Cola NEW: [%s]", pids_new);
        log_info(logs_obligatorios, "Cola READY: [%s]", pids_ready);
        if ( ALGORITMO_PLANIFICACION == VRR ) {
            char* pids_ready_aux = obtenerPids(cola_ready_aux, sem_cola_ready_aux);
            log_info(logs_obligatorios, "Cola READY Prioridad: [%s]", pids_ready_aux);
            free(pids_ready_aux);
        }
        log_info(logs_obligatorios, "Cola EXEC: [%s]", pids_exec);
        log_info(logs_obligatorios, "Cola BLOCKED: [%s]", pids_blocked);
        log_info(logs_obligatorios, "Cola EXIT: [%s]", pids_exit);
        free(pids_new);
        free(pids_ready);
        free(pids_exec);
        free(pids_blocked);
        free(pids_exit);
        break;
    default:
        log_info(logs_auxiliares, "Comando desconocido: %d", comando);
        break;
    }
}

comando_consola transformarAOperacion(char* operacionLeida) {
    if ( string_equals_ignore_case(operacionLeida, "EJECUTAR_SCRIPT") ) { // strcmp devuelve 0 si son iguales
        return EJECUTAR_SCRIPT;
    } else if ( string_equals_ignore_case(operacionLeida, "INICIAR_PROCESO") ) {
        return INICIAR_PROCESO;
    } else if ( string_equals_ignore_case(operacionLeida, "FINALIZAR_PROCESO") ) {
        return FINALIZAR_PROCESO;
    } else if ( string_equals_ignore_case(operacionLeida, "DETENER_PLANIFICACION") ) {
        return DETENER_PLANIFICACION;
    } else if ( string_equals_ignore_case(operacionLeida, "INICIAR_PLANIFICACION") ) {
        return INICIAR_PLANIFICACION;
    } else if ( string_equals_ignore_case(operacionLeida, "MULTIPROGRAMACION") ) {
        return MULTIPROGRAMACION;
    } else if ( string_equals_ignore_case(operacionLeida, "PROCESO_ESTADO") ) {
        return PROCESO_ESTADO;
    } else {
        return -1; // Valor por defecto para indicar error
    }
}

char* obtenerTipoInterfaz(typeInterface tipoInterfaz) {
    switch (tipoInterfaz) {
    case GENERICA:
        return "GENERICA";
    case STDIN:
        return "STDIN";
    case STDOUT:
        return "STDOUT";
    case FS:
        return "FS";
    default:
        log_error(logs_error, "Tipo interfaz no reconocida: %d", tipoInterfaz);
        return "ERROR";
    }
}

void enviarIoDetail(t_pcb* pcbAEjecutar, int fd_interfaz) {
    t_paquete* paquete = crear_paquete(OK_OPERACION);
    agregar_a_paquete(paquete, &pcbAEjecutar->contexto_ejecucion.pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &(pcbAEjecutar->contexto_ejecucion.io_detail.parametros->elements_count), sizeof(int));
    
    log_info(logs_auxiliares, "Cantidad parametros: %d", pcbAEjecutar->contexto_ejecucion.io_detail.parametros->elements_count);
    if (pcbAEjecutar->contexto_ejecucion.io_detail.parametros == NULL || pcbAEjecutar->contexto_ejecucion.io_detail.parametros->elements_count == 0) {
        return;
    }
    
    for (int i = 0; i < pcbAEjecutar->contexto_ejecucion.io_detail.parametros->elements_count; i++) {
        t_params_io parametro_io = *(t_params_io*)list_get(pcbAEjecutar->contexto_ejecucion.io_detail.parametros, i);
        int size_parametro;
        void *valor_parametro_a_enviar;
        switch (parametro_io.tipo_de_dato) {
            case INT:
                size_parametro = sizeof(int);
                valor_parametro_a_enviar = malloc(size_parametro);
                valor_parametro_a_enviar = (int *)parametro_io.valor;
                log_info(logs_auxiliares, "Se envia el parametro %d", *(int*)valor_parametro_a_enviar);
                break;
            case UINT32:
                size_parametro = sizeof(uint32_t);
                valor_parametro_a_enviar = malloc(size_parametro);
                valor_parametro_a_enviar = (uint32_t *)parametro_io.valor;
                log_info(logs_auxiliares, "Se envia el parametro %d", *(uint32_t*)valor_parametro_a_enviar);
                break;
            case STRING:
                size_parametro = string_length(parametro_io.valor) + 1;
                valor_parametro_a_enviar = malloc(size_parametro);
                valor_parametro_a_enviar = (char *)parametro_io.valor;
                log_info(logs_auxiliares, "Se envia el parametro string %s", (char *) valor_parametro_a_enviar);
                break;
            default:
                log_error(logs_error, "Error tipo de dato enviado");
                break;
        }
        agregar_a_paquete(paquete, &parametro_io.tipo_de_dato, sizeof(int));
        agregar_a_paquete(paquete, valor_parametro_a_enviar, size_parametro);
    }
    agregar_a_paquete(paquete, &pcbAEjecutar->contexto_ejecucion.io_detail.io_instruccion, sizeof(int));
    enviar_paquete(paquete, fd_interfaz);
    eliminar_paquete(paquete);
}

bool coincideInterfazADesconectar(void* interfazVoid) {
    interfazConectada* interfazAComprobar = (interfazConectada*) interfazVoid;
    return strcmp(interfazAComprobar->nombre, interfazAEliminar) == 0;
}

bool desconectarInterfaz(t_list* listaInterfaz, pthread_mutex_t semaforo, interfazConectada* datoInterfaz) {
    pthread_mutex_lock(&semaforo);
    pthread_mutex_lock(&mutexInterfazAEliminar);
    interfazAEliminar = datoInterfaz->nombre;
    interfazConectada* interfazADesconectar = (interfazConectada*) list_remove_by_condition(listaInterfaz, coincideInterfazADesconectar);
    pthread_mutex_unlock(&mutexInterfazAEliminar);
    pthread_mutex_unlock(&semaforo);
    if ( interfazADesconectar == NULL ) {
        return false;
    }
    log_info(logs_auxiliares, "Consola encontrada: %s", interfazADesconectar->nombre);
    eliminarInterfaz(interfazADesconectar);
    return true;
}

t_pcb* comprobarSiSeDebeEliminar(uint32_t* pcbAComprobar) {
    pthread_mutex_lock(&sem_cola_blocked);
    t_pcb* procesoLeido = NULL;
    for(int i = 0; i < list_size(cola_blocked); i++) {
        procesoLeido = (t_pcb*) list_get(cola_blocked, i);
        if ( procesoLeido->contexto_ejecucion.pid == *pcbAComprobar ) {
            pthread_mutex_unlock(&sem_cola_blocked);
            return procesoLeido;
        }
    }
    pthread_mutex_unlock(&sem_cola_blocked);
    return procesoLeido;
}

void atender_cliente(interfazConectada* datosInterfaz) {
    int codigoOperacion;
    uint32_t* pidAEjecutar;
    t_pcb* pcbAEjecutar;
    bool tomarOtroPcb;
    while( datosInterfaz->fd_interfaz != -1 ) {
        do {
            pthread_mutex_lock(&sem_planificacion);
            while( !planificacionEjecutandose || planificacionNoEjecutandosePorFinalizarProceso ) {
                pthread_cond_wait(&condicion_planificacion, &sem_planificacion);
            }
            pthread_mutex_unlock(&sem_planificacion);
            sem_wait(&datosInterfaz->semaforoCantProcesos);
            sem_wait(&datosInterfaz->libre);
            tomarOtroPcb = false;
            pthread_mutex_lock(&datosInterfaz->semaforoMutex);
            pidAEjecutar = queue_peek(datosInterfaz->colaEjecucion);
            pthread_mutex_unlock(&datosInterfaz->semaforoMutex);
            pcbAEjecutar = comprobarSiSeDebeEliminar(pidAEjecutar);
            if ( pcbAEjecutar == NULL ) {
                pthread_mutex_lock(&datosInterfaz->semaforoMutex);
                pidAEjecutar = queue_pop(datosInterfaz->colaEjecucion);
                pthread_mutex_unlock(&datosInterfaz->semaforoMutex);
                sem_post(&datosInterfaz->libre);
                tomarOtroPcb = true;
            }
        } while( tomarOtroPcb );
        enviarIoDetail(pcbAEjecutar, datosInterfaz->fd_interfaz);
        codigoOperacion = recibir_operacion(datosInterfaz->fd_interfaz);
        if ( codigoOperacion == -1 ) {
            log_warning(logs_auxiliares, "El cliente se desconecto de Kernel");
            break;
        }
        switch (codigoOperacion) {
        case MENSAJE:
            char* mensaje = recibir_mensaje(datosInterfaz->fd_interfaz);
            log_info(logs_auxiliares, "Me llegó el mensaje %s", mensaje);
            free(mensaje);
            break;
        case PAQUETE:
            t_list* valoresPaquete = recibir_paquete(datosInterfaz->fd_interfaz);
            list_iterate(valoresPaquete, (void*) iteradorPaquete);
            list_destroy(valoresPaquete);
            break;
        case OK_OPERACION:
            log_info(logs_auxiliares, "proceso vuelta de IO %s: %d", datosInterfaz->nombre, pcbAEjecutar->contexto_ejecucion.pid);
            pthread_mutex_lock(&datosInterfaz->semaforoMutex);
            pidAEjecutar = queue_pop(datosInterfaz->colaEjecucion);
            pthread_mutex_unlock(&datosInterfaz->semaforoMutex);
            pcbAEjecutar = comprobarSiSeDebeEliminar(pidAEjecutar);
            if ( pcbAEjecutar == NULL ) {
                enviarPCBExit(pcbAEjecutar);
                sem_post(&datosInterfaz->libre);
            }
            else {
                pcbAEjecutar->contexto_ejecucion.motivo_bloqueo = NOTHING;
                list_clean(pcbAEjecutar->contexto_ejecucion.io_detail.parametros);
                pcbAEjecutar->contexto_ejecucion.io_detail.nombre_io = "";
                pcbAEjecutar->contexto_ejecucion.io_detail.io_instruccion = NONE;
                pthread_mutex_lock(&sem_cola_blocked);
                list_remove_element(cola_blocked, pcbAEjecutar);     
                pthread_mutex_unlock(&sem_cola_blocked);
                if ( ALGORITMO_PLANIFICACION == VRR && pcbAEjecutar->quantum_faltante != QUANTUM ) {
                    agregarPcbCola(cola_ready_aux, sem_cola_ready_aux, pcbAEjecutar);
                    char* pids = obtenerPids(cola_ready_aux, sem_cola_ready_aux);
                    log_info(logs_obligatorios, "Cola Ready Prioridad: [%s]", pids);
                    free(pids);
                } else {
                    agregarPcbCola(cola_ready, sem_cola_ready, pcbAEjecutar);
                    char* pids = obtenerPids(cola_ready, sem_cola_ready);
                    log_info(logs_obligatorios, "Cola Ready: [%s]", pids);
                    free(pids);
                }
                cambiarEstado(READY, pcbAEjecutar);
                sem_post(&semContadorColaReady);
                sem_post(&datosInterfaz->libre);
            }
            break;
        default:
            log_error(logs_error, "Codigo de operacion no reconocido: %d", codigoOperacion);
            break;
        }
    }
    log_warning(logs_auxiliares, "Eliminando interfaz %s de tipo %d", datosInterfaz->nombre, datosInterfaz->tipoInterfaz);
    switch (datosInterfaz->tipoInterfaz) {
    case GENERICA:   
        if ( !desconectarInterfaz(interfacesGenericas, mutexInterfacesGenericas, datosInterfaz) ) {
            log_error(logs_auxiliares, "Error al eliminar interfaz %s de tipo %d", datosInterfaz->nombre, datosInterfaz->tipoInterfaz);
        }
        break;
    case STDIN:
        if ( !desconectarInterfaz(interfacesSTDIN, mutexInterfacesSTDIN, datosInterfaz) ) {
            log_error(logs_auxiliares, "Error al eliminar interfaz %s de tipo %d", datosInterfaz->nombre, datosInterfaz->tipoInterfaz);
        }
        break;
    case STDOUT:
        if ( !desconectarInterfaz(interfacesSTDOUT, mutexInterfacesSTDOUT, datosInterfaz) ) {
            log_error(logs_auxiliares, "Error al eliminar interfaz %s de tipo %d", datosInterfaz->nombre, datosInterfaz->tipoInterfaz);
        }
        break;
    case FS:
        if ( !desconectarInterfaz(interfacesFS, mutexInterfacesFS, datosInterfaz) ) {
            log_error(logs_auxiliares, "Error al eliminar interfaz %s de tipo %d", datosInterfaz->nombre, datosInterfaz->tipoInterfaz);
        }
        break;
    }
}

void iteradorPaquete(char* value) {
	log_info(logs_auxiliares,"%s", value);
}

interfazConectada* crearInterfaz(t_list* nombreYTipoInterfaz, int socket_cliente) {
    interfazConectada* interfazIO = malloc(sizeof(interfazConectada));
    if ( interfazIO == NULL ) {
        log_error(logs_error, "Error al crear la estructura de interfaz %s", (char*) list_get(nombreYTipoInterfaz, 0));
    }
    interfazIO->nombre = (char*) list_get(nombreYTipoInterfaz, 0);
    interfazIO->tipoInterfaz = *(typeInterface*) list_get(nombreYTipoInterfaz, 1);
    sem_t libre;
    sem_init(&libre, 0, 1);
    interfazIO->libre = libre;
    interfazIO->colaEjecucion = queue_create();
    pthread_mutex_t semaforoMutex;
    pthread_mutex_init(&semaforoMutex, NULL);
    interfazIO->semaforoMutex = semaforoMutex;
    sem_t semaforoCantProcesos;
    sem_init(&semaforoCantProcesos, 0, 0);
    interfazIO->semaforoCantProcesos = semaforoCantProcesos;
    interfazIO->fd_interfaz = socket_cliente;
    log_info(logs_auxiliares, "Interfaz de tipo %d y nombre: %s", interfazIO->tipoInterfaz, interfazIO->nombre);
    switch (interfazIO->tipoInterfaz) {
    case GENERICA:
        pthread_mutex_lock(&mutexInterfacesGenericas);
        list_add(interfacesGenericas, interfazIO);
        pthread_mutex_unlock(&mutexInterfacesGenericas);
        break;
    case STDIN:
        pthread_mutex_lock(&mutexInterfacesSTDIN);
        list_add(interfacesSTDIN, interfazIO);
        pthread_mutex_unlock(&mutexInterfacesSTDIN);
        break;
    case STDOUT:
        pthread_mutex_lock(&mutexInterfacesSTDOUT);
        list_add(interfacesSTDOUT, interfazIO);
        pthread_mutex_unlock(&mutexInterfacesSTDOUT);
        break;
    case FS:
        pthread_mutex_lock(&mutexInterfacesFS);
        list_add(interfacesFS, interfazIO);
        pthread_mutex_unlock(&mutexInterfacesFS);
        break;
    default:
        return NULL;
        break;
    }
    return interfazIO;
}

bool escucharServer(int socket_servidor) {
    // Escucho el socket constantemente ya que es bloqueante
    int socket_cliente = esperar_cliente(socket_servidor, logs_auxiliares, logs_error);
    // Si aparece alguien:
    if ( socket_cliente != -1 ) {
        // Creo hilo y le asigno atender_cliente pasandole el socket como parametro
        recibir_operacion(socket_cliente);
        t_list* nombreYTipoInterfaz = recibir_paquete(socket_cliente);
        interfazConectada* interfazNueva = crearInterfaz(nombreYTipoInterfaz, socket_cliente);
        if (interfazNueva == NULL) {
            log_error(logs_error, "Interfaz %s de tipo %s no pudo ser creada", (char*) list_get(nombreYTipoInterfaz, 0), obtenerTipoInterfaz(*(typeInterface*) list_get(nombreYTipoInterfaz, 1)));
            return true;
        }
        // Primero nombre y despues tipoInterfaz
        pthread_t thread_cliente;
        crearHiloDetach(&thread_cliente, (void*) atender_cliente, (interfazConectada*) interfazNueva, "Cliente", logs_auxiliares, logs_error);
        list_destroy(nombreYTipoInterfaz);
        log_info(logs_auxiliares, "Interfaz conectada: %s", interfazNueva->nombre);
        return true;
    }
    return false;
}

void enviar_handshake() {
    // Envio los mensajes iniciales
    enviar_mensaje("Soy Kernel!", fd_memoria);
    enviar_mensaje("Soy Kernel por dispatch!", fd_cpu_dispatch);
    enviar_mensaje("Soy Kernel por interrupt!", fd_cpu_interrupt);
}

void crearLogs() {
    logs_auxiliares = log_create("logsExtras2.log", "[EXTRA]", false, LOG_LEVEL_INFO);
    logs_obligatorios = log_create("obligatoriosKernel.log", "[OBLIGATORIOS]", false, LOG_LEVEL_INFO);
    logs_error = log_create("logsExtras2.log", "[ERROR]", false, LOG_LEVEL_INFO);
    // Comprobacion de logs creador correctamente
    if ( logs_auxiliares == NULL || logs_obligatorios == NULL || logs_error == NULL) {
        terminarPrograma();
        abort();
    }
}

bool crearConexiones() {

    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, logs_error);

    fd_cpu_dispatch = crear_conexion(IP_CPU, PUERTO_CPU_DISPATCH, logs_error);

    fd_cpu_interrupt = crear_conexion(IP_CPU, PUERTO_CPU_INTERRUPT, logs_error);

    return true;
}

void iniciarConfig() {
    // Inicializacion configuracion
    config = iniciar_config(rutaConfiguracion, logs_error, (void*)terminarPrograma); 
    leerConfig();
    return;
}

alg_planificacion obtenerAlgoritmo() {
    char* algoritmo = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    if ( string_equals_ignore_case(algoritmo, "FIFO") ) {
        return FIFO;
    } else if ( string_equals_ignore_case(algoritmo, "RR") ) {
        return RR;
    } else if ( string_equals_ignore_case(algoritmo, "VRR") ) {
        return VRR;
    } else {
        log_error(logs_error, "Algoritmo no reconocido: %s", algoritmo);
        abort();
    }
}

void leerConfig() {
    PUERTO_ESCUCHA = config_get_string_value(config, "PUERTO_ESCUCHA");
    IP_MEMORIA = config_get_string_value(config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(config, "PUERTO_MEMORIA");
    IP_CPU = config_get_string_value(config, "IP_CPU");
    PUERTO_CPU_DISPATCH = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    PUERTO_CPU_INTERRUPT = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    ALGORITMO_PLANIFICACION = obtenerAlgoritmo();
    QUANTUM = config_get_int_value(config, "QUANTUM");
    RECURSOS = config_get_array_value(config, "RECURSOS");
    char** arrayInstancias = string_array_new();
    arrayInstancias = config_get_array_value(config, "INSTANCIAS_RECURSOS");
    INSTANCIAS_RECURSOS = string_array_as_int_array(arrayInstancias);
    GRADO_MULTIPROGRAMACION = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    string_array_destroy(arrayInstancias);
    log_info(logs_auxiliares, "Configuracion cargada correctamente");
}

int* string_array_as_int_array(char** arrayInstancias) {
    int cantidadNumeros = string_array_size(arrayInstancias);
    int* numeros = malloc(sizeof(int) * cantidadNumeros);
    for ( int i = 0; i < cantidadNumeros; i++ ) {
        int numero = atoi(arrayInstancias[i]);
        numeros[i] = numero;
    }
    return numeros;
}

void enviarPCBExit(t_pcb* pcb) {
    agregarPcbCola(cola_exit, sem_cola_exit, pcb);
    cambiarEstado(EXIT, pcb);
    sem_post(&semContadorColaExit);
    log_info(logs_auxiliares, "pid: %d enviado a exit", pcb->contexto_ejecucion.pid);
}

void enviarProcesosInterfazAExit (t_queue* cola) {
    pthread_mutex_lock(&sem_cola_blocked);
    while(!queue_is_empty(cola)) {
        uint32_t* pid = queue_pop(cola);
        for(int i = 0; i < list_size(cola_blocked); i++) {
            t_pcb* procesoLeido = (t_pcb*) list_get(cola_blocked, i);
            if ( *pid == procesoLeido->contexto_ejecucion.pid ) {
                list_remove(cola_blocked, i);
                procesoLeido->contexto_ejecucion.motivoFinalizacion = INVALID_INTERFACE;
                enviarPCBExit(procesoLeido);
                break;
            }
        }
    }
    pthread_mutex_unlock(&sem_cola_blocked);
    queue_destroy(cola);
}

void eliminarInterfaz(interfazConectada* interfazAEliminar) {
    pthread_mutex_destroy(&interfazAEliminar->semaforoMutex);
    enviarProcesosInterfazAExit(interfazAEliminar->colaEjecucion);
    sem_destroy(&interfazAEliminar->semaforoCantProcesos);
    sem_destroy(&interfazAEliminar->libre);
    free(interfazAEliminar);
}

void eliminarRecurso(recursoSistema* recursoAEliminar) {
    pthread_mutex_destroy(&recursoAEliminar->mutexCantidadInstancias);
    pthread_mutex_destroy(&recursoAEliminar->mutexCola);
    sem_destroy(&recursoAEliminar->semCantidadInstancias);
    sem_destroy(&recursoAEliminar->semCola);
    queue_destroy_and_destroy_elements(recursoAEliminar->cola, (void*) enviarPCBExit);
    free(recursoAEliminar);
}

void liberarRecursos() {
    list_destroy_and_destroy_elements(listaRecursosSistema, (void*) eliminarRecurso);
}

void liberarInterfaces() {
    list_destroy_and_destroy_elements(interfacesGenericas, (void*) eliminarInterfaz);
    list_destroy_and_destroy_elements(interfacesSTDIN, (void*) eliminarInterfaz);
    list_destroy_and_destroy_elements(interfacesSTDOUT, (void*) eliminarInterfaz);
    list_destroy_and_destroy_elements(interfacesFS, (void*) eliminarInterfaz);
    pthread_mutex_destroy(&mutexInterfacesGenericas);
    pthread_mutex_destroy(&mutexInterfacesSTDIN);
    pthread_mutex_destroy(&mutexInterfacesSTDOUT);
    pthread_mutex_destroy(&mutexInterfacesFS);
}

void terminarPrograma() {
    log_destroy(logs_obligatorios);
    log_destroy(logs_auxiliares);
    log_destroy(logs_error);
    config_destroy(config);
    liberar_conexion(socket_servidor);
    liberar_conexion(fd_memoria);
    liberar_conexion(fd_cpu_dispatch);
    liberar_conexion(fd_cpu_interrupt);
    queue_destroy_and_destroy_elements(cola_new, (void*)eliminar_pcb);
    queue_destroy_and_destroy_elements(cola_ready, (void*)eliminar_pcb);
    queue_destroy_and_destroy_elements(cola_exec, (void*)eliminar_pcb);
    list_destroy_and_destroy_elements(cola_blocked, (void*)eliminar_pcb);
    queue_destroy(cola_blocked_aux);
    queue_destroy_and_destroy_elements(cola_exit, (void*)eliminar_pcb);
    queue_destroy_and_destroy_elements(cola_ready_aux, (void*)eliminar_pcb);
    pthread_mutex_destroy(&mutexEliminarProceso);
    pthread_mutex_destroy(&mutexpidAEnviarExit);
    pthread_mutex_destroy(&mutexInterfazAEliminar);
    pthread_mutex_destroy(&sem_planificacion);
    pthread_mutex_destroy(&sem_cola_new);
    pthread_mutex_destroy(&sem_cola_ready);
    pthread_mutex_destroy(&sem_cola_exec);
    pthread_mutex_destroy(&sem_cola_blocked_aux);
    pthread_mutex_destroy(&sem_cola_blocked);
    pthread_mutex_destroy(&sem_cola_exit);
    pthread_mutex_destroy(&sem_grado_multiprogramacion);
    pthread_mutex_destroy(&mx_cantidad_ejecutandose);
    pthread_cond_destroy(&condicion_planificacion);
    sem_destroy(&semContadorColaNew);
    sem_destroy(&semContadorColaReady);
    sem_destroy(&semContadorColaExec);
    sem_destroy(&semContadorColaBlocked);
    sem_destroy(&semContadorColaExit);
    liberarInterfaces();
    liberarRecursos();
}

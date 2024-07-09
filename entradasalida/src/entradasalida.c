#include "entradasalida.h"

int main(int argc, char* argv[]) {
    //Inicializa todo
    if(argc != 3){
        printf("Error faltan argumentos, tengo %d", argc);
        for(int i = 0; i < argc; i++) {
            printf("Argumentos recibidos: %s",argv[i]);
        }
        return 1;
    }
    nombre = argv[1];
    path_config = argv[2];
    inicializar();
    op_codigo codigoRecibido;
    int cantidadParametros;
    while( fd_kernel != -1 ){
        codigoRecibido = recibir_operacion(fd_kernel);
        if ( codigoRecibido == -1 ) {
            log_warning(logger_auxiliar, "Desconexion de kernel");
            break;
        }
        t_list* paquete = recibir_paquete(fd_kernel);
        u_int32_t pid = *(uint32_t*)list_get(paquete, 0);
        recibirIoDetail(paquete, 1);
        log_info(logger_obligatorio, "PID: %d - Operacion: %s",pid, enumToString(tipoInstruccion));
        switch (tipoInstruccion) {
        case IO_GEN_SLEEP:
            //chequeo que solo interfaces genericas puedan hacer io_gen_sleep Y lo mismo en los ifs de abajo
            if(TIPO_INTERFAZ != GENERICA){
                log_error(logger_error, "Se envió la instrucción IO_GEN_SLEEP a la interfaz no genérica: %s", nombre);
                break;
            }
            int cantidadUnidadesTrabajo = *(int*) list_get(parametrosRecibidos, 0);
            int tiempoSleep = cantidadUnidadesTrabajo * TIEMPO_UNIDAD_TRABAJO;
            usleep(tiempoSleep);
            enviar_codigo_op(OK_OPERACION, fd_kernel);
            break;

        case IO_STDIN_READ:
            if(TIPO_INTERFAZ != STDIN){
                log_error(logger_error, "Se envió la instrucción IO_STDIN_READ a la interfaz no STDIN: %s", nombre);
                break;
            }
            cantidadParametros = list_size(parametrosRecibidos) - 1;
            if(0 < cantidadParametros ){
                uint32_t direccionesMemoria[cantidadParametros];
                for(int i=0; i < cantidadParametros; i++){
                    direccionesMemoria[i] = *(int*) list_get(parametrosRecibidos, i);
                }
                int tamanio = *(int*) list_get(parametrosRecibidos, cantidadParametros);
                char* valorLeido = readline("Ingrese cadena > ");
                if (valorLeido != NULL) {
                    // Si la longitud de la cadena excede 'cantidad', trunca la cadena
                    if (strlen(valorLeido) > tamanio) {
                        (valorLeido)[tamanio] = '\0';
                    }
                }
                t_paquete* paqueteMemoria = crear_paquete(ESCRIBIR_VALOR_MEMORIA);
                agregar_a_paquete(paqueteMemoria, &cantidadParametros, sizeof(int));
                for (int i = 0; i < cantidadParametros; i++){
                    agregar_a_paquete(paqueteMemoria, &direccionesMemoria[i], sizeof(int));
                }
                tamanio++;
                agregar_a_paquete(paqueteMemoria, &pid, sizeof(int));
                agregar_a_paquete(paqueteMemoria, &tamanio, sizeof(uint32_t));
                agregar_a_paquete(paqueteMemoria, valorLeido, tamanio);
                enviar_paquete(paqueteMemoria, fd_memoria);
                eliminar_paquete(paqueteMemoria);
                //Ver como recibo valor de memoria
                op_codigo op = recibir_operacion(fd_memoria);
                /*switch (op){
                case ESCRIBIR_VALOR_MEMORIA:
                    log_info(logger_auxiliar, "Se escribio el valor '%s' en memoria", valorLeido);
                    enviar_codigo_op(OK_OPERACION, fd_kernel);
                    break;
                default:
                    log_error(logger_error, "Fallo escritura de Memoria con el valor: %s", valorLeido);
                    //enviar_codigo_op(ERROR_OPERACION, fd_kernel);
                    break;
                }
                */
                enviar_codigo_op(OK_OPERACION, fd_kernel);
                free(valorLeido);
            }else{
                log_error(logger_error, "No se recibio lo necesario para escribir en memoria");
                enviar_codigo_op(ERROR_OPERACION, fd_kernel);
            }
            break;

        case IO_STDOUT_WRITE:
            if(TIPO_INTERFAZ != STDOUT){
                log_error(logger_error, "Se envió la instrucción IO_STDOUT_WRITE a la interfaz no STDOUT: %s", nombre);
                break;
            }
            cantidadParametros = list_size(parametrosRecibidos) - 1;
            if(0 < cantidadParametros ){
                int direccionesMemoria[cantidadParametros];
                for(int i=0; i < cantidadParametros; i++){
                    direccionesMemoria[i] = *(int*) list_get(parametrosRecibidos, i);
                }
                int tamanio = *(int*) list_get(parametrosRecibidos, cantidadParametros);
                tamanio++;
                t_paquete* paqueteMemoria = crear_paquete(LEER_VALOR_MEMORIA);
                //agregar_a_paquete(paqueteMemoria, &(cantidadParametros-1), sizeof(int));
                agregar_a_paquete(paqueteMemoria, &cantidadParametros, sizeof(int));
                for (int i = 0; i < cantidadParametros; i++){
                    agregar_a_paquete(paqueteMemoria, &direccionesMemoria[i], sizeof(int));
                }
                agregar_a_paquete(paqueteMemoria, &pid, sizeof(int));
                agregar_a_paquete(paqueteMemoria, &tamanio, sizeof(uint32_t));
                enviar_paquete(paqueteMemoria, fd_memoria);
                eliminar_paquete(paqueteMemoria);
                //Ver como recibo valor de memoria
                op_codigo op = recibir_operacion(fd_memoria);
                while (op != LEER_VALOR_MEMORIA)
                {
                    op = recibir_operacion(fd_memoria);
                }
                switch (op){
                case LEER_VALOR_MEMORIA:
                    t_list* paqueteRecibido = recibir_paquete(fd_memoria);
                    char* valorAMostrar = list_get(paqueteRecibido, list_size(paqueteRecibido) - 1); //En el ultimo valor de la lista de valores leidos, se encuentra el valor completo (o final)
                    log_info(logger_auxiliar, "%s", valorAMostrar);
                    free(valorAMostrar);
                    enviar_codigo_op(OK_OPERACION, fd_kernel);
                    list_destroy(paqueteRecibido);
                    break;
                default:
                    log_error(logger_error, "Fallo lectura de Memoria");
                    enviar_codigo_op(ERROR_OPERACION, fd_kernel);
                    break;
                }
            }else{
                log_error(logger_error, "No se recibio lo necesario para leer de memoria");
                enviar_codigo_op(ERROR_OPERACION, fd_kernel);
            }
            break;
        
        case IO_FS_CREATE:
            if(TIPO_INTERFAZ != FS){
                log_error(logger_error, "Se envio la instrucción IO_FS_CREATE a la interfaz no FS: %s", nombre);
                break;
            }
            cantidadParametros = list_size(parametrosRecibidos);
            if (0 < cantidadParametros){
                char* nombre_archivo_a_crear = (char*) list_get(parametrosRecibidos, 0);
                io_fs_create(nombre_archivo_a_crear);
            }
            enviar_codigo_op(OK_OPERACION, fd_kernel);
            break;

        case IO_FS_DELETE:
            if(TIPO_INTERFAZ != FS){
                log_error(logger_error, "Se envió la instrucción IO_FS_DELETE a la interfaz no FS: %s", nombre);
                break;
            }
            break;

        case IO_FS_READ:
            if(TIPO_INTERFAZ != FS){
                log_error(logger_error, "Se envió la instrucción IO_FS_READ a la interfaz no FS: %s", nombre);
                break;
            }
            break;

        case IO_FS_WRITE:
            if(TIPO_INTERFAZ != FS){
                log_error(logger_error, "Se envió la instrucción IO_FS_WRITE a la interfaz no FS: %s", nombre);
                break;
            }
            break;

        case IO_FS_TRUNCATE:
            if(TIPO_INTERFAZ != FS){
                log_error(logger_error, "Se envio la instrucción IO_FS_TRUNCATE a la interfaz no FS: %s", nombre);
                break;
            }
            cantidadParametros = list_size(parametrosRecibidos);
            if (0 < cantidadParametros){
                char* nombre_archivo_a_truncar = (char*) list_get(parametrosRecibidos, 0);
                uint32_t nuevo_tamanio_archivo = *(uint32_t*) list_get(parametrosRecibidos, 1);
                io_fs_truncate(nombre_archivo_a_truncar, nuevo_tamanio_archivo);
            }
            enviar_codigo_op(OK_OPERACION, fd_kernel);
            break;
        
        default:
            log_error(logger_error, "Se recibio una instruccion no esperada: %d", tipoInstruccion);
            break;
        }
        list_destroy(paquete);
        list_clean(parametrosRecibidos);
    }
    terminarPrograma();
    return 0;
}

void recibirIoDetail(t_list* listaPaquete, int ultimo_indice) {

    uint32_t cantidad_parametros_io_detail = *(uint32_t*)list_get(listaPaquete, ultimo_indice);

    if (cantidad_parametros_io_detail != 0) {

        for (int i = 0; i < cantidad_parametros_io_detail; i++) {

            ultimo_indice++;
            tipo_de_dato tipo_de_dato_parametro_io = *(tipo_de_dato*) list_get(listaPaquete, ultimo_indice);

            //t_params_io* parametro_io_a_guardar;
            
            ultimo_indice++;
            void* valor_parametro_io_recibido = (void*) list_get(listaPaquete, ultimo_indice);
            void* valor_parametro_a_guardar;

            switch (tipo_de_dato_parametro_io) {
            case INT:
                //parametro_io_a_guardar = malloc(sizeof(int)*2);
                valor_parametro_a_guardar = malloc(sizeof(int));
                valor_parametro_a_guardar = (int*)valor_parametro_io_recibido;
                break;
            case UINT32:
                valor_parametro_a_guardar = malloc(sizeof(uint32_t));
                valor_parametro_a_guardar = (uint32_t *)valor_parametro_io_recibido;
                //log_info(logger_auxiliar, "Se envia el parametro %d", *(uint32_t*)valor_parametro_a_guardar);
                break;
            case STRING:
                valor_parametro_a_guardar = (char *)valor_parametro_io_recibido;
                break;
            default:
                log_error(logger_error, "Error tipo de dato recibido");
                break;
            }

            //parametro_io_a_guardar->tipo_de_dato = tipo_de_dato_parametro_io; //almaceno el tipo de dato del parametro de la instruccion de io 
            //(esto va a servir mas adelante para que kernel pueda usarlo correctamente, ya que puede recibir char* o int)
            //parametro_io_a_guardar->valor = valor_parametro_a_guardar; //almaceno el valor del parametro de la instruccion de io

            list_add_in_index(parametrosRecibidos, i, valor_parametro_a_guardar); //almaceno el parametro en la lista de parametros que usara kernel luego
        }
        ultimo_indice++;
        //pcb->contexto_ejecucion.io_detail.nombre_io = (char *)list_get(contexto, ultimo_indice); //obtengo el nombre de la IO
        
        //ultimo_indice++;
        tipoInstruccion = *(t_nombre_instruccion *)list_get(listaPaquete, ultimo_indice); //obtengo el nombre de la instruccion contra IO
    }
}

/*
void enviarMsj(){
    char* comandoLeido = readline("String > ");
    enviar_mensaje(comandoLeido, socketAEnviar);
    log_info(logger_auxiliar, "Mensaje enviado");
    free(comandoLeido);
}

void enviarPaquete() {
    char* comandoLeido;
	t_paquete* paquete = crear_paquete(PAQUETE);

	// Leemos y esta vez agregamos las lineas al paquete
	comandoLeido = readline("String > "); // Leo de consola
	while (strcmp(comandoLeido, "")){ // Mientras no sea cadena vacia
		agregar_a_paquete(paquete, comandoLeido, strlen(comandoLeido)+1); // Agregamos al paquete el stream
		comandoLeido = readline("String > "); // Leo nueva linea
	}
	enviar_paquete(paquete, socketAEnviar); // Enviamos el paquete
    log_info(logger_auxiliar, "Paquete enviado");
	free(comandoLeido);
	eliminar_paquete(paquete);
}

void enviarOperacionA() {
    char* moduloNombre; // No hace falta liberar ya que es cadena literal
    pthread_t hiloAEnviar;
    if ( !strcmp(enviarA, "KERNEL") ) {
        hiloAEnviar = hilo_kernel;
        socketAEnviar = fd_kernel;
        moduloNombre = "Kernel";
    }
    else if ( !strcmp(enviarA, "MEMORIA") ) {
        hiloAEnviar = hilo_memoria;
        socketAEnviar = fd_memoria;
        moduloNombre = "Memoria";
    }
    else {
        log_error(logger_error, "Destino Incorrecto");
        return;
    }
    crearHiloJoin(&hiloAEnviar, (void*) tipoOperacion, NULL, moduloNombre, logger_auxiliar, logger_error);
}

op_codigo transformarAOperacion(char* operacionLeida) {
    string_to_upper(operacionLeida);
    if ( !strcmp(operacionLeida, "MENSAJE") ) { // strcmp devuelve 0 si son iguales
        return MENSAJE;
    } else if ( !strcmp(operacionLeida, "PAQUETE") ) {
        return PAQUETE;
    } else {
        return -1; // Valor por defecto para indicar error
    }
}
*/

void inicializar(){
    inicializarLogs();

    inicializarConfig();

    inicializarConexiones();

    if (TIPO_INTERFAZ == FS) {
        levantarArchivoDeBloques();
        levantarArchivoDeBitmap();
        map_archivos_metadata = dictionary_create();
    }

    parametrosRecibidos = list_create();
}

void inicializarLogs(){
    logger_obligatorio = log_create("entradasalida.log", "LOG_OBLIGATORIO_ENTRADA-SALIDA", true, LOG_LEVEL_INFO);
    logger_auxiliar = log_create("entradasalidaExtras.log", "LOG_EXTRA_ENTRADA_SALIDA", true, LOG_LEVEL_INFO);
    logger_error = log_create("entradasalidaExtras.log", "LOG_ERROR_ENTRADA_SALIDA", false, LOG_LEVEL_ERROR);
    // Compruebo que los logs se hayan creado correctamente
    if (logger_auxiliar == NULL || logger_obligatorio == NULL || logger_error == NULL) {
        terminarPrograma();
        abort();
    }
}

void inicializarConfig(){
    configuracion = iniciar_config(path_config, logger_error, (void*)terminarPrograma);
    leerConfig();
}

tipo_de_interfaz leerTipoDeInterfaz() {
    char* tipo = config_get_string_value(configuracion, "TIPO_INTERFAZ");
    if ( string_equals_ignore_case(tipo, "GENERICA") ) {
        return GENERICA;
    } else if ( string_equals_ignore_case(tipo, "STDIN") ) {
        return STDIN;
    } else if ( string_equals_ignore_case(tipo, "STDOUT") ) {
        return STDOUT;
    } else if ( string_equals_ignore_case(tipo, "DIALFS") ) {
        return FS;
    } else {
        log_error(logger_error, "Tipo de interfaz no reconocida: %s", tipo);
        abort();
    }
}

void leerConfig() {
    IP_KERNEL = config_get_string_value(configuracion, "IP_KERNEL");
    PUERTO_KERNEL = config_get_string_value(configuracion, "PUERTO_KERNEL");
    TIPO_INTERFAZ = leerTipoDeInterfaz();
    //log_info(logger_auxiliar, "Tipo interfaz: %d", TIPO_INTERFAZ);
    switch (TIPO_INTERFAZ) {
    case GENERICA:
        TIEMPO_UNIDAD_TRABAJO = config_get_int_value(configuracion, "TIEMPO_UNIDAD_TRABAJO");            
        break;
    case STDIN:
        IP_MEMORIA = config_get_string_value(configuracion, "IP_MEMORIA");
        PUERTO_MEMORIA = config_get_string_value(configuracion, "PUERTO_MEMORIA");
        break;
    case STDOUT:
        TIEMPO_UNIDAD_TRABAJO = config_get_int_value(configuracion, "TIEMPO_UNIDAD_TRABAJO");
        IP_MEMORIA = config_get_string_value(configuracion, "IP_MEMORIA");
        PUERTO_MEMORIA = config_get_string_value(configuracion, "PUERTO_MEMORIA");
        break;
    case FS:
        TIEMPO_UNIDAD_TRABAJO = config_get_int_value(configuracion, "TIEMPO_UNIDAD_TRABAJO");
        IP_MEMORIA = config_get_string_value(configuracion, "IP_MEMORIA");
        PUERTO_MEMORIA = config_get_string_value(configuracion, "PUERTO_MEMORIA");
        PATH_BASE_DIALFS = config_get_string_value(configuracion, "PATH_BASE_DIALFS");
        BLOCK_SIZE = config_get_int_value(configuracion, "BLOCK_SIZE");
        BLOCK_COUNT = config_get_int_value(configuracion, "BLOCK_COUNT");
        RETRASO_COMPACTACION = config_get_int_value(configuracion, "RETRASO_COMPACTACION");
        break;        
    }
}

void inicializarConexiones(){
    inicializarConexionKernel();
    if ( TIPO_INTERFAZ != GENERICA ) {
        inicializarConexionMemoria();
    }
}

void inicializarConexionKernel()
{
    fd_kernel = crear_conexion(IP_KERNEL, PUERTO_KERNEL, logger_error);
    t_paquete* paquete = crear_paquete(OK_OPERACION);

	agregar_a_paquete(paquete, nombre, strlen(nombre)+1); // Agregamos al paquete el stream
	agregar_a_paquete(paquete, &TIPO_INTERFAZ, sizeof(int)); // Agregamos al paquete el stream
    enviar_paquete(paquete, fd_kernel);
	eliminar_paquete(paquete);
    //crearHiloDetach(&hilo_kernel, (void*)enviarPaquete, NULL, "Kernel", logger_auxiliar, logger_error);
}

void inicializarConexionMemoria()
{
    fd_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA, logger_error);
    enviarMsjMemoria();
    //crearHiloDetach(&hilo_memoria, (void*)enviarMsjMemoria, NULL, "Memoria", logger_auxiliar, logger_error);

}

// Función para sincronizar los cambios en el archivo mapeado
void sync_file(void *addr, size_t length) {
    if (msync(addr, length, MS_SYNC) == -1) {
        perror("msync");
        exit(EXIT_FAILURE);
    }
}

void levantarArchivoDeBloques() {

    char* path_bloques_de_datos = string_new();
    string_append(&path_bloques_de_datos, PATH_BASE_DIALFS);
    string_append(&path_bloques_de_datos, "/bloques.dat");
    fd_bloque_de_datos;

    if (access(path_bloques_de_datos, F_OK) == -1) { //validar si no existe el archivo
        fd_bloque_de_datos = open(path_bloques_de_datos, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd_bloque_de_datos == -1) {
            perror("Error al abrir bloques.dat");
            exit(EXIT_FAILURE);
        }
        if (ftruncate(fd_bloque_de_datos, BLOCK_COUNT) == -1) {
            perror("Error al truncar bloques.dat");
            exit(EXIT_FAILURE);
        }
    } else {
        fd_bloque_de_datos = open(path_bloques_de_datos, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }

    bloques_datos_addr = mmap(NULL, BLOCK_COUNT * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloque_de_datos, 0);
    
    if (bloques_datos_addr == MAP_FAILED) {
        perror("Error al mapear bloques.dat");
        exit(EXIT_FAILURE);
    }
}

void levantarArchivoDeBitmap() {

    char* path_bitmap = string_new();
    string_append(&path_bitmap, PATH_BASE_DIALFS);
    string_append(&path_bitmap, "/bitmap.dat");
    fd_bitmap;
    if (access(path_bitmap, F_OK) == -1) { //validar si no existe el archivo
        fd_bitmap = open(path_bitmap, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd_bitmap == -1) {
            perror("Error al abrir bitmap.dat");
            exit(EXIT_FAILURE);
        }
        if (ftruncate(fd_bitmap, BLOCK_COUNT) == -1) {
            perror("Error al truncar bitmap.dat");
            exit(EXIT_FAILURE);
        }
    } else {
        fd_bitmap = open(path_bitmap, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    }

    bitmap_addr = mmap(NULL, BLOCK_COUNT, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitmap, 0);
    
    if (bitmap_addr == MAP_FAILED) {
        perror("Error al mapear bitmap.dat");
        exit(EXIT_FAILURE);
    }

    // Crear el bitmap utilizando la estructura t_bitarray
    bitmap_mapeado = bitarray_create_with_mode((char *)bitmap_addr, BLOCK_COUNT, LSB_FIRST);
}

void escribir_metadata(t_metadata_archivo metadata, char* nombre_archivo) {

    char* path_metadata = string_new();
    string_append(&path_metadata, PATH_BASE_DIALFS);
    string_append(&path_metadata, "/");
    string_append(&path_metadata, nombre_archivo);
    int fd_metadata = open(path_metadata, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if (write(fd_metadata, &metadata, sizeof(t_metadata_archivo)) == -1) {
        perror("Error al escribir en archivo de metadata");
        exit(EXIT_FAILURE);
    }

    t_metadata_archivo* metadata_a_guardar_en_mapa = malloc(sizeof(t_metadata_archivo));
    *metadata_a_guardar_en_mapa = metadata;
    dictionary_put(map_archivos_metadata, nombre_archivo, metadata_a_guardar_en_mapa);
    close(fd_metadata);
}

t_metadata_archivo leer_metadata_archivo(char* nombre_archivo_a_leer) {
    return *(t_metadata_archivo*) dictionary_get(map_archivos_metadata, nombre_archivo_a_leer);
}

uint32_t buscar_primer_bloque_libre() {
    
    uint32_t bloque_libre = 0;
    while (bitarray_test_bit(bitmap_mapeado, bloque_libre) && bloque_libre < BLOCK_COUNT) {
        bloque_libre++;
    }

    return bloque_libre;
}

void io_fs_create(char *nombre_archivo_a_crear) {

    t_metadata_archivo metadata;

    // Encontrar un bloque libre en el bitmap
    uint32_t bloque_libre = buscar_primer_bloque_libre();

    // Si no hay bloques libres
    if (bloque_libre >= BLOCK_COUNT) {
        fprintf(stderr, "Error: No hay bloques libres disponibles.\n");
        exit(EXIT_FAILURE);
    }

    // Marcar el bloque como ocupado en el bitmap
    bitarray_set_bit(bitmap_mapeado, bloque_libre);
    sync_file(bitmap_addr, BLOCK_COUNT);

    // Escribir metadata del archivo en el archivo de metadata
    metadata.bloque_inicial = bloque_libre;
    metadata.tamanio_archivo = 0; // Empieza con tamaño 0 bytes
    //escribir metadata
    escribir_metadata(metadata, nombre_archivo_a_crear);
}

void liberar_bloques(uint32_t actual_nro_bloque_final_archivo, uint32_t tamanio_a_truncar_en_bloques) {
    for (int i = actual_nro_bloque_final_archivo; i > tamanio_a_truncar_en_bloques; i --) {
        bitarray_clean_bit(bitmap_mapeado, i);
    }
}

void achicar_archivo(char* nombre_archivo_a_truncar, uint32_t nuevo_tamanio_archivo, t_metadata_archivo metadata_archivo_a_truncar, uint32_t actual_nro_bloque_final_archivo) {

    uint32_t tamanio_a_truncar_en_bytes = metadata_archivo_a_truncar.tamanio_archivo - nuevo_tamanio_archivo;
    float resultado_tamanio_a_truncar_en_bytes = ((float)tamanio_a_truncar_en_bytes / (float)BLOCK_SIZE);
    uint32_t tamanio_a_truncar_en_bloques = ceil(resultado_tamanio_a_truncar_en_bytes);

    // Liberar los bloques que exceden al archivo
    liberar_bloques(actual_nro_bloque_final_archivo, tamanio_a_truncar_en_bloques);
    metadata_archivo_a_truncar.tamanio_archivo = nuevo_tamanio_archivo;

    //escribir metadata
    escribir_metadata(metadata_archivo_a_truncar, nombre_archivo_a_truncar);
    sync_file(bitmap_addr, BLOCK_COUNT);
}

uint32_t obtener_cantidad_de_bloques_libres_al_final_de_archivo(uint32_t actual_nro_bloque_final_archivo) {

    uint32_t cantidad_bloques_libres = 0;

    actual_nro_bloque_final_archivo++;
    while (actual_nro_bloque_final_archivo < bitarray_get_max_bit(bitmap_mapeado) && !bitarray_test_bit(bitmap_mapeado, actual_nro_bloque_final_archivo) && actual_nro_bloque_final_archivo < BLOCK_COUNT) {
        cantidad_bloques_libres++;
        actual_nro_bloque_final_archivo++;
    }

    return cantidad_bloques_libres;
}

uint32_t ocupar_bloques(uint32_t nro_bloque_inicio, uint32_t cantidad_bloques) {
    for (int i = nro_bloque_inicio; i < cantidad_bloques; i++) {
        bitarray_set_bit(bitmap_mapeado, i);
    }
}

uint32_t buscar_espacio_libre_contiguo_en_disco(uint32_t tamanio_a_truncar_en_bloques) {
    
    uint32_t nro_bloque_a_evaluar = 0;
    uint32_t cantidad_bloques_libres = 0;

    for (int i = 0; i < bitarray_get_max_bit(bitmap_mapeado) && cantidad_bloques_libres == tamanio_a_truncar_en_bloques; i++){
        if (bitarray_test_bit(bitmap_mapeado, i)) {
            nro_bloque_a_evaluar = i;
            cantidad_bloques_libres = 0;
        } else {
            cantidad_bloques_libres++;
        }
    }

    return cantidad_bloques_libres == tamanio_a_truncar_en_bloques ? nro_bloque_a_evaluar : -1;
}

uint32_t espacios_libres() {
    
    uint32_t cantidad_bloques_libres = 0;
    
    for (int i = 0; i < bitarray_get_max_bit(bitmap_mapeado); i++){
        if (!bitarray_test_bit(bitmap_mapeado, i)) {
            cantidad_bloques_libres++;
        }
    }

    return cantidad_bloques_libres;
}

void clear_bitmap() {
    for (int i = 0; i < bitarray_get_max_bit(bitmap_mapeado); i++) {
        bitarray_clean_bit(bitmap_mapeado, i);
    }
}

void fill_bitmap(uint32_t cantidad_a_fillear) {
    for (int i = 0; i < cantidad_a_fillear; i++) {
        bitarray_set_bit(bitmap_mapeado, i);
    }
}


void compactar() {
    
    void* bloques_de_archivos_nuevo = malloc(BLOCK_COUNT * BLOCK_SIZE);

    t_list* nombre_archivos_metadata = dictionary_keys(map_archivos_metadata);
    int pointer_memory_bloque_de_datos = 0;

    for (int i = 0; i < list_size(nombre_archivos_metadata); i++) {
        char* nombre_metadata_archivo = list_get(nombre_archivos_metadata, i);
        t_metadata_archivo metadata_archivo = *(t_metadata_archivo*) dictionary_get(map_archivos_metadata, nombre_metadata_archivo);
        
        float resultado_tamanio_actual =  ((float) metadata_archivo.tamanio_archivo / (float) BLOCK_SIZE);
        uint32_t tamanio_actual_en_bloques = ceil(resultado_tamanio_actual);
        
        uint32_t limite = metadata_archivo.bloque_inicial + tamanio_actual_en_bloques;

        uint32_t nuevo_bloque_inicial = pointer_memory_bloque_de_datos;
        for (int j = metadata_archivo.bloque_inicial; j < limite; j++) {
            memcpy(bloques_de_archivos_nuevo + (pointer_memory_bloque_de_datos * BLOCK_SIZE), bloques_datos_addr, BLOCK_SIZE);
            pointer_memory_bloque_de_datos++;
        }

        metadata_archivo.bloque_inicial = nuevo_bloque_inicial;
    }

    bloques_datos_addr = bloques_de_archivos_nuevo;

    //reseteamos todo el bitmap a 0 para luego volver a setearlo correspondiente a los bloques de datos que hayamos ocupado
    clear_bitmap();
    fill_bitmap(pointer_memory_bloque_de_datos);

    //escribimos en disco las modificaciones
    sync_file(bitmap_addr, BLOCK_COUNT);
    sync_file(bloques_datos_addr, BLOCK_COUNT * BLOCK_SIZE);
}

void io_fs_truncate(char* nombre_archivo_a_truncar, uint32_t nuevo_tamanio_archivo) {
    
    t_metadata_archivo metadata_archivo_a_truncar = leer_metadata_archivo(nombre_archivo_a_truncar);

    float resultado_tamanio_actual_en_bloques_de_archivo_a_truncar =  ((float) metadata_archivo_a_truncar.tamanio_archivo / (float) BLOCK_SIZE);
    uint32_t tamanio_actual_en_bloques_de_archivo_a_truncar = ceil(resultado_tamanio_actual_en_bloques_de_archivo_a_truncar);

    uint32_t actual_nro_bloque_inicial_archivo = metadata_archivo_a_truncar.bloque_inicial;
    uint32_t actual_nro_bloque_final_archivo = actual_nro_bloque_inicial_archivo + tamanio_actual_en_bloques_de_archivo_a_truncar - 1;

    //Primer chequeo: Debe crecer o achicarse el archivo

    if (nuevo_tamanio_archivo < metadata_archivo_a_truncar.tamanio_archivo) {
        achicar_archivo(nombre_archivo_a_truncar, nuevo_tamanio_archivo, metadata_archivo_a_truncar, actual_nro_bloque_final_archivo);
        return;
    }

    if (nuevo_tamanio_archivo > metadata_archivo_a_truncar.tamanio_archivo) {
        uint32_t tamanio_a_truncar_en_bytes = metadata_archivo_a_truncar.tamanio_archivo - nuevo_tamanio_archivo;
        float resultado_tamanio_a_truncar_en_bytes = ((float)tamanio_a_truncar_en_bytes / (float)BLOCK_SIZE);
        uint32_t tamanio_a_truncar_en_bloques = ceil(resultado_tamanio_a_truncar_en_bytes);

        uint32_t bloques_libres_al_final = obtener_cantidad_de_bloques_libres_al_final_de_archivo(actual_nro_bloque_final_archivo);
        
        //validamos si hay suficientes bloques libres una vez finalizado el archivo

        if (bloques_libres_al_final >= tamanio_a_truncar_en_bloques) {
            ocupar_bloques(actual_nro_bloque_final_archivo, tamanio_a_truncar_en_bloques);
            metadata_archivo_a_truncar.tamanio_archivo = tamanio_a_truncar_en_bytes;
            //escribir metadata
            escribir_metadata(metadata_archivo_a_truncar, nombre_archivo_a_truncar);
            sync_file(bitmap_addr, BLOCK_COUNT);
            return;
        }

        //si no hay suficientes bloques libres al final del archivo...
        if (bloques_libres_al_final < tamanio_a_truncar_en_bloques) {
            //Primero, antes de compactar, hacemos una busqueda a ver si no entra en algun lugar del disco el archivo completo (si encontramos, no es necesario compactar)
            uint32_t bloque_inicio_de_espacio_encontrado = buscar_espacio_libre_contiguo_en_disco(tamanio_a_truncar_en_bloques);
        
            if (bloque_inicio_de_espacio_encontrado != -1) {
                //Se encontro un espacio contiguo libre suficiente en el disco
                liberar_bloques(actual_nro_bloque_inicial_archivo, tamanio_actual_en_bloques_de_archivo_a_truncar);
                metadata_archivo_a_truncar.bloque_inicial = bloque_inicio_de_espacio_encontrado;
                metadata_archivo_a_truncar.tamanio_archivo = nuevo_tamanio_archivo;
                ocupar_bloques(bloque_inicio_de_espacio_encontrado, tamanio_a_truncar_en_bloques);
                escribir_metadata(metadata_archivo_a_truncar, nombre_archivo_a_truncar);
                sync_file(bitmap_addr, BLOCK_COUNT);
                return;
            }

            if (espacios_libres() >= tamanio_a_truncar_en_bloques) {
                //Quedan espacios libres, pero estan dispersos, no contiguos, por lo tanto debemos compactar
                compactar();
                uint32_t primer_bloque_libre = buscar_primer_bloque_libre();
                metadata_archivo_a_truncar.bloque_inicial = primer_bloque_libre;
				metadata_archivo_a_truncar.tamanio_archivo = nuevo_tamanio_archivo;

                escribir_metadata(metadata_archivo_a_truncar, nombre_archivo_a_truncar);
                return;
            }

            log_error(logger_error, "No queda mas espacio para truncar archivo con tamanio en bloques: %d", tamanio_a_truncar_en_bloques);
        }

    }
}

char* enumToString(t_nombre_instruccion nombreDeInstruccion){
    switch(nombreDeInstruccion){
        case IO_GEN_SLEEP:
            return "IO_GEN_SLEEP";
        case IO_STDIN_READ:
            return "IO_STDIN_READ";
        case IO_STDOUT_WRITE:
            return "IO_STDOUT_WRITE";
        case IO_FS_CREATE:
            return "IO_FS_CREATE";
        case IO_FS_DELETE:
            return "IO_FS_DELETE";
        case IO_FS_TRUNCATE:
            return "IO_FS_TRUNCATE";
        case IO_FS_WRITE:
            return "IO_FS_WRITE";
        case IO_FS_READ:
            return "IO_FS_READ";
        default:
            return "COMANDO NO RECONOCIDO";
    };
}

void enviarMsjMemoria(){
    enviar_mensaje("Hola, soy I/O!", fd_memoria);
}

void enviarMsjKernel(){
    enviar_mensaje("Hola, soy I/O!", fd_kernel);
}

void cerrar_archivos() {
    close(fd_bitmap);
    close(fd_bloque_de_datos);
}

void terminarPrograma() {
    log_destroy(logger_obligatorio);
    log_destroy(logger_auxiliar);
    log_destroy(logger_error);
    config_destroy(configuracion);
    liberar_conexion(fd_memoria);
    liberar_conexion(fd_kernel);
    cerrar_archivos();
}

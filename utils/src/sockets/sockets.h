#ifndef SOCKETS_H_
#define SOCKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>


/* ------------ STRUCTS --------*/
typedef enum {
    MENSAJE,
    PAQUETE,
} op_codigo;

typedef struct{
	int size;
	void* stream;
} t_buffer;
typedef struct{
	op_codigo codigo_operacion;
	t_buffer* buffer;
} t_paquete;


/*
Iniciamos el servidor
puerto = puerto al cual escuchar
loggerAuxiliar = logger para cargar datos extras
*/
int iniciar_servidor(char* puerto, t_log* logger);
/*
Creamos conexion contra el servidor
ip = ip a la cual conectarse
puerto = puerto al cual conectarse
loggerAuxiliar = logger para cargar datos extras
*/
int crear_conexion(char *ip, char* puerto, t_log* loggerAuxiliar);
/*
Esperamos a que el cliente se conecte
socket_servidor = socket al cual se van a conectar
loggerAuxiliar = logger para cargar datos extras
*/
int esperar_cliente(int socket_servidor, t_log* loggerAuxiliar);
/*
Recibimos mensaje del cliente
socket_cliente = socket por el cual se va a recibir informacion
COMPROBAR QUE DEVOLVER
*/
int recibir_operacion(int socket_cliente);
/*
Liberamos la conexion de espacio de memoria
socket = socket a liberar
*/
void liberar_conexion(int socket);
/*
Envia un mensaje string hacia el servidor indicado
mensaje = string a enviar
socket_cliente = socket por el cual enviar el mensaje
*/
void enviar_mensaje(char* mensaje, int socket_servidor);
/*
Serializa un paquete
paquete = paquete a serializar
bytes = bytes del contenido del paquete (tamaño buffer [sizeof(int)], buffer, tamaño codigo de op[sizeof(int)])
*/
void* serializar_paquete(t_paquete* paquete, int bytes);
/*
Elimina un paquete
paquete = paquete a eliminar de la memoria
*/
void eliminar_paquete(t_paquete* paquete);

#endif
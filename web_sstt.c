#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <stdbool.h>	// bool
#include <libgen.h>		// basename

#define VERSION		24
#define BUFSIZE		8096
#define ERROR		42
#define LOG			44
#define PROHIBIDO	403
#define NOENCONTRADO	404
#define SSERVER "Server: Test\r\n"

//- VARIABLES GLOBALES -//

char * sEmail = "valentin%40um.es"; // debido a formato, @ == %40
int * bodyLength = 0;				// Tamanyo del cuerpo -> BODY
long fSize = 0;						// Tamanyo de cada fichero


struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };

void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];
	
	switch (log_message_type) {
		case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case PROHIBIDO:
			// Enviar como respuesta 403 Forbidden
			(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",message, additional_info);
			break;
		case NOENCONTRADO:
			// Enviar como respuesta 404 Not Found
			(void)sprintf(logbuffer,"NOT FOUND: %s:%s",message, additional_info);
			break;
		case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",message, additional_info, socket_fd); break;
	}

	if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(log_message_type == ERROR || log_message_type == NOENCONTRADO || log_message_type == PROHIBIDO) exit(3);
}

/*Comprueba si existe un fichero pasado por parametro, intentando abrirlo*/
bool existeFichero(char * path)
{
	int fd = 0;
	if (fd = open(path, O_RDONLY) != -1)
		return true;

	return false;
}

/*Comprueba si existe el tipo de extension*/
bool parse_extension(char * path)
{
	int i = 0;
	// Obtenemos el archivo de la ruta
	char * ext = basename(path);
	// Obtenemos solo la extension
	while (*ext != '.')
		ext++;
	ext++;	//	despues del '.'
	//debug(LOG, "EXTENSION: ", ext, 4);
	for (i = 0; extensions[i].ext != 0; i++)
		if (!strcmp(extensions[i].ext, ext)){
			return true;
		}
	return false;
	
}

/*Devuelve el tipo de fichero asociado a una extension*/
//
//	Evaluar el tipo de fichero que se está solicitando, y actuar en
//	consecuencia devolviendolo si se soporta u devolviendo el error correspondiente en otro caso
//
char * content_type(char * path)
{
	int i = 0;
	// Obtenemos el archivo de la ruta
	char * ext = basename(path);
	// Obtenemos solo la extension
	while (*ext != '.')
		ext++;
	ext++;
	for (i = 0; extensions[i].ext != 0; i++)
		if (!strcmp(extensions[i].ext, ext))
			return extensions[i].filetype;	
}

/*Envia la cabecera HTTP OK */
void send_header(int sClient)
{
   char buf[BUFSIZE];
   strcpy(buf, "HTTP/1.1 200 OK\r\n");
   send(sClient, buf, strlen(buf), 0);
   strcpy(buf, SSERVER);
   write(sClient, buf, strlen(buf)); 		
}

//
//	En caso de que el fichero sea soportado, exista, etc. se envia el fichero con la cabecera
//	correspondiente, y el envio del fichero se hace en blockes de un máximo de  8kB
//
void send_file(int sClient, char * path)
{
	int fd = 0;
    char buf[BUFSIZE];
	int remain = 0;
	
	fd = open(path, O_RDONLY); 
	if (fd == -1)
		debug(ERROR, "Error al abrir fichero ", path, sClient);
	
	int rd = 0;
	int wr = 0;
	memset(buf, 0, BUFSIZE);
	
	while (rd = read(fd, buf, BUFSIZE) > 0)
	{
		
		remain = fSize - BUFSIZE;
				
		if (remain < 0)
			wr = write(sClient, buf, fSize);
		else
		{
			wr = write(sClient, buf, BUFSIZE);
			fSize-=BUFSIZE;
		}
			
	}

	close(fd);
}


/*Envia cabecera+cuerpo*/
void _send(int sClient, char * path)
{
	
	int fd = open(path, O_RDONLY); 
	if (fd <= 0)
		debug(ERROR, "Error al abrir fichero ", path, sClient);
	
	// Calcular tamanyo final-principio bytes
	fSize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET); // posicionar al principio otra vez
	
	send_header(sClient);						// envia HTTP 200 OK
	char content[BUFSIZE];
	sprintf(content, "Content-Length: %ld \r\n", fSize);
	strcat(content,"Content-Type: ");
	strcat(content, content_type(path));
	strcat(content, "\r\n\r\n");
	write(sClient, content, strlen(content));	// envia cabeceras adicionales
	send_file(sClient, path);					// envia el cuerpo (DATA)
	
	close(fd);
}


/*Comprueba si la cadena del email coincide con la cadena de la variable global*/
bool parse_email(int sClient, char * email)
{
	return (!strcmp(email, sEmail)); 
}

/*Este metodo permite analizar sintacticamente el buffer y realizar todo tipo de operaciones
necesarias para comunicarse con el cliente.*/
void parse_request(int sClient, char * buf)
{
   char method[1024];
   char url[1024];
	
   int i = 0; int j = 0;
  
   while (!isspace(buf[j]) && (i < sizeof(method) - 1))
   {
      method[i] = buf[j];
      i++;j++;
   }
   method[i] = '\0';
	
	i = 0;j++;
	url[i] = '.';i++; // directorio actual
   while (!isspace(buf[j]) && (i < sizeof(url) - 1) )
   {
      url[i] = buf[j];
      i++;j++;
   }
	
   url[i] = '\0';
	debug(LOG, "PATH: ", url, sClient);
	//
	//	Como se trata el caso de acceso ilegal a directorios superiores de la
	//	jerarquia de directorios
	//	del sistema
	//
/*Comprobar acceso al path
No se puede comprobar /.. ya que la consulta HTTP no describe exactamente los puntos -> acceso
al directorio padre. Uso de strncmp para evitar desbordamiento de buffer*/
if (!strncmp(url, "./bin", 5) || !strncmp(url, "./etc", 5) || !strncmp(url, "./dev", 5)
			|| !strncmp(url, "./tmp", 5) || !strncmp(url, "./bin", 5) || !strncmp(url, "./usr", 5)
			|| !strncmp(url, "./lib", 5) || !strncmp(url, "./sbin", 6) )
{
		if (existeFichero("bad_request.html"))
			_send(sClient, "bad_request.html");
		debug(PROHIBIDO, "Prohibido el acceso a ", url, sClient);
}
else
{
  if (!strcmp(method, "GET"))				// - GET - //
  {
	//	Como se trata el caso excepcional de la URL que no apunta a ningún fichero
	//	html
	 if (strlen(url) == 2)		// 2 caracteres: ./
	 {
		if (existeFichero("index.html"))
		_send(sClient, "index.html");
		else debug(NOENCONTRADO, "Fichero index.html no encontrado", " ", sClient);
		 
	 }
	else{	// en otro caso se envia el fichero en si
		if ((existeFichero(url)) && parse_extension(url))
			_send(sClient, url);
		else {
			_send(sClient, "not_found.html");
			debug(PROHIBIDO, "response", "Formato de extension incorrecto o no existe fichero", sClient);
		}
	} 
  }  	
  else if (!strcmp(method, "POST"))			// - POST - //
  	{
		if ((existeFichero(url)) && parse_extension(url))
		{
			char * ptrEmail = strstr(buf, "email="); // subcadena email con delimitadores
			// recortar delimitadores, tokens, etc..
			char * email = strchr(ptrEmail, '=');
			email++; // despues de '='

			if (parse_email(sClient, email))	// Analizar email
				_send(sClient, "correoValido.html");
			else
				_send(sClient, "correoNoValido.html");
			
		} else _send(sClient, "bad_request.html");
	} else {
		_send(sClient, "bad_request.html");
		debug(PROHIBIDO, "No compatible tipo de mensaje: ", method,  sClient);
	}
}

}
void process_web_request(int socket)
{
	debug(LOG,"request","Ha llegado una peticion",socket);
	//
	// Definir buffer y variables necesarias para leer las peticiones
	//
	char * ext;
	char buf[BUFSIZE];
	char bufSend[BUFSIZE];
	int readed;
	readed = read(socket, buf, BUFSIZE);
	
	//
	// Comprobación de errores de lectura
	//
	if (readed == -1 || readed == 0)
		debug(ERROR, "No se ha podido leer", " ", socket);
	
	
	//
	// Leer la petición HTTP
	//
	debug(LOG, "Peticion: ", buf, socket);
	
	//
	// Si la lectura tiene datos válidos terminar el buffer con un \0
	//
	if (readed > 0 && readed < BUFSIZE)
		buf[readed] = '\0';
	
	//
	// Se eliminan los caracteres de retorno de carro y nueva linea
	//
	int i = 0;
	for (i = 0; i<readed; i++)
		if (buf[i] == '\r' || buf[i] == '\n')
			buf[i] = '*';
	
	
	/*Este metodo permite analizar sintacticamente el buffer y realizar todo tipo de operaciones
	necesarias para comunicarse con el cliente.*/
	parse_request(socket, buf);

	close(socket);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr;		// static = Inicializado con ceros
	static struct sockaddr_in serv_addr;	// static = Inicializado con ceros
	
	//  Argumentos que se esperan:
	//
	//	argv[1]
	//	En el primer argumento del programa se espera el puerto en el que el servidor escuchara
	//
	//  argv[2]
	//  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
	//
	//  Verficiar que los argumentos que se pasan al iniciar el programa son los esperados
	//

	//
	//  Verficiar que el directorio escogido es apto. Que no es un directorio del sistema y que se tienen
	//  permisos para ser usado
	//

	if(chdir(argv[2]) == -1){ 
		(void)printf("ERROR: No se puede cambiar de directorio %s\n",argv[2]);
		exit(4);
	}
	// Hacemos que el proceso sea un demonio sin hijos zombies
	if(fork() != 0)
		return 0; // El proceso padre devuelve un OK al shell

	(void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
	(void)signal(SIGHUP, SIG_IGN); // Ignoramos cuelgues
	
	debug(LOG,"web server starting...", argv[1] ,getpid());
	
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		debug(ERROR, "system call","socket",0);
	
	port = atoi(argv[1]);
	
	if(port < 0 || port >60000)
		debug(ERROR,"Puerto invalido, prueba un puerto de 1 a 60000",argv[1],0);
	
	/*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/
	serv_addr.sin_port = htons(port); /*... en el puerto port especificado como parámetro*/
	
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0);
	
	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0);
	
	while(1){
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	// Proceso hijo
				(void)close(listenfd);
				process_web_request(socketfd); // El hijo termina tras llamar a esta función
			} else { 	// Proceso padre
				(void)close(socketfd);
			}
		}
	}
}

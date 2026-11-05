/*
** Fichero: servidor.c
** Autores:
** Unai García Blanco
** Jorge Caballero González
*/

/*
 *          		S E R V I D O R
 */
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>



#define PUERTO 1921
#define ADDRNOTFOUND    0xffffffff      /* return address for unfound host */
#define BUFFERSIZE      1024    /* maximum size of packets to be received */
#define TAM_BUFFER 512
#define MAXHOST 128

extern int errno;

typedef struct {
	char pregunta[TAM_BUFFER]; // Puedes ajustar el tamaño según tus necesidades
	int solucion;
} Datos;


//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES GENERALES
//------------------------------------------------------------------------
Datos readNextLine(FILE* file);
int esNumero(const char *cadena);
void imprimirConSecuenciasDeEscape(const char *cadena);
int RecibirTramaAdios(int s, char* bufffff, int* len);


//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES TCP
//------------------------------------------------------------------------
int RecibirTramaHolaTCP(int s, char* buf, int* len, char* hostname);
void RecibirTramaRespuestaTCP(int s, char* buff, int *len,int *retorno, int *temp2, Datos juego);


//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES UDP
//------------------------------------------------------------------------
void RecibirTramaHolaUDP(int s, char* buf, int *temp1, int *len, char* hostname);
void RecibirTramaRespuestaUDP(int s, char* buff, int *len,int *retorno, int *temp2, Datos juego);


//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES SERVIDOR
//------------------------------------------------------------------------
void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, struct sockaddr_in clientaddr_in);



/*
 *			M A I N
 *
 *	This routine starts the server.  It forks, leaving the child
 *	to do all the work, so it does not have to be run in the
 *	background.  It sets up the sockets.  It
 *	will loop forever, until killed by a signal.
 *
 */

struct sockaddr_in myaddr_in;   /* for local socket address */
struct sockaddr_in udpaddr_in;
struct sockaddr_in clientaddr_in;       /* for peer socket address */
int addrlen;

void errout(char*); /* declare error out routine */

int FIN = 0; /* Para el cierre ordenado */
void finalizar() { FIN = 1; }

int main(argc, argv) 
int argc;
char* argv[];
{

	int s_TCP, s_UDP; /* connected socket descriptor */
	int ls_TCP;		  /* listen socket descriptor */
	int ss_UDP;

	int cc; /* contains the number of bytes read */

	struct sigaction sa = { .sa_handler = SIG_IGN }; /* used to ignore SIGCHLD */

	fd_set readmask;
	int numfds, s_mayor;

	char buffer[BUFFERSIZE]; /* buffer for packets to be read into */

	struct sigaction vec;

	/* Create the listen socket. */
	ls_TCP = socket(AF_INET, SOCK_STREAM, 0);
	if (ls_TCP == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
		exit(1);
	}
	/* clear out address structures */
	memset((char*)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset((char*)&clientaddr_in, 0, sizeof(struct sockaddr_in));

	addrlen = sizeof(struct sockaddr_in);

	/* Set up address structure for the listen socket. */
	myaddr_in.sin_family = AF_INET;
	/* The server should listen on the wildcard address,
		 * rather than its own internet address.  This is
		 * generally good practice for servers, because on
		 * systems which are connected to more than one
		 * network at once will be able to have one server
		 * listening on all networks at once.  Even when the
		 * host is connected to only one network, this is good
		 * practice, because it makes the server program more
		 * portable.
		 */
	myaddr_in.sin_addr.s_addr = INADDR_ANY;
	myaddr_in.sin_port = htons(PUERTO);

	/* Bind the listen address to the socket. */
	if (bind(ls_TCP, (const struct sockaddr*)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(2);
	}
	/* Initiate the listen on the socket so remote users
		 * can connect.  The listen backlog is set to 5, which
		 * is the largest currently supported.
		 */
	if (listen(ls_TCP, 5) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
		exit(3);
	}

	/* Create the socket UDP. */
	s_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_UDP == -1)
	{
		perror(argv[0]);
		printf("%s: unable to create socket UDP\n", argv[0]);
		exit(4);
	}
	/* Bind the server's address to the socket. */
	if (bind(s_UDP, (struct sockaddr*)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
		perror(argv[0]);
		printf("%s: unable to bind address UDP\n", argv[0]);
		exit(5);
	}

	/* Now, all the initialization of the server is
		 * complete, and any user errors will have already
		 * been detected.  Now we can fork the daemon and
		 * return to the user.  We need to do a setpgrp
		 * so that the daemon will no longer be associated
		 * with the user's control terminal.  This is done
		 * before the fork, so that the child will not be
		 * a process group leader.  Otherwise, if the child
		 * were to open a terminal, it would become associated
		 * with that terminal as its control terminal.  It is
		 * always best for the parent to do the setpgrp.
		 */
	setpgrp();

	switch (fork())
	{
		case -1: /* Unable to fork, for some reason. */
			perror(argv[0]);
			fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
			exit(6);

		case 0: /* The child process (daemon) comes here. */

			/* Close stdin and stderr so that they will not
				* be kept open.  Stdout is assumed to have been
				* redirected to some logging file, or /dev/null.
				* From now on, the daemon will not report any
				* error messages.  This daemon will loop forever,
				* waiting for connections and forking a child
				* server to handle each one.
				*/
			fclose(stdin);
			fclose(stderr);

			/* Set SIGCLD to SIG_IGN, in order to prevent
				* the accumulation of zombies as each child
				* terminates.  This means the daemon does not
				* have to make wait calls to clean them up.
				*/
			if (sigaction(SIGCHLD, &sa, NULL) == -1)
			{
				perror(" sigaction(SIGCHLD)");
				fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
				exit(7);
			}

			/* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
			vec.sa_handler = (void*)finalizar;
			vec.sa_flags = 0;
			if (sigaction(SIGTERM, &vec, (struct sigaction*)0) == -1)
			{
				perror(" sigaction(SIGTERM)");
				fprintf(stderr, "%s: unable to register the SIGTERM signal\n", argv[0]);
				exit(8);
			}

			while (!FIN)
			{
				/* Meter en el conjunto de sockets los sockets UDP y TCP */
				FD_ZERO(&readmask);
				FD_SET(ls_TCP, &readmask);
				FD_SET(s_UDP, &readmask);
				/*
				Seleccionar el descriptor del socket que ha cambiado. Deja una marca en
				el conjunto de sockets (readmask)
				*/
				if (ls_TCP > s_UDP)
					s_mayor = ls_TCP;
				else
					s_mayor = s_UDP;

				if ((numfds = select(s_mayor + 1, &readmask, (fd_set*)0, (fd_set*)0, NULL)) < 0)
				{
					if (errno == EINTR)
					{
						FIN = 1;
						close(ls_TCP);
						close(s_UDP);
						perror("\nFinalizando el servidor. Senal recibida en select\n ");
					}
				}
				else
				{

					/* Comprobamos si el socket seleccionado es el socket TCP */
					if (FD_ISSET(ls_TCP, &readmask))
					{
						/* Note that addrlen is passed as a pointer
						* so that the accept call can return the
						* size of the returned address.
						*/
						/* This call will block until a new
						* connection arrives.  Then, it will
						* return the address of the connecting
						* peer, and a new socket descriptor, s,
						* for that connection.
						*/
						s_TCP = accept(ls_TCP, (struct sockaddr*)&clientaddr_in, &addrlen);
						if (s_TCP == -1)
							exit(9);
						switch (fork())
						{
							case -1: /* Can't fork, just exit. */

							case 0:			   /* Child process comes here. */
								close(ls_TCP); /* Close the listen socket inherited from the daemon. */
								serverTCP(s_TCP, clientaddr_in);
								exit(0);
							default: /* Daemon process comes here. */
								/* The daemon needs to remember
								* to close the new accept socket
								* after forking the child.  This
								* prevents the daemon from running
								* out of file descriptor space.  It
								* also means that when the server
								* closes the socket, that it will
								* allow the socket to be destroyed
								* since it will be the last close.
								*/
								close(s_TCP);
						}
					} /* De TCP*/
					/* Comprobamos si el socket seleccionado es el socket UDP */
					if (FD_ISSET(s_UDP, &readmask))
					{
						/* This call will block until a new
					* request arrives.  Then, it will
					* return the address of the client,
					* and a buffer containing its request.
					* BUFFERSIZE - 1 bytes are read so that
					* room is left at the end of the buffer
					* for a null character.
					*/
						cc = recvfrom(s_UDP, buffer, BUFFERSIZE - 1, 0,
							(struct sockaddr*)&clientaddr_in, &addrlen);
						if (cc == -1)
						{
							perror(argv[0]);
							printf("%s: recvfrom error\n", argv[0]);
							exit(11);
						}
						/* Make sure the message received is
					* null terminated.
					*/
						buffer[cc] = '\0';
						if (!strcmp(buffer, "********************************************\r\n"))
							ss_UDP = socket(AF_INET, SOCK_DGRAM, 0);

						if (ss_UDP == -1) 
							exit(-1);

						udpaddr_in.sin_family = AF_INET;
						udpaddr_in.sin_addr.s_addr = INADDR_ANY;
						udpaddr_in.sin_port = 0;

						if (bind(ss_UDP, (struct sockaddr*)&udpaddr_in, sizeof(struct sockaddr_in)) == -1) {
							perror(argv[0]);
							printf("(S)%s: unable to bind address UDP\n", argv[0]);
							exit(1);
						}

						switch (fork()) 
						{
							case -1:
								exit(-1);
							case 0:
								close(s_UDP);
								serverUDP(ss_UDP, clientaddr_in);
								exit(0);
							default:
								close(ss_UDP);
						}
					}
				}
		} /* Fin del bucle infinito de atención a clientes */
		/* Cerramos los sockets UDP y TCP */
		close(ls_TCP);
		close(s_UDP);

		printf("\nFin de programa servidor!\n");

	default: /* Parent process comes here. */
		exit(0);
	}
}



/*
 *				S E R V E R T C P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */

void serverTCP(int s, struct sockaddr_in clientaddr_in) 
{
//------------------------------------------------------------------------------------------
// ABRIMOS FICHERO DE TEXTO
//------------------------------------------------------------------------------------------
	FILE* salida;
	salida = fopen("peticionesTCP.log", "w");
//------------------------------------------------------------------------------------------

	int reqcnt = 0;         /* keeps count of number of requests */
	char buf[TAM_BUFFER];
	char buff[TAM_BUFFER];
	char bufff[TAM_BUFFER];
	char buffff[TAM_BUFFER];
	char bufffff[TAM_BUFFER];       /* This example uses TAM_BUFFER byte messages. */
	char bufCerrando[TAM_BUFFER];
	char hostname[MAXHOST];         /* remote host's name string */

	int len;
	int len1;
	int status;

	struct hostent *hp;         /* pointer to host info for remote host */
	long timevar;                       /* contains time returned by time() */

	struct linger linger;               /* allow a lingering, graceful close; */
	/* used when setting SO_LINGER */

	char ip[16];

	status = getnameinfo((struct sockaddr*)&clientaddr_in, sizeof(clientaddr_in), hostname, MAXHOST, NULL, 0, 0);

	if (status) 
	{
		if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
			perror(" inet_ntop \n");
	}

	time(&timevar);

	struct in_addr clientIP;
	clientIP = clientaddr_in.sin_addr;
	char pd[INET_ADDRSTRLEN];

	printf("(S-TCP)Startup from %s, port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", (char*)ctime(&timevar));

	fprintf(salida, "Startup from %s port %u, IP %s, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), inet_ntop(AF_INET, &clientIP, pd, INET_ADDRSTRLEN),
		"Protocol TCP", (char*)ctime(&timevar));

	linger.l_onoff = 1;
	linger.l_linger = 1;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) 
	{
		errout(hostname);
	}

	int f = 0;
	int centinela = 0;
	char codError[TAM_BUFFER];

	fprintf(salida, "\n---- PATATA CALIENTE ---- host: %s ---- port: %u ---- IP: %d ---- %s ----\n", hostname, ntohs(clientaddr_in.sin_port), 
				clientaddr_in.sin_addr.s_addr, "Protocol TCP");

	while (f != 5 && centinela != 1) // para que si lee 5 errores seguidos no ejecute el resto
	{ 
		//----------------------------------------------------------------------------------------------
		// HOLA Y PREGUNTA
		//----------------------------------------------------------------------------------------------
		f = 0;
		int temp1;
		int intentos = rand() % (6 - 2 + 1) + 2;
		Datos juego;
		
		FILE* archPreguntas = fopen("preguntas.txt", "r");
		
		juego = readNextLine(archPreguntas);

		if (archPreguntas == NULL) 
		{
			perror("Error al abrir el archivo");
			//return 1;
		}

		while (temp1 = RecibirTramaHolaTCP(s, buf, &len, hostname) != 0) 
		{
			sprintf(codError, "500\r\n");
			f++;
			fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
				(char*)ctime(&timevar));
			fprintf(salida, "%s\t%s ERROR DE SINTAXIS\n\n\n", buf, codError);
			fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
				(char*)ctime(&timevar));
			memset(buf, 0, strlen(buf));
			if (send(s, codError, sizeof(codError), 0) == -1) 
			{
				fprintf(stdout, "(S-TCP)Error pidiendo reenvío HOLA\n");
				errout(hostname);
			}
		}

		if (temp1 == 0) 
		{
			sprintf(codError, "250\r\n");
			fprintf(salida, "\t\t  port %u, IP %d, %s, at %s", ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", (char*)ctime(&timevar));
			fprintf(salida, "%s\t%s %s#%d\n\n", buf, codError, juego.pregunta, intentos);
			fprintf(salida, "\t\t  port %u, IP %d, %s, at %s", ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", (char*)ctime(&timevar));
			memset(buf, 0, strlen(buf));
			if (send(s, codError, sizeof(codError), 0) == -1) 
			{
				fprintf(stdout, "(S-TCP)Error enviando 250 PREGUNTA#Intentos\n");
				errout(hostname);
			}
		}

		if (f == 5)
			break;
		
		//----------------------------------------------------------------------------------------------
		// RESPUESTA
		//----------------------------------------------------------------------------------------------
		f = 0;
		int temp2 = 0;
		int retorno = 0;
		bool acabar = false;
		 
		while(!acabar)
		{
			RecibirTramaRespuestaTCP(s, buff, &len, &retorno, &temp2, juego);

			if(temp2 == 2) 
			{
		            acabar = true;
				break;
		    	}
			if(temp2 != 0)
			{
				sprintf(codError, "500\r\n");
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
					(char*)ctime(&timevar));
				fprintf(salida, "%s\t%s ERROR DE SINTAXIS\n\n\n", buff, codError);
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
					(char*)ctime(&timevar));
				memset(buff, 0, strlen(buff));
				if (send(s, codError, sizeof(codError), 0) == -1) 
				{
					fprintf(stdout, "(S-TCP)Error pidiendo reenvío RESPUESTA\n");
					errout(hostname);
				}
			}

			if (temp2 == 0) 
			{
				if (retorno == 3)
				{
					intentos--;
					sprintf(codError, "354\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s MENOR#%d\n\n\n", buff, codError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					
					if (send(s, codError, sizeof(codError), 0) == -1) 
					{
						fprintf(stdout, "(S-TCP)Error enviando 354 MENOR\n");
						errout(hostname);
					}
				}
				if (retorno == 4)
				{
					intentos--;
					sprintf(codError, "354\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s MAYOR#%d\n\n\n", buff, codError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (send(s, codError, sizeof(codError), 0) == -1) 
					{
						fprintf(stdout, "(S-TCP)Error enviando 354 MAYOR\n");
						errout(hostname);
					}
				}
				if (retorno == 0)
				{
					sprintf(codError, "350\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s ACIERTO\n\n\n", buff, codError);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (send(s, codError, sizeof(codError), 0) == -1) 
					{
						fprintf(stdout, "(S-TCP)Error enviando 350 ACIERTO\n");
						errout(hostname);
					}
				}
				if (retorno == 5)
				{
					intentos = rand() % (6 - 2 + 1) + 2;
					juego = readNextLine(archPreguntas); 
					sprintf(codError, "250\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s %s#%d\n\n", buff, codError, juego.pregunta, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (send(s, codError, sizeof(codError), 0) == -1) 
					{
						fprintf(stdout, "(S-TCP)Error enviando 250 PREGUNTA#Intentos\n");
						errout(hostname);
					}
				}
				if ((retorno == 3 || retorno == 4) && intentos == 0)
				{
					sprintf(codError, "375\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s FALLO no quedan mas intentos#%d\n\n\n", buff, codError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (send(s, codError, sizeof(codError), 0) == -1) 
					{
						fprintf(stdout, "(S-TCP)Error enviando 375 FALLO\n");
						errout(hostname);
					}
				}
			}
		}

		//----------------------------------------------------------------------------------------------
		// ADIOS
		//----------------------------------------------------------------------------------------------
		f = 0;
		int temp5 = 0;

		if (temp2 == 2) 
		{
			sprintf(codError, "221\r\n");
			memset(bufffff, 0, TAM_BUFFER);
			sprintf(bufffff, "\nADIOS\n");
			fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", (char*)ctime(&timevar));
			fprintf(salida, "%s\t%s Cerrando el servicio", bufffff, codError);
			fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol TCP", (char*)ctime(&timevar));
			memset(bufffff, 0, strlen(bufffff));
			if (send(s, codError, TAM_BUFFER, 0) == -1) 
			{
				fprintf(stdout, "(S-TCP)Error enviando 221 ADIOS\n");
				errout(hostname);
			}
		}

		if (f == 5)
			break;

		centinela = 1;
	}

	if (f == 5) 
	{
		reqcnt++;
		sleep(1);

		close(s);

		/* Log a finishing message. */
		time(&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
		printf("(S-TCP)Uncompleted %s port %u, %d requests, IP %d, %s, at %s\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
			"Protocol TCP", (char*)ctime(&timevar));
		fprintf(salida, "Uncompleted %s port %u, %d requests, IP %d, %s, at %s\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
			"Protocol TCP", (char*)ctime(&timevar));
	}
	else 
	{
		reqcnt++;
		sleep(1);

		close(s);

		/* Log a finishing message. */
		time(&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
		printf("(S-TCP)Completed %s port %u, %d requests, IP %d, %s, at %s\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
			"Protocol TCP", (char*)ctime(&timevar));
		fprintf(salida, "Completed %s port %u, %d requests, IP %d, %s, at %s\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
			"Protocol TCP", (char*)ctime(&timevar));
	}

	fclose(salida);
}

void errout(char *hostname)
{
	printf("Connection with %s aborted on error\n", hostname);
	exit(1);
}

/*
 *                              S E R V E R U D P
 *
 *      This is the actual server routine that the daemon forks to
 *      handle each individual connection.  Its purpose is to receive
 *      the request packets from the remote client, process them,
 *      and return the results to the client.  It will also write some
 *      logging information to stdout.
 *
 */
void serverUDP(int s, struct sockaddr_in clientaddr_in)
{
//------------------------------------------------------------------------------------------
// ABRIMOS FICHERO DE TEXTO
//------------------------------------------------------------------------------------------
	FILE *salida;
	salida = fopen("peticionesUDP.log", "w");
//------------------------------------------------------------------------------------------
	struct in_addr reqaddr; /* for requested host's address */
	struct hostent *hp;             /* pointer to host info for requested host */
	int nc, errcode;

	struct addrinfo hints, *res;

	int addrlen;
	int reqcnt = 0;         /* keeps count of number of requests */
	char buf[TAM_BUFFER];
	char buff[TAM_BUFFER];
	char bufff[TAM_BUFFER];
	char buffff[TAM_BUFFER];
	char bufffff[TAM_BUFFER];       /* This example uses TAM_BUFFER byte messages. */
	char bufCerrando[TAM_BUFFER];
	char hostname[MAXHOST];         /* remote host's name string */
	int len, status, conexion;
	char codigoError[TAM_BUFFER];

	status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in),
						hostname,MAXHOST,NULL,0,0);

	if(status)
	{
			/* The information is unavailable for the remote
						* host.  Just format its internet address to be
						* printed out in the logging information.  The
						* address will be shown in "internet dot format".
						*/
						/* inet_ntop para interoperatividad con IPv6 */
		if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
			perror(" inet_ntop \n");
	}

	addrlen = sizeof(struct sockaddr_in);

	memset (&hints, 0, sizeof (hints));

	hints.ai_family = AF_INET;

	
	sprintf(codigoError, "500\r\n");

	nc = sendto (s, &codigoError, sizeof(codigoError), 0,
					(struct sockaddr *)&clientaddr_in, addrlen);
	if ( nc == -1) 
	{
		perror("serverUDP");
		printf("(S-UDP)%s: sendto error\n", "serverUDP");
		return;
	}
	long timevar;
	time(&timevar);
	printf("(S-UDP)Startup from %s port %u, IP %d, %s, at %s",
			hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr,
			"Protocol UDP", (char *) ctime(&timevar));
	fprintf(salida, "Startup from %s port %u, IP %d, %s, at %s",
			hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr,
			"Protocol UDP", (char *) ctime(&timevar));

	int centinela = 0;
	int f = 0;

	fprintf(salida, "\n---- PATATA CALIENTE ---- host: %s ---- port: %u ---- IP: %d ---- %s ----\n", hostname, ntohs(clientaddr_in.sin_port), 
				clientaddr_in.sin_addr.s_addr, "Protocol UDP");
	
	while (f != 5 && centinela != 1) 
	{
		//----------------------------------------------------------------------------------------------
		// HOLA
		//----------------------------------------------------------------------------------------------

		f = 0;
		int temp1;
		int intentos = rand() % (6 - 2 + 1) + 2;
		int numPatata = 1;
		Datos juego;
		bool acabarHola = false;

		
		FILE* archPreguntas = fopen("preguntas.txt", "r");
		
		juego = readNextLine(archPreguntas);

		if (archPreguntas == NULL) 
		{
			perror("Error al abrir el archivo");
			//return 1;
		}
		
		while (!acabarHola) 
		{	
			RecibirTramaHolaUDP(s, buf, &temp1, &len, hostname);
			if(temp1 == -1){
				sprintf(codigoError, "500\r\n");
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
					(char*)ctime(&timevar));
				fprintf(salida, "%s\t%s ERROR DE SINTAXIS\n\n\n", buf, codigoError);
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
					(char*)ctime(&timevar));
				memset(buf, 0, strlen(buf));
				if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
				{
					fprintf(stdout, "(S-UDP)Error pidiendo reenvío HOLA\n");
					errout(hostname);
				}
			}
			if (temp1 == 0) 
			{	
				sprintf(codigoError, "250\r\n");
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", (char *) ctime(&timevar));
				fprintf(salida, "%s\t%s %s#%d\n\n", buf, codigoError, juego.pregunta, intentos);
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", (char *) ctime(&timevar));
				memset(buf, 0, strlen(buf));
				if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
				{
						fprintf(stdout, "(S-UDP)Error enviando 250 OK HOLA\n");
						errout(hostname);
				}
				acabarHola = true;
				break;
			}
			if(temp1 == 2){
				sprintf(codigoError, "500\r\n");
				memset(buf, 0, strlen(buf));
				if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
				{
					fprintf(stdout, "(S-UDP)Error pidiendo reenvío HOLA\n");
					errout(hostname);
				}
			}
		}

		if (f == 5)
				break;

		//--------------------------------------------------------------------------------------------------
		// RESPUESTA
		//----------------------------------------------------------------------------------------------
		f = 0;
		int zentorno = 0;
		int numCorreo = 1;
		int temp2 = 0;
		int retorno = 0;
		bool acabar = false;
		 
		while(!acabar)
		{
			RecibirTramaRespuestaUDP(s, buff, &len, &retorno, &temp2, juego);

			if(temp2 == 2) 
			{
		            	acabar = true;
				break;
		    	}
			if(temp2 != 0)
			{
				sprintf(codigoError, "500\r\n");
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
					(char*)ctime(&timevar));
				fprintf(salida, "%s\t%s ERROR DE SINTAXIS\n\n\n", buff, codigoError);
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
					(char*)ctime(&timevar));
				memset(buff, 0, strlen(buff));
				if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
				{
					fprintf(stdout, "(S-UDP)Error pidiendo reenvío RESPUESTA\n");
					errout(hostname);
				}
			}

			if (temp2 == 0) 
			{
				if (retorno == 3)
				{
					intentos--;
					sprintf(codigoError, "354\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s MENOR#%d\n\n\n", buff, codigoError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					
					if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
					{
						fprintf(stdout, "(S-UDP)Error enviando 354 MENOR\n");
						errout(hostname);
					}
				}
				if (retorno == 4)
				{
					intentos--;
					sprintf(codigoError, "354\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s MAYOR#%d\n\n\n", buff, codigoError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
					{
						fprintf(stdout, "(S-UDP)Error enviando 354 MAYOR\n");
						errout(hostname);
					}
				}
				if (retorno == 0)
				{
					sprintf(codigoError, "350\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s ACIERTO\n\n\n", buff, codigoError);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
					{
						fprintf(stdout, "(S-UDP)Error enviando 350 ACIERTO\n");
						errout(hostname);
					}
				}
				if (retorno == 5)
				{
					intentos = rand() % (6 - 2 + 1) + 2;
					juego = readNextLine(archPreguntas); 
					sprintf(codigoError, "250\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s %s#%d\n\n", buff, codigoError, juego.pregunta, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
					{
						fprintf(stdout, "(S-UDP)Error enviando 250 PREGUNTA#Intentos\n");
						errout(hostname);
					}
				}
				if ((retorno == 3 || retorno == 4) && intentos == 0)
				{
					sprintf(codigoError, "375\r\n");
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					fprintf(salida, "%s\t%s FALLO no quedan mas intentos#%d\n\n\n", buff, codigoError, intentos);
					fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", 
						(char*)ctime(&timevar));
					memset(buff, 0, strlen(buff));
					if (sendto(s, &codigoError, sizeof(codigoError), 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
					{
						fprintf(stdout, "(S-UDP)Error enviando 374 FALLO\n");
						errout(hostname);
					}
				}
			}
		}

		//----------------------------------------------------------------------------------------------
		// ADIOS
		//----------------------------------------------------------------------------------------------
		f = 0;
		int temp5 = 0;

		if (temp2 == 2) 
		{
				sprintf(codigoError, "221\r\n");
				memset(bufffff, 0, TAM_BUFFER);
				sprintf(bufffff, "\nADIOS\n");
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", (char *) ctime(&timevar));
				fprintf(salida, "%s\t%s Cerrando el servicio", bufffff, codigoError);
				fprintf(salida, "\n\t\t %s port %u, IP %d, %s, at %s", hostname, ntohs(clientaddr_in.sin_port), clientaddr_in.sin_addr.s_addr, "Protocol UDP", (char *) ctime(&timevar));
				memset(bufffff, 0, strlen(bufffff));
				if (sendto(s, codigoError, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1) 
				{
						fprintf(stdout, "(S-UDP)Error enviando 221 ADIOS\n");
						errout(hostname);
				}
		}

		if (f == 5)
				break;

		centinela = 1;
	}
		
	printf("(S-UDP)Completed %s port %u, %d requests, IP %d, %s, at %s\n",
					hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
					"Protocol UDP", (char *) ctime(&timevar));
	fprintf(salida, "Completed %s port %u, %d requests, IP %d, %s, at %s\n",
					hostname, ntohs(clientaddr_in.sin_port), reqcnt, clientaddr_in.sin_addr.s_addr,
					"Protocol UDP", (char *) ctime(&timevar));
	fclose(salida);
}	
	
//----------------------------------------------------------------------------------------------
// FUNCIONES GENERALES
//----------------------------------------------------------------------------------------------

Datos readNextLine(FILE* file) 
{
	Datos result;
	char buffer[100];

    if (fgets(buffer, sizeof(buffer), file) != NULL) 
	{
        // Usar strtok para dividir la línea en campos separados por '*'
        char *token = strtok(buffer, "*");

        // Copiar el primer campo (pregunta) en result.pregunta
        if (token != NULL) 
		{
            strncpy(result.pregunta, token, sizeof(result.pregunta));
            result.pregunta[sizeof(result.pregunta) - 1] = '\0';  // Asegurar terminación nula
        } 
		else 
		{
            // Si no hay campo, poner result.pregunta en cadena vacía
            result.pregunta[0] = '\0';
        }

        // Obtener el siguiente campo (solucion) usando atoi
        token = strtok(NULL, "*");
        if (token != NULL) 
		{
            result.solucion = atoi(token);
        } 
		else 
		{
            // Si no hay campo, poner result.solucion en 0
            result.solucion = 0;
        }
    } 
	else 
	{
        // Si no se puede leer más líneas, poner result.pregunta en cadena vacía y result.solucion en 0
        result.pregunta[0] = '\0';
        result.solucion = 0;
    }

    return result;
}

int esNumero(const char *cadena) 
{
    int i = 0;

    // Si el primer caracter es un signo, se ignora
    if (cadena[0] == '+' || cadena[0] == '-') 
	{
        i++;
    }

    while (cadena[i] != '\0') 
	{
        if (!isdigit(cadena[i])) 
		{
            return 0;  // No es un número
        }
        i++;
    }

    return 1;  // Es un número
}

void imprimirConSecuenciasDeEscape(const char *cadena) {
    while (*cadena) {
        if (*cadena == '\n') {
            printf("\\n");
        } else if(*cadena == '\r') {
	   printf("\\r");
	}else{
            putchar(*cadena);
        }
        cadena++;
    }
}

int RecibirTramaAdios(int s, char* bufffff, int* len) 
{
	char adios[TAM_BUFFER];
	int len1;

	memset(bufffff, 0, strlen(bufffff));

	sprintf(adios, "ADIOS\r\n");

	*len = recv(s, bufffff, TAM_BUFFER, 0);
	if (*len == -1) 
	{
		fprintf(stdout, "(S-TCP)Error recibiendo ADIOS\n");
		while (bufffff[*len - 2] != '\r' && bufffff[*len - 1] != '\n') 
		{
			if (len1 = recv(s, &bufffff[*len], strlen(adios), 0) == -1) 
			{
				fprintf(stdout, "(S-TCP)Error recibiendo ADIOS");
			}
			else
				*len += len1;
		}
	}

	if (strcmp(adios, bufffff) == 0) 
	{
		return 0;
	}
	else 
	{
		return -1;
	}

}


//----------------------------------------------------------------------------------------------
// FUNCIONES TCP
//----------------------------------------------------------------------------------------------

int RecibirTramaHolaTCP(int s, char* buf, int* len, char* hostname) 
{
	char hola[TAM_BUFFER];
	int len1;

	memset(buf, 0, strlen(buf));

	sprintf(hola, "HOLA\r\n");

	*len = recv(s, buf, TAM_BUFFER, 0);
	if (*len == -1) 
	{
		fprintf(stdout, "(S-TCP)Error recibiendo HOLA\r\n");
		errout(hostname);
		while (buf[*len - 2] != '\r' && buf[*len - 1] != '\n') 
		{
			if (len1 = recv(s, &buf[*len], TAM_BUFFER, 0) == -1) 
			{
				fprintf(stdout, "(S-TCP)Error recibiendo HOLA");
				errout(hostname);
			}
			else
				*len += len1;
		}
	}

	if (strcmp(buf, hola) == 0)
	{
		return 0;
	}
	else
		return -1;
}

char seps[] = {' ', '\n'};

void RecibirTramaRespuestaTCP(int s, char *buff, int *len,int *retorno,int *temp2, Datos juego) 
{
	int len1;
	char *tokens;
	char buffTemp[TAM_BUFFER];
	int solucion = 0;

	memset(buff, 0, strlen(buff));

	*len = recv(s, buffTemp, TAM_BUFFER, 0);

	for (int i = 0; i < strlen(buffTemp); i++) 
	{
			if (buffTemp[i] == '\r' && buffTemp[i+1] == '\n') 
			{
				buff[i] = '\0';
				break;
			}
			buff[i] = buffTemp[i];
	}

	if (*len == -1) 
	{
			fprintf(stdout, "(S-TCP)Error recibiendo RESPUESTA\r\n");
			while (buff[*len-2] != '\r' && buff[*len-1] != '\n') 
			{
				if (len1 = recv(s, &buff[*len], TAM_BUFFER, 0) == -1) 
				{
					fprintf(stdout, "(S-TCP)Error recibiendo RESPUESTA");
				}
				else
					*len += len1;
			}
	}

	tokens = strtok(buff, seps);

	int i = 0;
    char respuesta[strlen(tokens)][TAM_BUFFER];

	while (tokens != NULL) 
	{
		sprintf(respuesta[i], "%s", tokens);
		tokens = strtok(NULL, seps);
		i++;
	}

	strcpy(buff, respuesta[0]);
    strcat(buff, " ");
    strcat(buff, respuesta[1]);
    strcat(buff, "\n");
	
	if(strcmp(buffTemp,"ADIOS\r\n")==0)
	{
		*retorno = 2;
		*temp2 = 2;
		return;
	}

	if (strcmp(buffTemp, "+\r\n") == 0)
    {
		memset(buff, 0, strlen(buff));
		strcpy(buff, "+\r\n");
		*retorno = 5;
		*temp2 = 0;
		return;
    }

	if(i != 2 || strcmp(respuesta[0], "RESPUESTA") != 0 || esNumero(respuesta[1]) != 1)
	{
		memset(buff, 0, strlen(buff));
		strcpy(buff, buffTemp);
		*temp2 = -1;
		*retorno = -1;
		return;
	}

	else 
    {
		solucion = atoi(respuesta[1]);
		
		if (solucion < juego.solucion)
		{
			*retorno = 4;
		}
		else if (solucion > juego.solucion)
		{
			*retorno = 3;
		}
		else if (solucion == juego.solucion)
		{
			*retorno = 0;
		}
		
		*temp2 = 0;
    }
}


//----------------------------------------------------------------------------------------------
// FUNCIONES UDP
//----------------------------------------------------------------------------------------------

void RecibirTramaHolaUDP(int s, char* buf, int *temp1, int *len, char* hostname)
{
	char hola[TAM_BUFFER];
	char inicio[TAM_BUFFER];
	int len1;
	memset(buf, 0, strlen(buf));
	
	sprintf(inicio, "********************************************\r\n");
	sprintf(hola, "HOLA\r\n");

	*len = recvfrom(s, buf, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
	if (*len == -1) 
	{
		fprintf(stdout, "(S-UDP)Error recibiendo HOLA\r\n");
		errout(hostname);
		while (buf[*len-2] != '\r' && buf[*len-1] != '\n') 
		{
			if (len1 = recv(s, &buf[*len], TAM_BUFFER, 0) == -1) 
			{
				fprintf(stdout, "(S-UDP)Error recibiendo HOLA");
				errout(hostname);
			}
			else
				*len += len1;
		}
	}
	if(strcmp(inicio, buf) == 0)
	{
		*temp1 = 2;
		return;
	}
	else if (strcmp(hola, buf) == 0) 
	{
		*temp1 = 0;
		return;
	} else 
	{
		*temp1 = -1;
		return;
	}
}

void RecibirTramaRespuestaUDP(int s, char *buff, int *len,int *retorno,int *temp2, Datos juego) 
{
	int len1;
	char *tokens;
	char buffTemp[TAM_BUFFER];
	int solucion = 0;

	memset(buff, 0, strlen(buff));

	*len = recvfrom(s, buffTemp, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, &addrlen);

	for (int i = 0; i < strlen(buffTemp); i++) 
	{
			if (buffTemp[i] == '\r' && buffTemp[i+1] == '\n') 
			{
				buff[i] = '\0';
				break;
			}
			buff[i] = buffTemp[i];
	}

	if (*len == -1) 
	{
			fprintf(stdout, "(S-UDP)Error recibiendo RESPUESTA\r\n");
			while (buff[*len-2] != '\r' && buff[*len-1] != '\n') 
			{
				if (len1 = recvfrom(s, buffTemp, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, &addrlen) == -1) 
				{
					fprintf(stdout, "(S-UDP)Error recibiendo RESPUESTA");
				}
				else
					*len += len1;
			}
	}

	tokens = strtok(buff, seps);

	int i = 0;
    	char respuesta[strlen(tokens)][TAM_BUFFER];

	while (tokens != NULL) 
	{
		sprintf(respuesta[i], "%s", tokens);
		tokens = strtok(NULL, seps);
		i++;
	}

	strcpy(buff, respuesta[0]);
    	strcat(buff, " ");
    	strcat(buff, respuesta[1]);
    	strcat(buff, "\n");
	
	if(strcmp(buffTemp,"ADIOS\r\n")==0)
	{
		*retorno = 2;
		*temp2 = 2;
		return;
	}

	if (strcmp(buffTemp, "+\r\n") == 0)
    	{
		memset(buff, 0, strlen(buff));
		strcpy(buff, "+\r\n");
		*retorno = 5;
		*temp2 = 0;
		return;
    	}

	if(i != 2 || strcmp(respuesta[0], "RESPUESTA") != 0 || esNumero(respuesta[1]) != 1)
	{
		memset(buff, 0, strlen(buff));
		strcpy(buff, buffTemp);
		*temp2 = -1;
		*retorno = -1;
		return;
	}

	else 
    	{
		solucion = atoi(respuesta[1]);
		
		if (solucion < juego.solucion)
		{
			*retorno = 4;
		}
		else if (solucion > juego.solucion)
		{
			*retorno = 3;
		}
		else if (solucion == juego.solucion)
		{
			*retorno = 0;
		}
		
		*temp2 = 0;
    	}
}

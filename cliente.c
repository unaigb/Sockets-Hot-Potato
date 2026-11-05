/*
** Fichero: cliente.c
** Autores:
** Unai García Blanco
** Jorge Caballero González
*/

/*
 *          		C L I E N T E
 */
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

extern int errno;

#define ADDRNOTFOUND    0xffffffff      /* value returned for unknown host */
#define RETRIES 5               /* number of times to retry before givin up */
#define BUFFERSIZE      1024    /* maximum size of packets to be received */
#define PUERTO 1921
#define TIMEOUT 6
#define MAXHOST 512
#define TAM_BUFFER 512

/*
 *                      H A N D L E R
 *
 *      This routine is the signal handler for the alarm signal.
 */
void handler()
{
	printf("Alarma recibida \n");
}


FILE* salida;
//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES TCP
//------------------------------------------------------------------------
int EnvioHolaTCP(int s, char* bufHola, char* argv[], char* hostname);
int EnvioRespuestaTCP(int s, char* bufRespuesta);
int EnvioAdiosTCP(int s, char* bufSalida);

//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES UDP
//------------------------------------------------------------------------
int EnvioHolaUDP(int s, char* bufHola, char* argv[], char* hostname);
int EnvioRespuestaUDP(int s, char *bufRespuesta);
int EnvioAdiosUDP(int s, char* bufSalida);


//------------------------------------------------------------------------
// PROTOTIPOS DE FUNCIONES CLIENTE
//------------------------------------------------------------------------
void ClienteTCP(int argc, char* argv[], FILE* entrada);
void ClienteUDP(int argc, char* argv[], FILE* entrada);



/*
 *                      M A I N
 *
 *      This routine is the client which request service from the remote.
 *      It creates a connection, sends a number of
 *      requests, shuts down the connection in one direction to signal the
 *      server about the end of data, and then receives all of the responses.
 *      Status will be written to stdout.
 *
 *      The name of the system to which the requests will be sent is given
 *      as a parameter to the command.
 */
int main(argc, argv)
int argc;
char* argv[];
{
        //------------------------------------------------------------------------------------------
        // ABRIMOS FICHERO DE TEXTO
        //------------------------------------------------------------------------------------------
        FILE* entrada;
        entrada = fopen(argv[3], "r");
        //------------------------------------------------------------------------------------------

        if (argc != 4) 
        {
        fprintf(stderr, "(C)Usage:  %s <remote host>\n", argv[0]);
        exit(1);
        }

        if (strcmp(argv[2], "TCP") == 0)
        ClienteTCP(argc, argv, entrada);
        else if (strcmp(argv[2], "UDP") == 0)
        ClienteUDP(argc, argv, entrada);
        else
        fprintf(stdout, "(C)Protocolo mal introducido (TCP / UDP)\n\n");

        fclose(entrada);

        return 0;
}

//------------------------------------------------------------------------------------------
// CLIENTE PROTOCOLO TCP
//------------------------------------------------------------------------------------------
void ClienteTCP(int argc, char* argv[], FILE* entrada) 
{
        int s;                                                      /* connected socket descriptor */
        struct addrinfo hints, * res;
        long timevar;                                       /* contains time returned by time() */
        struct sockaddr_in myaddr_in;       /* for local socket address */
        struct sockaddr_in servaddr_in;     /* for server socket address */
        int addrlen, i, j, errcode;
        /* This example uses TAM_BUFFER byte messages. */
        char buf1[TAM_BUFFER];
        char buf2[TAM_BUFFER];
        char buf3[TAM_BUFFER];
        char buf4[TAM_BUFFER];
        char buf5[TAM_BUFFER];

        //Genera el soque con flujo stream t to las cosas
        /* Create the socket. */
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-TCP)%s: unable to create socket\n", argv[0]);
                exit(1);
        }

        /* clear out address structures */
        memset((char*)&myaddr_in, 0, sizeof(struct sockaddr_in));  // no entender mucho muy
        // reserva memoria pa los struct
        memset((char*)&servaddr_in, 0, sizeof(struct sockaddr_in));

        /* Set up the peer address to which we will connect. */// rellena el struct del servidor con la direccion a la que nos queremos conectar
        servaddr_in.sin_family = AF_INET;

        /* Get the host information for the hostname that the
        * user passed in. */
        memset(&hints, 0, sizeof(hints));// mete en el struct la direccion
        hints.ai_family = AF_INET;
        /* esta función es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
        errcode = getaddrinfo(argv[1], NULL, &hints, &res);
        if (errcode != 0) 
        {
                /* Name was not found.  Return a
                        * special value signifying the error. */
                fprintf(stderr, "(C-TCP)%s: No es posible resolver la IP de %s\n",
                        argv[0], argv[1]);
                exit(1);
        }
        else 
        {
                /* Copy address of host */
                servaddr_in.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
        }
        freeaddrinfo(res);

        /* puerto del servidor en orden de red*/
        servaddr_in.sin_port = htons(PUERTO);

        /* Try to connect to the remote server at the address
        * which was just built into peeraddr.
        */
        if (connect(s, (const struct sockaddr*)&servaddr_in, sizeof(struct sockaddr_in)) == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-TCP)%s: unable to connect to remote\n", argv[0]);
                exit(1);
        }
        /* Since the connect call assigns a free address
        * to the local end of this connection, let's use
        * getsockname to see what it assigned.  Note that
        * addrlen needs to be passed in as a pointer,
        * because getsockname returns the actual length
        * of the address.
        */
        addrlen = sizeof(struct sockaddr_in);
        if (getsockname(s, (struct sockaddr*)&myaddr_in, &addrlen) == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-TCP)%s: unable to read socket address\n", argv[0]);
                exit(1);
        }

        /* Print out a startup message for the user. */
        time(&timevar);
        /* The port number must be converted first to host byte
        * order before printing.  On most hosts, this is not
        * necessary, but the ntohs() call is included here so
        * that this program could easily be ported to a host
        * that does require it.
        */
        printf("(C-TCP)Connected to %s on port %u at %s",
        argv[1], ntohs(myaddr_in.sin_port), (char*)ctime(&timevar));
        //----------------------------------------------------------------------------------
        char puertito[TAM_BUFFER];
        sprintf(puertito, "%d", ntohs(myaddr_in.sin_port));
        strcat(puertito, "TCP.txt");
        salida = fopen(puertito, "w");

        int f;
        int centinela = 0;
        int salirse = 0;
        while (f != 5 && centinela != 1) 
        {
                //----------------------------------------------------------------------------
                // HOLA
                //----------------------------------------------------------------------------

                int resp;
                f = 0;
                int h;
                char temp1[TAM_BUFFER];

                memset(buf1, 0, TAM_BUFFER);

                fgets(temp1, TAM_BUFFER, entrada);
                for (h = 0; h < strlen(temp1) - 1; h++) 
                {
                        buf1[h] = temp1[h];
                }
                strcat(buf1, "\r\n");

                resp = EnvioHolaTCP(s, buf1, argv, argv[1]);


                for (f = 0; f < 5; f++) 
                {
                        if (resp != 250) 
                        {
                        memset(buf1, 0, strlen(buf1));
                        memset(temp1, 0, strlen(temp1));
                        fgets(temp1, TAM_BUFFER, entrada);
                        for (h = 0; h < strlen(temp1) - 1; h++) 
                        {
                                buf1[h] = temp1[h];
                        }
                        strcat(buf1, "\r\n");
                        resp = EnvioHolaTCP(s, buf1, argv, argv[1]);
                        }
                        else
                        break;
                }

                memset(buf1, 0, strlen(buf1));
                memset(temp1, 0, strlen(temp1));

                if (f == 5)
                        break;

                char bufAdios[TAM_BUFFER] = "+++";
                //----------------------------------------------------------------------------
                // RESPUESTA
                //----------------------------------------------------------------------------

                bool fallar = false;
                while(!fallar)
                {
                        char temp2[TAM_BUFFER];
                        f = 0;
                        resp = 0;

                        if(strcmp(bufAdios, "+++") == 0)
                        {
                                memset(buf2, 0, TAM_BUFFER);

                                fgets(temp2, TAM_BUFFER, entrada);
                                for (h = 0; h < strlen(temp2) - 1; h++) 
                                {
                                        buf2[h] = temp2[h];
                                }
                                strcat(buf2, "\r\n");
                        }
                        else 
                        {
                                strcpy(buf2, bufAdios);
                                strcat(buf2, "\r\n");
                                memset(bufAdios, 0, TAM_BUFFER);
                        }
                        if(strcmp(buf2, "ADIOS\r\n") == 0)
                        {		
                                fallar = true;
                                break;
                        }
                        resp = EnvioRespuestaTCP(s, buf2);
                }

                char tempAdios[TAM_BUFFER];
                memset(tempAdios, 0, TAM_BUFFER);
                strcpy(tempAdios, buf2);
                if(strcmp(tempAdios, "ADIOS\r\n") == 0)
                {
                        memset(buf5, 0, TAM_BUFFER);
                        strcpy(buf5, "ADIOS");
                }

                //----------------------------------------------------------------------------
                // SALIDA // Ctrl+Shift+,
                //----------------------------------------------------------------------------
                f = 0;
                resp = 0;
                char temp5[TAM_BUFFER];
                strcat(buf5, "\r\n");

                resp = EnvioAdiosTCP(s, buf5);

                if (resp != 221) 
                {
                        memset(buf5, 0, strlen(buf5));
                        memset(temp5, 0, strlen(buf5));
                        fgets(temp5, TAM_BUFFER, entrada);
                        for (h = 0; h < strlen(temp5) - 1; h++) 
                        {
                        buf5[h] = temp5[h];
                        }
                        strcat(buf5, "\r\n");
                        resp = EnvioAdiosTCP(s, buf5);
                }
                else
                        break;

                memset(buf5, 0, strlen(buf5));
                memset(temp5, 0, strlen(buf5));

                if (f == 5)
                        break;

                centinela = 1;
        }

        if (shutdown(s, 1) == -1) 
        {
                perror(argv[0]);
                exit(1);
        }

        if (f == 5) 
        {
                time(&timevar);
                printf("(C-TCP)Program aborted at %s", (char*)ctime(&timevar));
        }
        else 
        {
                time(&timevar);
                printf("(C-TCP)All done at %s", (char*)ctime(&timevar));
        }

        fclose(salida);
}



//------------------------------------------------------------------------------------------
// FUNCIONES TCP
//------------------------------------------------------------------------------------------
int EnvioHolaTCP(int s, char* bufHola, char* argv[], char* hostname) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];

        if (send(s, bufHola, TAM_BUFFER, 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al enviar HOLA\n");
                exit(-1);
        }


        if (recv(s, buffTemp, sizeof(buffTemp), 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al recibir respuesta HOLA\n");
                exit(-1);
        }
        fprintf(salida, "\n Mando %s Recibo: %s\n", bufHola, buffTemp);

        if (strcmp(buffTemp, "250\r\n") == 0)
                codigoError = 250;
        else
                codigoError = 500;

        return codigoError;
}

int EnvioRespuestaTCP(int s, char *bufRespuesta) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];

        if (send(s, bufRespuesta, TAM_BUFFER, 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al enviar RESPUESTA\n");
                exit(-1);
        }

        if (recv(s, buffTemp, TAM_BUFFER, 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al recibir respuesta RESPUESTA\n");
                exit(-1);
        }
        fprintf(salida,"\n Mando %s Recibo: %s\n", bufRespuesta, buffTemp);

        if (strcmp(buffTemp, "250\r\n") == 0)
                codigoError = 250;
        else if (strcmp(buffTemp, "354\r\n") == 0)
                codigoError = 354;
        else if (strcmp(buffTemp, "350\r\n") == 0)
                codigoError = 350;
        else if (strcmp(buffTemp, "375\r\n") == 0)
                codigoError = 375;
        else if (strcmp(buffTemp, "221\r\n") == 0)
                codigoError = 221;
        else
                codigoError = 500;

        return codigoError;
}

int EnvioAdiosTCP(int s, char* bufSalida) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];

        if (send(s, bufSalida, TAM_BUFFER, 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al enviar ADIOS\n");
                exit(-1);
        }

        memset(buffTemp, 0, TAM_BUFFER);
        if (recv(s, buffTemp, TAM_BUFFER, 0) == -1) 
        {
                fprintf(stdout, "(C-TCP)Error al recibir respuesta ADIOS");
                exit(-1);
        }
        fprintf(salida, "\n Mando %s Recibo: %s\n", bufSalida, buffTemp);

        if (strcmp(buffTemp, "221\r\n") == 0)
                codigoError = 221;
        else
                codigoError = 500;

        return codigoError;
}



//------------------------------------------------------------------------------------------
// CLIENTE PROTOCOLO UDP
//------------------------------------------------------------------------------------------
int i, errcode;
int retry = RETRIES;            /* holds the retry count */
int s;                          /* socket descriptor */
long timevar;                       /* contains time returned by time() */
struct sockaddr_in myaddr_in;   /* for local socket address */
struct sockaddr_in servaddr_in; /* for server socket address */
struct in_addr reqaddr;         /* for returned internet address */
int     addrlen, n_retry;
struct sigaction vec;
char hostname[MAXHOST];
struct addrinfo hints, *res;

void ClienteUDP(int argc, char *argv[], FILE *entrada) 
{
        /* Create the socket. */
        s = socket (AF_INET, SOCK_DGRAM, 0);
        if (s == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-UDP)%s: unable to create socket\n", argv[0]);
                exit(1);
        }

        /* clear out address structures */
        memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
        memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

                        /* Bind socket to some local address so that the
                 * server can send the reply back.  A port number
                 * of zero will be used so that the system will
                 * assign any available port number.  An address
                 * of INADDR_ANY will be used so we do not have to
                 * look up the internet address of the local host.
                 */
        myaddr_in.sin_family = AF_INET;
        myaddr_in.sin_port = 0;
        myaddr_in.sin_addr.s_addr = INADDR_ANY;
        if (bind(s, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-UDP)%s: unable to bind socket\n", argv[0]);
                exit(1);
        }
        addrlen = sizeof(struct sockaddr_in);
        if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-UDP)%s: unable to read socket address\n", argv[0]);
                exit(1);
        }

            /* Print out a startup message for the user. */
        time(&timevar);
            /* The port number must be converted first to host byte
             * order before printing.  On most hosts, this is not
             * necessary, but the ntohs() call is included here so
             * that this program could easily be ported to a host
             * that does require it.
             */
        printf("(C-UDP)Connected to %s on port %u at %s", argv[1], ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

        /* Set up the server address. */
        servaddr_in.sin_family = AF_INET;
                /* Get the host information for the server's hostname that the
                 * user passed in.
                 */
        memset (&hints, 0, sizeof (hints));
        hints.ai_family = AF_INET;
         /* esta función es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
        errcode = getaddrinfo (argv[1], NULL, &hints, &res);
        if (errcode != 0)
        {
                        /* Name was not found.  Return a
                         * special value signifying the error. */
                fprintf(stderr, "(C-UDP)%s: No es posible resolver la IP de %s\n", argv[0], argv[1]);
                exit(1);
        }
        else 
        {
                        /* Copy address of host */
                servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
        }
        freeaddrinfo(res);
        /* puerto del servidor en orden de red*/
        servaddr_in.sin_port = htons(PUERTO);

        /* Registrar SIGALRM para no quedar bloqueados en los recvfrom */
        vec.sa_handler = (void *) handler;
        vec.sa_flags = 0;
        if ( sigaction(SIGALRM, &vec, (struct sigaction *) 0) == -1) 
        {
                perror(" sigaction(SIGALRM)");
                fprintf(stderr,"(C-UDP)%s: unable to register the SIGALRM signal\n", argv[0]);
                exit(1);
        }

        n_retry=RETRIES;

//------------------------------------------------------------------------------------
//COMENZAMOS LA COMUNICACION
//------------------------------------------------------------------------------------
        char lineaInicio[TAM_BUFFER] = "********************************************\r\n";
        char buf1[TAM_BUFFER];
        char buf2[TAM_BUFFER];
        char buf3[TAM_BUFFER];
        char buf4[TAM_BUFFER];
        char buf5[TAM_BUFFER];
        int codError;

        while (n_retry > 0) 
        {
                /* Send the request to the nameserver. */
                if (sendto (s, lineaInicio, strlen(lineaInicio), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) 
                {
                        perror(argv[0]);
                        fprintf(stderr, "(C-UDP)%s: unable to send request\n", argv[0]);
                        exit(1);
                }

                /* Set up a timeout so I don't hang in case the packet
                * gets lost.  After all, UDP does not guarantee
                * delivery.
                */
                alarm(TIMEOUT);
                /* Wait for the reply to come in. */
                if (recvfrom (s, &codError, sizeof(struct in_addr), 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) 
                {
                        if (errno == EINTR) 
                        {
                                printf("(C-UDP)attempt %d (retries %d).\n", n_retry, RETRIES);
                                n_retry--;
                        }
                        else  
                        {
                                printf("(C-UDP)Unable to get response from");
                                exit(1);
                        }
                }

                if (sendto (s, lineaInicio, strlen(lineaInicio), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) 
                {
                        perror(argv[0]);
                        fprintf(stderr, "(C-UDP)%s: unable to send request\n", argv[0]);
                        exit(1);
                }

                alarm(TIMEOUT);

                if (recvfrom (s, &codError, sizeof(struct in_addr), 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) 
                {
                        if (errno == EINTR) 
                        {
                                printf("(C-UDP)attempt %d (retries %d).\n", n_retry, RETRIES);
                                n_retry--;
                        }
                        else  
                        {
                                printf("(C-UDP)Unable to get response from");
                                exit(1);
                        }
                }
                else 
                {
                        break;
                }
        }

	char puertito[TAM_BUFFER];
        sprintf(puertito,"%d",ntohs(myaddr_in.sin_port));
        strcat(puertito,"UDP.txt"); 
        salida=fopen(puertito,"w");
	
        int h;
        int f = 0;
        int centinela = 0;
        int macarena = 0;
        while (f != 5 && centinela != 1) 
        {
		
                // --------------------------------------------------
                // HOLA
                // --------------------------------------------------
                char temp1[TAM_BUFFER];
                int resp;

                memset(buf1, 0, TAM_BUFFER);

                fgets(temp1, TAM_BUFFER, entrada);
                for (h = 0; h < strlen(temp1)-1; h++) 
                {
                        buf1[h] = temp1[h];
                }
                strcat(buf1, "\r\n");

                resp = EnvioHolaUDP(s, buf1, argv, argv[1]);
		
                for (f = 0; f < 5; f++) 
                {
                        if (resp != 250) 
                        {
                                memset(buf1, 0, strlen(buf1));
                                memset(temp1, 0, strlen(temp1));
                                fgets(temp1, TAM_BUFFER, entrada);
                                for (h = 0; h < strlen(temp1)-1; h++) {
                                                buf1[h] = temp1[h];
                                }
                                strcat(buf1, "\r\n");
                                resp = EnvioHolaUDP(s, buf1, argv, argv[1]);
                        }
                        else
                                break;
                }

                memset(buf1, 0, strlen(buf1));
                memset(temp1, 0, strlen(temp1));

                if (f == 5)
                        break;
		
                char bufAdios[TAM_BUFFER] = "+++";
		bool fallar = false;
		
                while(!fallar)
                {
                        char temp2[TAM_BUFFER];
                        f = 0;
                        resp = 0;

                        if(strcmp(bufAdios, "+++") == 0)
                        {
                                memset(buf2, 0, TAM_BUFFER);

                                fgets(temp2, TAM_BUFFER, entrada);
                                for (h = 0; h < strlen(temp2) - 1; h++) 
                                {
                                        buf2[h] = temp2[h];
                                }
                                strcat(buf2, "\r\n");
                        }
                        else 
                        {
                                strcpy(buf2, bufAdios);
                                strcat(buf2, "\r\n");
                                memset(bufAdios, 0, TAM_BUFFER);
                        }
                        if(strcmp(buf2, "ADIOS\r\n") == 0)
                        {		
                                fallar = true;
                                break;
                        }
                        resp = EnvioRespuestaUDP(s, buf2);
                }

                char tempAdios[TAM_BUFFER];
                memset(tempAdios, 0, TAM_BUFFER);
                strcpy(tempAdios, buf2);
                if(strcmp(tempAdios, "ADIOS\r\n") == 0)
                {
                        memset(buf5, 0, TAM_BUFFER);
                        strcpy(buf5, "ADIOS");
                }

                if (f == 5)
                        break;

                //----------------------------------------------------------------------------
                // SALIDA // Ctrl+Shift+,
                //----------------------------------------------------------------------------
                f = 0;
                resp = 0;
                char temp5[TAM_BUFFER];
                strcat(buf5, "\r\n");
                resp = EnvioAdiosUDP(s, buf5);

                if (resp != 221) 
                {
                        memset(buf5, 0, strlen(buf5));
                        memset(temp5, 0, strlen(buf5));
                        fgets(temp5, TAM_BUFFER, entrada);
                        for (h = 0; h < strlen(temp5)-1; h++) 
                        {
                                        buf5[h] = temp5[h];
                        }
                        strcat(buf5, "\r\n");
                        resp = EnvioAdiosUDP(s, buf5);
                }
                else
                        break;

                memset(buf5, 0, strlen(buf5));
                memset(temp5, 0, strlen(buf5));

                if (f == 5)
                        break;

                centinela = 1;
        }


        if (n_retry == 0) 
        {
                printf("(C-UDP)Unable to get response from");
                printf("(C-UDP) %s after %d attempts.\n", argv[1], RETRIES);
        }
        time(&timevar);
        printf("(C-UDP)All done at %s", (char *)ctime(&timevar));
        fclose(salida);
}



//------------------------------------------------------------------------------------------
// FUCIONES UDP
//------------------------------------------------------------------------------------------

int EnvioHolaUDP(int s, char * bufHola, char* argv[], char* hostname) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];
        if (sendto (s, bufHola, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) 
        {
                perror(argv[0]);
                fprintf(stderr, "(C-UDP)Error sendto envioHolaUDP\n");
                exit(1);
        }


        if (recvfrom (s, &buffTemp, sizeof(buffTemp), 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) 
        {
                if (errno == EINTR) 
                {
                                /* Alarm went off and aborted the receive.
                                 * Need to retry the request if we have
                                 * not already exceeded the retry limit.
                                 */
                        printf("(C-UDP)attempt %d (retries %d).\n", n_retry, RETRIES);
                        n_retry--;
                }
                else  
                {
                        printf("(C-UDP)Unable to get response from");
                        exit(1);
                }
        }

	fprintf(salida,"\n Mando %s Recibo: %s\n", bufHola, buffTemp);	

        if (strcmp(buffTemp, "250\r\n") == 0)
                codigoError = 250;
        else
                codigoError = 500;

        return codigoError;
}

int EnvioRespuestaUDP(int s, char *bufRespuesta) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];
	if (sendto(s, bufRespuesta, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
                fprintf(stdout, "(C-UDP)Error al enviar RESPUESTA\n");
                exit(-1);
        }

	if (recvfrom (s, buffTemp, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) {
                if (errno == EINTR) {
                                /* Alarm went off and aborted the receive.
                                 * Need to retry the request if we have
                                 * not already exceeded the retry limit.
                                 */
                 	printf("(C-UDP)attempt %d (retries %d).\n", n_retry, RETRIES);
                	n_retry--;
		}
        	else  {
                        printf("(C-UDP)Unable to get response from");
                        exit(1);
        	}
        }

        fprintf(salida,"\n Mando %s Recibo: %s\n", bufRespuesta, buffTemp);

        if (strcmp(buffTemp, "250\r\n") == 0)
                codigoError = 250;
        else if (strcmp(buffTemp, "354\r\n") == 0)
                codigoError = 354;
        else if (strcmp(buffTemp, "350\r\n") == 0)
                codigoError = 350;
        else if (strcmp(buffTemp, "375\r\n") == 0)
                codigoError = 375;
        else if (strcmp(buffTemp, "221\r\n") == 0)
                codigoError = 221;
        else
                codigoError = 500;

        return codigoError;
}

int EnvioAdiosUDP(int s, char *bufSalida) 
{
        int codigoError;
        char buffTemp[TAM_BUFFER];

        if (sendto(s, bufSalida, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) 
        {
                fprintf(stdout, "(C-UDP)Error al enviar ADIOS\n");
                exit(-1);
        }

        memset(buffTemp, 0, TAM_BUFFER);
        if (recvfrom (s, buffTemp, TAM_BUFFER, 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) 
        {
                if (errno == EINTR) 
                {
                                /* Alarm went off and aborted the receive.
                                 * Need to retry the request if we have
                                 * not already exceeded the retry limit.
                                 */
                        printf("(C-UDP)attempt %d (retries %d).\n", n_retry, RETRIES);
                        n_retry--;
                }
                else  
                {
                        printf("(C-UDP)Unable to get response from");
                        exit(1);
                }
        }

        fprintf(salida,"\n Mando %s Recibo: %s\n", bufSalida, buffTemp);

        if (strcmp(buffTemp, "221\r\n") == 0)
                codigoError = 221;
        else
                codigoError = 500;

        return codigoError;
}

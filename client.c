// https://stackoverflow.com/questions/11208299/how-to-make-an-http-get-request-in-c-without-libcurl/35680609#35680609
/*
Paul Crocker
Muitas Modificações
*/
// Definir sta linha com 1 ou com 0 se não quiser ver as linhas com debug info.
#define DEBUG 0

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "csapp.h"
struct protoent *protoent;

struct hostent *hostent;
char *hostname = "localhost";
unsigned short server_port = 8080; // default port
char *file;
char *fileindex = "/";
char *filedynamic = "/cgi-bin/adder?150&100";
char *filestatic = "/godzilla.jpg";

char buffer[BUFSIZ];
enum CONSTEXPR
{
    MAX_REQUEST_LEN = 1024
};
char request[MAX_REQUEST_LEN];
char request_template[] = "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n";

in_addr_t in_addr;
int request_len;
int socket_file_descriptor;
ssize_t nbytes;

struct sockaddr_in sockaddr_in;

void *httpProtocol(void *arg);

pthread_barrier_t barrier;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int num_threads = 0;
char *schedalg;
int main(int argc, char **argv)
{

    if (argc < 6)
    {
        num_threads = atoi(argv[3]);
        schedalg = argv[4];
        printf("./client [host] [portnum] [threads] [schedalg] [filename1] [filename2]* OPCIONAL\n");
        exit(1);
    }
    if (argc == 6)
    {
        hostname = argv[1];
        server_port = strtoul(argv[2], NULL, 10);
        num_threads = atoi(argv[3]);
        schedalg = argv[4];
        file = argv[5];
        // print das variaveis anteriors para ver se esta a funcionar
        if (DEBUG)
        {
            printf("hostname: %s\n", hostname);
            printf("server_port: %d\n", server_port);
            printf("num_threads: %d\n", num_threads);
            printf("schedalg: %s\n", schedalg);
            printf("file: %s\n", file);
        }
        // ./client localhost 8080 1 FIFO /
    }
    else
    {
        file = fileindex;
    }

    // ESCOLHER ENTRE DOIS FICHEIROS :::
    /* the name of a second file that the client is requesting from the server. This
argument is optional. If it does not exist, then the client should repeatedly ask for only the
first file. If it does exist, then each thread of the client should alternate which file it is
requesting.*/
    // ou escolher outra filedynamic filestatic

    request_len = snprintf(request, MAX_REQUEST_LEN, request_template, file, hostname);
    if (request_len >= MAX_REQUEST_LEN)
    {
        fprintf(stderr, "request length large: %d\n", request_len);
        exit(EXIT_FAILURE);
    }

    pthread_t tid[num_threads];
    pthread_barrier_init(&barrier, NULL, 1);
    while (1)
    {

        // create the threads

        for (int i = 0; i < num_threads; i++)
        {
            pthread_create(&tid[i], NULL, httpProtocol, NULL);
        }
        // join the threads
        for (int i = 0; i < num_threads; i++)
        {
            pthread_barrier_wait(&barrier);
            pthread_join(tid[i], NULL);
        }
        // For Loop n times pthread_wait thread
        // sleep for 1 second
        sleep(1);
        // httpProtocol();
    }
    exit(EXIT_SUCCESS);
}

void *httpProtocol(void *arg)
{
    pthread_mutex_lock(&mutex);
    // construção do pedido de http
    /* Build the socket. */
    protoent = getprotobyname("tcp");
    if (protoent == NULL)
    {
        perror("getprotobyname");
        exit(EXIT_FAILURE);
    }

    // Open the socket
    socket_file_descriptor = Socket(AF_INET, SOCK_STREAM, protoent->p_proto);

    /* Build the address. */
    // 1 get the hostname address
    hostent = Gethostbyname(hostname);

    in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1)
    {
        fprintf(stderr, "error: inet_addr(\"%s\")\n", *(hostent->h_addr_list));
        exit(EXIT_FAILURE);
    }
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(server_port);

    /* Ligar ao servidor */
    Connect(socket_file_descriptor, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));

    /* Send HTTP request. */

    Rio_writen(socket_file_descriptor, request, request_len);

    /* Read the response. */
    if (DEBUG)
        fprintf(stderr, "debug: before first read\n");

    rio_t rio;
    char buf[MAXLINE];

    /* Leituras das linhas da resposta . Os cabecalhos - Headers */
    const int numeroDeHeaders = 5;
    Rio_readinitb(&rio, socket_file_descriptor);
    for (int k = 0; k < numeroDeHeaders; k++)
    {
        Rio_readlineb(&rio, buf, MAXLINE);

        // Envio das estatisticas para o canal de standard error
        if (strstr(buf, "Stat") != NULL)
            fprintf(stderr, "STATISTIC : %s", buf);
    }

    // Ler o resto da resposta - o corpo de resposta.
    // Vamos ler em blocos caso que seja uma resposta grande.
    while ((nbytes = Rio_readn(socket_file_descriptor, buffer, BUFSIZ)) > 0)
    {
        if (DEBUG)
            fprintf(stderr, "debug: after a block read\n");
        // commentar a lina seguinte se não quiser ver o output
        Rio_writen(STDOUT_FILENO, buffer, nbytes);
    }

    if (DEBUG)
        fprintf(stderr, "debug: after last read\n");

    Close(socket_file_descriptor);
    pthread_mutex_unlock(&mutex);
    return NULL;
}
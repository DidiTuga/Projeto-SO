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

// ficheiros
char *file;
char *file_s;
char *file_p;
char *filedynamic = "/cgi-bin/adder?150&100";
char *filestatic = "/home.html";

char buffer[BUFSIZ];
enum CONSTEXPR
{
    MAX_REQUEST_LEN = 1024
};
char request[MAX_REQUEST_LEN];
char request_template[] = "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n";

void *fifo_funcao(void *arg);
void *httpProtocol(void *arg);

pthread_mutex_t mutex;

int num_threads;
int flag = 0;
// criar array de semaforos
sem_t *semaforos;
char *schedalg;
int main(int argc, char **argv)
{

    if (argc < 6 || argc > 7)
    {
        fprintf(stderr, "Usage: %s [host] [portnum] [threads] [schedalg] [filename1] [filename2]* OPCIONAL\n", argv[0]);
        exit(1);
    }
    else if (argc == 6)
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
        hostname = argv[1];
        server_port = strtoul(argv[2], NULL, 10);
        num_threads = atoi(argv[3]);
        schedalg = argv[4];
        file_p = argv[5];
        file_s = argv[6];
        // print das variaveis anteriors para ver se esta a funcionar
        if (DEBUG)
        {
            printf("hostname: %s\n", hostname);
            printf("server_port: %d\n", server_port);
            printf("num_threads: %d\n", num_threads);
            printf("schedalg: %s\n", schedalg);
            printf("file_p: %s\n", file_p);
            printf("file_s: %s\n", file_s);
        }
    }

    pthread_t tid[num_threads];

    //  verificar se o schedalg é FIFO ou RR
    if (strcmp(schedalg, "FIFO") == 0)
    {
        flag = 1;
        semaforos = malloc(num_threads * sizeof(sem_t));
        // criar primeiro as threads
        sem_init(&semaforos[0], 0, 1);
        for (int i = 1; i < num_threads; i++)
        {
            sem_init(&semaforos[i], 0, 0);
        }
        for (int i = 0; i < num_threads; i++)
        {
            pthread_create(&tid[i], NULL, fifo_funcao, &i);
        }
        // inicializar as semaforos

        // esperar as threads terminarem
        for (int i = 0; i < num_threads; i++)
        {
            pthread_join(tid[i], NULL);
        }
        while (1)
        {
        }
    }
    else if (strcmp(schedalg, "CONCUR") == 0)
    {
        while (1)
        {
            for (int i = 0; i < num_threads; i++)
            {
                pthread_create(&tid[i], NULL, httpProtocol, NULL);
            }
            for (int i = 0; i < num_threads; i++)
            {
                pthread_join(tid[i], NULL);
            }
            sleep(1);
        }
    }
    else
    {
        printf("o escalonamento só pode ser CONCUR ou FIFO\n");
        exit(1);
    }

    exit(EXIT_SUCCESS);
}

// funcao para o FIFO client
void *fifo_funcao(void *arg)
{

    int id = *(int *)arg;
    sem_wait(&semaforos[id]);
    httpProtocol(NULL);
    if (id == num_threads)
    {
        sem_post(&semaforos[0]);
    }
    else
    {
        sem_post(&semaforos[id + 1]);
    }

    return NULL;
}

void *httpProtocol(void *arg)
{
    // flag usada pois se flag for 1 é FIFO ou seja nao usa os mutex
    // mutex lock
    if (flag == 0)
    {
        pthread_mutex_lock(&mutex);
    }
    // se o file_s for NULL é porque é existem dois ficheiros e depois crio um rand para gerar um numero aleatorio
    // para escolher o ficheiro ou estatico ou dinamico
    if (file_s != NULL)
    {
        int random = rand() % 2;
        if (random == 0)
        {
            file = filedynamic;
        }
        else
        {
            file = filestatic;
        }
    }
    in_addr_t in_addr;
    int request_len;
    int socket_file_descriptor;
    ssize_t nbytes;
    struct sockaddr_in sockaddr_in;
    request_len = snprintf(request, MAX_REQUEST_LEN, request_template, file, hostname);
    if (request_len >= MAX_REQUEST_LEN)
    {
        fprintf(stderr, "request length large: %d\n", request_len);
        exit(EXIT_FAILURE);
    }
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
    if (flag == 0)
    {
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
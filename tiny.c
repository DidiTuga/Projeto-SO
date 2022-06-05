/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.

 Fom csapp
 Modified by Paul

 */
#include "csapp.h"

queue *head = NULL;
queue *tail = NULL;
queue *h_dynamic = NULL;
queue *t_dynamic = NULL;

void enqueue(int *fd)
{
  queue *novo = malloc(sizeof(queue));
  novo->socket = fd;
  novo->Prox = NULL;
  if (head == NULL)
  {
    head = novo;
  }
  else
  {

    tail->Prox = novo;
  }
  tail = novo;
}
void enqueue_dynamic(int *fd)
{
  queue *novo = malloc(sizeof(queue));
  novo->socket = fd;
  novo->Prox = NULL;
  if (h_dynamic == NULL)
  {
    h_dynamic = novo;
  }
  else
  {

    t_dynamic->Prox = novo;
  }
  t_dynamic = novo;
}

// saida da fila
int *dequeue()
{
  if (head == NULL)
  {
    return NULL;
  }
  else
  {
    int *fd = head->socket;
    queue *aux = head;
    head = head->Prox;
    if (head == NULL)
    {
      tail = NULL;
    }
    free(aux);
    return fd;
  }
}
void *dequeue_dynamic()
{
  if (h_dynamic == NULL)
  {
    return NULL;
  }
  else
  {
    int *fd = h_dynamic->socket;
    queue *aux = h_dynamic;
    h_dynamic = h_dynamic->Prox;
    if (h_dynamic == NULL)
    {
      t_dynamic = NULL;
    }
    free(aux);
    return fd;
  }
}
void doit(int *fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void * verQueue(void *arg);

int verifica_static(int fd);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
sem_t sem;

int numeroRequestStat = 0;
int numeroRequestDyn = 0;
int queueSize = 0;
char *schedalg;
// pthread_barrier_wait() init

int main(int argc, char **argv)
{
  sem.__align = 0;
  int listenfd, connfd, port;
  unsigned int clientlen; // change to unsigned as sizeof returns unsigned
  struct sockaddr_in clientaddr;

  /* Check command line args */
  if (argc != 5)
  {
    fprintf(stderr, "usage: %s <port> <threads> <queue> <schedalg>\n", argv[0]);
    // 
    // schedalg: the scheduling algorithm to be performed. One of ANY, FIFO, HPSC, or HPDC.
    exit(1);
  }

  // inicializar variaveis
  port = atoi(argv[1]);
  int nthreads = atoi(argv[2]);
  queueSize = atoi(argv[3]);
  schedalg = argv[4];

  // create thread pool
  pthread_t thread_p[nthreads];
  int i;
  for (i = 0; i < nthreads; i++)
  {
    pthread_create(&thread_p[i], NULL, verQueue, NULL);
  }

  fprintf(stderr, "Server : %s Running on  <%d> port with <%d> threads and <%d> buff-size schedalg: <%s>\n", argv[0], port, nthreads, queueSize, schedalg);
  // ./tiny.c 8080 4 4 ANY
  // Abriu o ouvinte da porta
  listenfd = Open_listenfd(port);

  if (strcmp(schedalg, "FIFO") == 0 || strcmp(schedalg, "ANY") == 0)
  {
    fprintf(stderr, "Scheduling algorithm: FIFO\n");
    while (1)
    {
      // se o numero de request for igual a queueSize, espera para diminuir a queue
      if (numeroRequestStat == queueSize)
      {
        sem_wait(&sem); // blocka enquanto espera que alguem saia da queue
      }
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
      int *p_connfd = malloc(sizeof(int));
      *p_connfd = connfd;

      pthread_mutex_lock(&mutex);
      enqueue(p_connfd);
      numeroRequestStat++;
      pthread_cond_signal(&condition_var); // manda signal para trabalhar
      pthread_mutex_unlock(&mutex);
    }
  }
  else if (strcmp(schedalg, "HPSC") == 0)
  {
    fprintf(stderr, "Scheduling algorithm: HPSC\n");
    while (1)
    {
      // se o numero de request for igual a queueSize, espera para diminuir a queue
      if ((numeroRequestStat+numeroRequestDyn) == queueSize)
      {
        sem_wait(&sem); // blocka enquanto espera que alguem saia da queue
      }
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
      int *p_connfd = malloc(sizeof(int));
      *p_connfd = connfd;
      int verifica = verifica_static(connfd);
      if (verifica == 0)
      { // adiciona na fila dinamica
        /* code */
        pthread_mutex_lock(&mutex);
        enqueue_dynamic(p_connfd);
        numeroRequestDyn++;
        pthread_cond_signal(&condition_var); // manda signal para trabalhar
        pthread_mutex_unlock(&mutex);
      }
      else
      {
        pthread_mutex_lock(&mutex);
        enqueue(p_connfd);
        numeroRequestStat++;
        pthread_cond_signal(&condition_var); // manda signal para trabalhar
        pthread_mutex_unlock(&mutex);
      }
    }
  }
  else if (strcmp(schedalg, "HPDC") == 0)
  {
    fprintf(stderr, "Scheduling algorithm: HPDC\n");
    while (1)
    {
      // se o numero de request for igual a queueSize, espera para diminuir a queue
      if ((numeroRequestStat+numeroRequestDyn) == queueSize)
      {
        sem_wait(&sem); // blocka enquanto espera que alguem saia da queue
      }
      clientlen = sizeof(clientaddr);
      connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
      int *p_connfd = malloc(sizeof(int));
      *p_connfd = connfd;
      int verifica = verifica_static(connfd);
      if (verifica == 0)
      { // adiciona na fila dinamica
        /* code */
        pthread_mutex_lock(&mutex);
        enqueue_dynamic(p_connfd);
        numeroRequestDyn++;
        pthread_cond_signal(&condition_var); // manda signal para trabalhar
        pthread_mutex_unlock(&mutex);
      }
      else
      {
        pthread_mutex_lock(&mutex);
        enqueue(p_connfd);
        numeroRequestStat++;
        pthread_cond_signal(&condition_var); // manda signal para trabalhar
        pthread_mutex_unlock(&mutex);
      }
    }
  }
  else
  {
    fprintf(stderr, "Scheduling algorithm: <%s> not supported\n", schedalg);
    exit(1);
  }

  sem_destroy(&sem);
}

// Funcao das threads que esta sempre a verificar queue
void * verQueue(void *arg)
{
  /* verificar se o schedalg é fifo
Se for fifo ele adiciona na queue e le da queue pois nao importa se é dinamico ou nao
  */
  if (strcmp(schedalg, "FIFO") == 0)
  {
    while (1)
    {
      pthread_mutex_lock(&mutex);
      int *fd = dequeue();
      pthread_mutex_unlock(&mutex);

      if (fd != NULL)
      {
        numeroRequestStat--;
        doit(fd);
        sem_post(&sem); // liberta o semaforo para trabalhar a main
        if (numeroRequestStat == 0)
        {
          pthread_cond_wait(&condition_var, &mutex);
        }
      }
    }
  }
  /* verificar se o schedalg é HPSC
  Se for HPSC vai ver se a queue static tem algo se tiver le da queue static e quando tiver fazia le
  a queue dinamica e quando estao as duas fazias manda parar na main
  */
  else if (strcmp(schedalg, "HPSC") == 0)
  {
    while (1)
    {
      pthread_mutex_lock(&mutex);
      int *fd = dequeue();
      pthread_mutex_unlock(&mutex);

      if (fd != NULL)
      {
        numeroRequestStat--;
        doit(fd);
        sem_post(&sem); // liberta o semaforo para trabalhar a main
        if (numeroRequestStat == 0)
        {
          pthread_cond_wait(&condition_var, &mutex);
        }
      }
      else if (numeroRequestDyn > 0)
      {
        pthread_mutex_lock(&mutex);
        int *fd = dequeue_dynamic();
        pthread_mutex_unlock(&mutex);

        if (fd != NULL)
        {
          numeroRequestDyn--;
          doit(fd);
          sem_post(&sem); // liberta o semaforo para trabalhar a main
          if (numeroRequestDyn == 0)
          {
            pthread_cond_wait(&condition_var, &mutex);
          }
        }
      }
    }
  }
  else if (strcmp(schedalg, "HPDC") == 0)
  {
    while (1)
    {

      pthread_mutex_lock(&mutex);
      int *fd = dequeue_dynamic();
      pthread_mutex_unlock(&mutex);

      if (fd != NULL)
      {
        numeroRequestDyn--;
        doit(fd);
        sem_post(&sem); // liberta o semaforo para trabalhar a main
        if (numeroRequestDyn == 0)
        {
        }
      }
      else if (numeroRequestStat > 0)
      {
        // faz os doit da queue statica
        pthread_mutex_lock(&mutex);
        int *fdS = dequeue();
        pthread_mutex_unlock(&mutex);
        if (fdS != NULL)
        {
          numeroRequestStat--;
          doit(fdS);
          sem_post(&sem); // liberta o semaforo para trabalhar a main
          if (numeroRequestStat == 0)
          {
            pthread_cond_wait(&condition_var, &mutex);
          }
        }
      }
    }
  }
}

// Function to verifica se a ligacao é statica ou dinamica
int verifica_static(int fd)
{
  int is_static;
  char uri[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  /* verifica se o fd é statico */
  is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck
  return is_static;
}

/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */

void doit(int *p_fd)
{
  int fd = *p_fd;
  free(p_fd);
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);             // line:netp:doit:readrequest
  sscanf(buf, "%s %s %s", method, uri, version); // line:netp:doit:parserequest
  if (strcasecmp(method, "GET"))
  { // line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }                       // line:netp:doit:endrequesterr
  read_requesthdrs(&rio); // line:netp:doit:readrequesthdrs

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); // line:netp:doit:staticcheck
  if (stat(filename, &sbuf) < 0)
  { // line:netp:doit:beginnotfound
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return ;
  } // line:netp:doit:endnotfound

  if (is_static)
  { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    { // line:netp:doit:readable
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // line:netp:doit:servestatic
  }
  else
  { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    { // line:netp:doit:executable
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // line:netp:doit:servedynamic
  }
  Close(fd);
}

/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  { // line:netp:readhdrs:checkterm
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  { /* Static content */             // line:netp:parseuri:isstatic
    strcpy(cgiargs, "");             // line:netp:parseuri:clearcgi
    strcpy(filename, ".");           // line:netp:parseuri:beginconvert1

    strcat(filename, uri);           // line:netp:parseuri:endconvert1
    if (uri[strlen(uri) - 1] == '/') // line:netp:parseuri:slashcheck
      strcat(filename, "home.html"); // line:netp:parseuri:appenddefault
    return 1;
  }
  else
  { /* Dynamic content */  // line:netp:parseuri:isdynamic
    ptr = index(uri, '?'); // line:netp:parseuri:beginextract
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, ""); // line:netp:parseuri:endextract
     strcpy(filename, "."); // line:netp:parseuri:beginconvert2
    strcat(filename, uri); // line:netp:parseuri:endconvert2
    return 0;
  }
}

/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);    // line:netp:servestatic:getfiletype
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // line:netp:servestatic:endserve

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);                        // line:netp:servestatic:open
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // line:netp:servestatic:mmap
  Close(srcfd);                                               // line:netp:servestatic:close
  Rio_writen(fd, srcp, filesize);                             // line:netp:servestatic:write
  Munmap(srcp, filesize);                                     // line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 * Deverá adicionar mais tipos
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  int pipefd[2];

  /*Paul Crocker
    Changed so that client content is piped back to parent
  */
  Pipe(pipefd);

  if (Fork() == 0)
  { /* child */ // line:netp:servedynamic:fork
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // line:netp:servedynamic:setenv
    // Dup2 (fd, STDOUT_FILENO);	/* Redirect stdout to client *///line:netp:servedynamic:dup2
    Dup2(pipefd[1], STDOUT_FILENO);

    Execve(filename, emptylist, environ); /* Run CGI program */ // line:netp:servedynamic:execve
  }
  close(pipefd[1]);
  char content[1024]; // max size that cgi program will return

  int contentLength = read(pipefd[0], content, 1024);
  Wait(NULL); /* Parent waits for and reaps child */ // line:netp:servedynamic:wait

  /* Generate the HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
  sprintf(buf, "%sContent-length: %d\r\n", buf, contentLength);
  sprintf(buf, "%sContent-type: text/html\r\n\r\n", buf);
  Rio_writen(fd, buf, strlen(buf)); // line:netp:servestatic:endserve

  Rio_writen(fd, content, contentLength);
}

/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  /* Fazer primeiro visto que precisamos de saber o tamanho do body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sRequestStat: %d\r\n", buf, numeroRequestStat++);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));
}

/* $end clienterror */

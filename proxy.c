#include <stdio.h>
#include "csapp.h"
#include "lrucache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define DEFAULT_HTTP_PORT 80

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_ = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";

static sem_t mutex;

int parse_request(rio_t *rp, char *uri, char *host, int *port, char *sendreq);
void connect_server(char *host, int port, char *buf);
void *do_proxy(void *filedesc);
int open_clientfd_n(char *hostname, int port);
int Open_clientfd_n(char *hostname, int port);
void sigpipe_handler(int signal);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);

int main(int argc, char**argv)
{
    int listenfd, port;
    int *connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char *haddrp;
    pthread_t tid;
    
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    initialize_cache(MAX_CACHE_SIZE, MAX_OBJECT_SIZE);
    Signal(SIGPIPE, sigpipe_handler);
    listenfd = Open_listenfd(port);
    clientlen = sizeof(struct sockaddr_in);
    while (1) {
        
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("Receive connection from ip address: %s\n",haddrp);
        Pthread_create(&tid, NULL, do_proxy, connfd);
    }
    return 0;
}

/* 
 * sigpipe_handler - ignore the SIGPIPE signal 
 */
void sigpipe_handler(int signal)
{
    printf("****Receive SIGPIPE****");
    return;
}

/* 
 * do_proxy - main proxy routine 
 */
void *do_proxy(void *filedesc)
{
    int clientfd, port, fd, status;
    size_t n;
    char response[MAXBUF], sendreq[MAXBUF], host[MAXLINE], url[MAXLINE];
    char version[10], statusmsg[20];
    void *pcache;
    rio_t rio;
    size_t object_size;
    Pthread_detach(pthread_self()); 
    fd = *((int *)filedesc);
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (parse_request(&rio, url, host, &port, sendreq) == 0) {
        print_cachelist();
        printf("fd:%d GET url:%s\n",fd,url);
        
        if ((pcache = cache_get(url,&object_size)) != NULL) {
            
            // read from cache
            printf("fd:%d ==========cache HIT!==========\n",fd);
            Rio_writen(fd, pcache, object_size);
        } else {
            
            // read from io
            clientfd = Open_clientfd_n(host, port);
            Rio_readinitb(&rio, clientfd);
            Rio_writen(clientfd, sendreq, strlen(sendreq));
            printf("fd:%d Sending request to host:%s... \n%s",fd,host,sendreq);
            
            // read status code
            n = Rio_readlineb(&rio, response, MAXLINE);
            sscanf(response, "%s %d %s", version, &status, statusmsg);
            printf("%s",response);
            size_t size = n;
            Rio_writen(fd, response, n);
            if (status == 200) {
                pcache = Malloc(size);
                memcpy(pcache, response, n);
            }
            bzero(response, MAXLINE);
            while (1) {
                
                if ((n = Rio_readnb(&rio, response, MAXBUF)) <= 0) {
                    break;
                }
                
                size += n;
                Rio_writen(fd, response, n);
                if (pcache) {
                    if (size <= MAX_OBJECT_SIZE) {
                        pcache = Realloc(pcache, size);
                        memcpy(pcache + (size - n), response, n);
                        
                    } else {
                        Free(pcache);
                        pcache = NULL;
                    }
                }
                printf("%s",response);
                bzero(response, MAXBUF);
            }
            if (pcache) {
                cache_put(url, pcache, size);
            }
            
            
            Close(clientfd);
        }
    }
    
    
    Close(fd);
    Free(filedesc);
    return NULL;
}

/* 
 * parse_request - parse the request received from client 
 */
int parse_request(rio_t *rp, char *uri, char *host, int *port, char *sendreq)
{
    char buf[MAXLINE], method[MAXLINE], version[10];
    char *p, *path;
    int i = 0;
    
    // ignore empty request

    size_t n = Rio_readlineb(rp, buf, MAXLINE);
    if (n <= 0) {
        clienterror(rp->rio_fd, "Bad request", "0", "Empty request", "Empty request");
        return -1;
    }
    // eliminate all CRLF at beginning
    while(!strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
    }
    printf(">%s", buf);
    fflush(stdout);
    sscanf(buf, "%s %s %s", method, uri, version);
    p = strstr(uri, "://");
    if (p == NULL) {
        clienterror(rp->rio_fd, "Bad request", "1", "Wrong Protocol", "Wrong Protocol");
        return -1;
    }
    p += 3;
    for (; *p != '\0' && *p != '/'; p++, i++) {
        host[i] = *p;
    }
    host[i+1]='\0';
    
    if (*p == '\0') {
        strcat(uri, "/");
    }
    path = p;
    
    p = strstr(host, ":");
    if (!p) {
        *port = DEFAULT_HTTP_PORT;
    } else {
        *p = '\0';
        p++;
        *port = atoi(p);
        
    }
    
    sprintf(sendreq, "%s %s HTTP/1.0\r\n", method, path);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        if (strstr(buf, ":") && !strstr(buf, "User-Agent:") && !strstr(buf, "Accept:")
            && !strstr(buf, "Accept-Encoding:") && !strstr(buf, "Connection:") && !strstr(buf, "Proxy-Connection:")) {
            strcat(sendreq, buf);
        }
        
        printf(">%s", buf);
        fflush(stdout);
    }
    strcat(sendreq, user_agent);
    strcat(sendreq, accept_);
    strcat(sendreq, accept_encoding);
    strcat(sendreq, "Connection: close\r\n");
    strcat(sendreq, "Proxy-Connection: close\r\n");
    if (!strstr(sendreq, "Host:")) {
        sprintf(sendreq, "%sHost: %s\r\n",sendreq,host);
    }
    strcat(sendreq, "\r\n");
    return 0;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);
    
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * open_clientfd_n - open connection to server at <hostname, port>
 *   and return a socket descriptor ready for reading and writing.
 *   Returns -1 and sets errno on Unix error.
 *   Returns -2 and sets h_errno on DNS (gethostbyname) error.
 */
int open_clientfd_n(char *hostname, int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;
    
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* check errno for cause of error */
    Sem_init(&mutex, 0, 1);
    P(&mutex);
    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; /* check h_errno for cause of error */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
          (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);
    V(&mutex);
    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}

/*
 * Open_clientfd_n - wrapper for open_clientfd_n
 */
int Open_clientfd_n(char *hostname, int port)
{
    int rc;
    
    if ((rc = open_clientfd_n(hostname, port)) < 0) {
        if (rc == -1)
            unix_error("Open_clientfd Unix error");
        else
            dns_error("Open_clientfd DNS error");
    }
    return rc;
}


/*Aayush Karki
 Andrew : aayushka*/

#include "csapp.h"
#include "cache.h"

/********************************
Uses second reader-writer policy
*********************************/

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

struct req_info{
    char method[12];
    char hostname[200];
    char suffix[MAXLINE];
    char cache_id[MAXLINE];
    char header[MAXLINE];
    char port[7];
};

void getHeaders(rio_t *rio, struct req_info *req);
int parser(char *cmdline, struct req_info *req);
char* adjust_hdr(char *buf);
int client_to_proxy(int fd, struct req_info *req);
void getHeaders(rio_t *rp, struct req_info *req);
void proxy_to_client(int fd, struct file *obj);
void proxy_to_client(int fd, struct file *obj);
void server_to_client_N_cache(int fd, struct req_info *req);
void handle_clientrequest(int fd, struct req_info *req);
void *client_thread(void *vargp);

sem_t mutex1, mutex2, mutex3,r, w;
int readcnt = 0;
int writecnt = 0;


/*
* Parse a host request cmdline to fields in req
*/
int parser(char *cmdline, struct req_info *req){   
    char url[MAXLINE];
    char version[10];
    char method[10];

    //parse cmd. eg: (CONNECT www.google.com:8080 HTTP/1.1)
    if(sscanf(cmdline, "%s %s %s", method, url, version) !=3)
        return 0;
    

    char hostnameAndport[MAXLINE];
    char hostname[MAXLINE];
    char port[10];
    char path[MAXLINE];
    path[0] = '/';

    char *tmp = url;
    tmp = strstr(tmp, "://");  // tmp is NULL if "://" exists
    
    //if "://" exists then move the char ptr after its occurence
    tmp = (tmp==NULL)? url : tmp+3; 

    //split at first "/" in eg. google.com:8080/imgsearch/xyz.jpg
    sscanf(tmp, "%[^/]%s",  hostnameAndport,path);
     
    //partition at ":" in eg. google.com:8080
    sscanf(hostnameAndport, "%[^:]:%s", hostname,port);

     strcpy(req->method, method);
     strcpy(req->hostname, hostname);
     strcpy(req->suffix, path);
 
     if (strlen(port)==0) //assign default port 80 if not requested
             strcpy(port, "80");
     else strcpy(req->port, port);
   
     //cache is indexed by hostname, port and path together as a string
     sprintf(req->cache_id, "%s %s %s", hostname, port, path);

    return 1;
}


/*
* Adjust host headers
*/
char* adjust_hdr(char *buf){
     
        char hdrkey[MAXLINE];
        char hdr_value[MAXLINE];

        if (sscanf(buf, "%s %s", hdrkey, hdr_value)!=2){
            return buf;
        }
        //adjust to default headers
        if(!strcmp(hdrkey, "User-Agent:"))
            strcpy(buf, user_agent_hdr);
        else if(!strcmp(hdrkey, "Accept:"))
            strcpy(buf, accept_hdr);
        else if(!strcmp(hdrkey, "Accept-Encoding:"))
            strcpy(buf, accept_encoding_hdr);
        else if(!strcmp(hdrkey, "Connection:"))
            strcpy(buf, conn_hdr);
        else if(!strcmp(hdrkey, "Proxy-Connection:"))
            strcpy(buf, prox_hdr);     
        
    return buf;
}


/*
* Gets connection request headers from client for proxy
*/
int client_to_proxy(int fd, struct req_info *req){
    size_t n;
    char buf[MAXLINE];
    rio_t rio;
    
    Rio_readinitb(&rio, fd); 
    
    //get request from client
    if((n = Rio_readlineb(&rio, buf, MAXLINE)) <= 0) return 0;

    if(parser(buf, req) == 0) {
        printf("%s\n", "Command parseing error" );
        return 0;
    }
    getHeaders(&rio, req); //adjust all request headers

    return 1;
}

/*
*Get all the request headers and adjust them
*/
void getHeaders(rio_t *rp, struct req_info *req) 
{
    char buf[MAXLINE];
    char tmp[6]; 
    
    //check is first header is hostname

    Rio_readlineb(rp, buf, MAXLINE);
    sscanf(buf, "%s ", tmp);
    strcpy(req->header, buf);

    //check if the first header is the host info
    if (strcmp(tmp, "Host:")){
        strcat(req->header, "Host: ");
        strcat(req->header, req->hostname);
    }


    while(strcmp(buf, "\r\n")) { //read all request headers     
        Rio_readlineb(rp, buf, MAXLINE);
        //adjust headers before forwarding to real server
        strcat(req->header, adjust_hdr(buf)); 
    }
    
    return;
}

/*
* Forward data  to client
*/
void proxy_to_client(int fd, struct file *obj){
        //if write from proxy to client fails, then close
        //connection by exiting the client thread
          if (rio_writen(fd, obj->data, obj->fsize)  <=0){
            Close(fd);
            Pthread_exit(NULL);
        }  
}

/*
* Opens connection with server.
* Receives data from server and forwards to client.
* Keeps a copy of data received from server and caches it
*/
void server_to_client_N_cache(int fd, struct req_info *req){
    int serverfd;
    int n;
    rio_t srio;
    char buf[MAXLINE];

        serverfd = Open_clientfd(req->hostname, req->port);
        char cmd[MAXLINE];


        sprintf(cmd, "%s %s HTTP/1.0\r\n%s", req->method, 
                                             req->suffix, 
                                             req->header);
  
        //send request to server
        if (rio_writen(serverfd, cmd, strlen(cmd)) == -1){
            printf("%s\n", "rio writen error 1");
            Pthread_cancel(pthread_self());
        }
        //terminate request
        if (rio_writen(serverfd, "\r\n", 2)  == -1 ){
            printf("%s\n", "rio writen error 2");
            Pthread_cancel(pthread_self());
        }

    
    
    printf("...start receiving from server\n" );
    Rio_readinitb(&srio, serverfd);
    
    int objsize=0;
    char *mbuf = (char *)malloc(MAX_OBJECT_SIZE); 
    char *mbuf_head = mbuf;
    
            //keep reading from server
            while((n = Rio_readlineb(&srio, buf, MAXLINE)) > 0){
                
                //If write to client fails then exit thread
                if (rio_writen(fd, buf, n)==-1)
                {
                    free(mbuf);
                    Close(serverfd);
                    Pthread_exit(NULL);
                }
                
                if(objsize+n <= MAX_OBJECT_SIZE){ 
                    memcpy(mbuf, buf, n);
                    mbuf += n; //move pointer by n bytes
                    objsize +=n;        
                }
       
            }
        /*Cache the object only if it is less than the max size */
            if( (objsize<=MAX_OBJECT_SIZE) &&  (!strcmp(req->method, "GET")) ){
                //implement second reader-writer
                P(&mutex2);
                writecnt += 1;
                if (writecnt ==1) P(&r);
                V(&mutex2);
                P(&w);

                add2cache(req->cache_id, mbuf_head, objsize );
                V(&w);

                P(&mutex2);
                writecnt = writecnt -1;
                if (writecnt ==0) V(&r);
                V(&mutex2);
            }
            free(mbuf_head);


}

/*
* Forwards from proxy cache to cleint if available.
  Else calls server_to_client_N_cache(fd, req)
*/
void handle_clientrequest(int fd, struct req_info *req){

    P(&mutex3);
    P(&r);
    P(&mutex1);
    readcnt +=1;
    if (readcnt ==1) P(&w);
    V(&mutex1);
    V(&r);
    V(&mutex3);

    struct file *obj =  retrieve(req->cache_id); //retrieve from cache

    if(obj!=NULL){ // cache_id is found in cache
        proxy_to_client(fd, obj);
      
        P(&mutex1);
        readcnt = readcnt - 1;
        if (readcnt==0) V(&w);
        V(&mutex1);
        return ;
    }
    
    
        P(&mutex1);
        readcnt = readcnt - 1;
        if (readcnt==0) V(&w);
        V(&mutex1);

        if (strcmp(req->method,"GET")){
            printf("method %s\n", req->method );
            printf("%s %s\n", 
                    "non-Get method. Forward to client anyways.", 
                     req->method);
        
        }

    server_to_client_N_cache(fd, req);   

}

/*
* Inititiates each client as an indeprendent terminating thread
*/
void *client_thread(void *vargp) /* Thread routine */
 {  /*detach thread from parent thread*/
    Pthread_detach(pthread_self());
    int connfd = *((int*) vargp);
    Free(vargp);

    struct req_info req;
    
        //establish connection between client and proxy
        if (client_to_proxy(connfd, &req) == 0){
            printf("bad request\n" );
            Close(connfd);
            Pthread_cancel(pthread_self());
        }
        
    handle_clientrequest(connfd, &req);
    Close(connfd);
    printf("%s\n", 
            "Handled all client request. Closing connection with proxy\n\n\n");
    Pthread_exit(NULL);
    return NULL;
 }




int main(int argc, char **argv)
{


    if (argc !=2 ){
        printf("usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Sem_init(&mutex1, 0, 1);
    Sem_init(&mutex2, 0, 1);
    Sem_init(&mutex3, 0, 1);
    Sem_init(&r, 0, 1);
    Sem_init(&w, 0, 1);
    
    int *connfd, listenfd;
    socklen_t clientlen;
    char *listenport;

    struct sockaddr_in clientaddr;
    pthread_t tid;

    signal(SIGINT, erase_cache);
       // ignore SIGPIPE 
    signal(SIGPIPE, SIG_IGN);


    listenport = argv[1];
    
    if((listenfd = Open_listenfd(listenport))<0) exit(0);
    clientlen = sizeof(clientaddr);

    init_cache(); //initialize proxy cache

    while(1){
        clientlen = sizeof(clientaddr);
        connfd = (int*)calloc(1, sizeof(int));
        *connfd = Accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen); 
       Pthread_create(&tid, NULL, client_thread, (void *) connfd);
     
      
    }
    erase_cache(); //Erase the cache; also invokes exit(0)
    
    return 0;
}








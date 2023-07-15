#define _XOPEN_SOURCE 700
/**
a. Is compiled by the “all” and “default” target of a Makefile in the “server” directory and supports cross compilation, placing the executable file in the “server” directory and named aesdsocket.

b. Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.

     c. Listens for and accepts a connection

     d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client. 

     e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.

Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.

You may assume the data stream does not include null characters (therefore can be processed using string handling functions).

You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.

     f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.

You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

     g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

     h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

     i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/wait.h>

#define NR_OPEN 1048576
#define PORT "9000"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

static void handler(int signum)
{
     /* Take appropriate actions for signal delivery */
     if (signum == SIGTERM || signum == SIGINT) {
          int res;
          if ((res = remove("/var/tmp/aesdsocketdata")) == -1) {
               fprintf(stderr, "Error in removing \"/var/tmp/aesdsocketdata\" : %s\n", strerror(errno));
               exit(-1);
          };
          exit(0);   
     }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {

     pid_t pid;
     int i;
     
     /**
      * sig action 
      */
     struct sigaction sa;
     memset(&sa, 0, sizeof(struct sigaction));
     sa.sa_handler = handler;
     sigemptyset (&sa.sa_mask);
     sa.sa_flags = SA_RESTART;

     sigaction (SIGINT, &sa, NULL);
     sigaction (SIGTERM, &sa, NULL);
  
     struct addrinfo hints, *servinfo, *p;
     struct sockaddr_storage their_addr; // connector's address information
     socklen_t sin_size;
     struct sigaction saChild;
     int yes=1;
     char s[INET6_ADDRSTRLEN];
     int rv;

     int server_socket;
     

     memset(&hints, 0, sizeof hints);
     hints.ai_family = AF_UNSPEC;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_flags = AI_PASSIVE; // use my IP

     if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
     }

     // server_socket = socket(PF_INET, SOCK_STREAM, 0);
     // loop through all the results and bind to the first we can
     for(p = servinfo; p != NULL; p = p->ai_next) {
          if ((server_socket = socket(p->ai_family, p->ai_socktype, 
                    p->ai_protocol)) == -1) {
               perror("server: socket");
               continue;
          }

          if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(int)) == -1) {
               perror("setsockopt");
               exit(-1);
          }

          if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
               close(server_socket);
               perror("server: bind");
               continue;
          }

          break;
     }

     freeaddrinfo(servinfo); // all done with this structure

     if (p == NULL)  {
          fprintf(stderr, "server: failed to bind\n");
          exit(-1);
     }



     if (server_socket == -1) {
          return -1;
     }

     // struct sockaddr_in server_addr;
     // memset(&server_addr, 0, sizeof(server_addr));
     // server_addr.sin_family = AF_INET;
     // server_addr.sin_port = htons(9000);
     // server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

     // int bdr;     
     // if ((bdr = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
     //      return -1;
     // }

     if (argc == 2 && strlen(argv[1]) == strlen("-d") && strncmp(argv[1], "-d", strlen("-d")) == 0) {
          if ((pid = fork()) != 0) {
               exit(EXIT_SUCCESS);
          } else {
               /* create new session and process group */ 
               if (setsid () == -1)
                    return -1;
               /* set the working directory to the root directory */
               if (chdir ("/") == -1)
                    return -1;
               /* close all open files--NR_OPEN is overkill, but works */
               // for (i = 0; i < NR_OPEN; i++)
               //      close (i);
               /* redirect fd's 0,1,2 to /dev/null */
               open ("/dev/null", O_RDWR);
               dup (0);
               dup (0);
          }
     }

     if (listen(server_socket, BACKLOG) == -1) {
          perror("listen");
          exit(-1);
     }

     saChild.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&saChild.sa_mask);
     saChild.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &saChild, NULL) == -1) {
          perror("sigaction");
          exit(-1);
     }

     int client_socket;

     while (1)
     {
          struct sockaddr *addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
          socklen_t addr_len = sizeof(addr);
          
          client_socket = accept(server_socket, addr, &addr_len);

          struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
          char *sAddr = (char *)malloc(sizeof(char)* INET_ADDRSTRLEN);
          inet_ntop(AF_INET, (void *)(&(addr_in->sin_addr)), sAddr, INET_ADDRSTRLEN);

          // Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client. 
          syslog(LOG_DEBUG, "Accepted connection from %s", sAddr);

          /**
           * Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.
           * Your implementation should use a newline to separate data packets received.  In other words a packet is considered 
           * complete when a newline character is found in the input receive stream, and each newline should result in an append 
           * to the /var/tmp/aesdsocketdata file.
           * 
           * You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
           * You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you 
           * handle malloc() associated failures with error messages you may discard associated over-length packets.
           */
          int receivedSize;
          char buffer[1000];

          int fd;
          fd = open ("/var/tmp/aesdsocketdata", O_CREAT | O_WRONLY | O_APPEND, 0644);

          if (fd == -1) {
               printf("Error in creating file.\n");
               return -1;
          }

          memset(&buffer, '\0', 1000);

          bool packetEnded = false;
          /**
           * Receiving packets from clients 
           */
          while (!packetEnded && (receivedSize = recv(client_socket, buffer, 999, 0)) > 0) {
               char *message = (char *)malloc(sizeof(char)*(receivedSize));

               // for (int i = 0; i < receivedSize; i++) {
               //      printf("%d\n", buffer[i]);
               //      printf("%s\n", "-");
               // }
               // printf("%s\n", "*");

               // Check if memory is allocated
               if (message == NULL) {
                    return -1;
               }

               for (int i = 0; i < receivedSize; i++) {
                    if (buffer[i] == 10) {
                         message[i] = '\n';
                         packetEnded = true;
                         break;
                    } else {
                         message[i] = buffer[i];
                    }
               }

               /**
                * Check contents of buffer for new line.
                * Add content up to new line to packetData.
                * Write the packetData.
                * Set packetData the value of the remaining part of buffer.
                * Send back content of /var/tmp/aesdsocketdata
                */

               ssize_t nr;
               nr = write (fd, message, receivedSize);
               if (nr == -1) {
                    free(message);
                    return -1;
               }
               free(message);
               for (int i = 0; i < receivedSize; i++) {
                    if (buffer[i] == EOF)
                         break;
               }
               memset(&buffer, '\0', 1000);
          }
          close(fd);

          fd = open ("/var/tmp/aesdsocketdata", O_RDONLY);

          if (fd == -1) {
               printf("Error in opening file.\n");
               return -1;
          }

          
          /** 
           * Returns the full content of /var/tmp/aesdsocketdata to the client 
           * as soon as the received data packet completes.
           * 
           * You may assume the total size of all packets sent (and therefore size 
           * of /var/tmp/aesdsocketdata) will be less than the size of the root 
           * filesystem, however you may not assume this total size of all packets 
           * sent will be less than the size of the available RAM for the process heap.
           */ 
          int readSize;
          int size = 0, capacity = 1000;
          char *message = (char *)calloc(capacity, sizeof(char));
          memset(&buffer, '\0', 1000);


          while ((readSize = read(fd, buffer, 999)) > 0)
          {
               size += readSize;
               if (size > capacity) {
                    message = (char *)realloc(message, sizeof(char)*(size+1));
                    capacity = size + 1;
               }
               for (int i = size - readSize; i < size; i ++) {
                    message[i] = buffer[i - (size - readSize)];
               }
               memset(&buffer, '\0', 1000);
          }
          if (size > capacity) {
               message = (char *)realloc(message, sizeof(char)*(size+1));
               capacity = size + 1;
          }
          message[size] = '\0';
          send(client_socket, message, strlen(message) * sizeof(char), 0);
          
          /**
           * Logs message to the syslog “Closed connection from XXX” where XXX is 
           * the IP address of the connected client.
           */ 
          syslog(LOG_DEBUG, "Closed connection from %s", sAddr);

          free(message);      
          free(addr);   
          free(sAddr); 
          close(fd);
          close(client_socket);
     }

     /**
      * Gracefully exits when SIGINT or SIGTERM is received, completing any 
      * open connection operations, closing any open sockets, and deleting 
      * the file /var/tmp/aesdsocketdata.
      */ 
     int res;
     if ((res = remove("/var/tmp/aesdsocketdata")) == -1) {
          fprintf(stderr, "Error in removing \"/var/tmp/aesdsocketdata\" : %s\n", strerror(errno));
          return -1;
     };

     /**
      * Logs message to the syslog “Caught signal, exiting” when SIGINT 
      * or SIGTERM is received.     
      */
     close(server_socket);
     
     return 0;
}

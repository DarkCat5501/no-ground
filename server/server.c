#include <aio.h>
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctls.h>
#include <assert.h>
#include <base64.h>
#include <core.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sha1.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <thpool.h>
#include <unistd.h>
#include <ws.h>
// OPEN SSL

void calculateWebSocketAccept(const char *key, char *acceptBuffer,
                              size_t acceptBufferSize) {
  char magicString[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  u8 concatenated[256] = {0};
  int cat_len = snprintf((char *)concatenated, sizeof(concatenated), "%s%s",
                         key, magicString);
  unsigned char digest[SHA1HashSize] = {0};

  // Encode SHA1
  SHA1Context shactx = {0}; // old way
  SHA1Reset(&shactx);
  SHA1Input(&shactx, concatenated, cat_len);
  SHA1Result(&shactx, digest);

  // Base 64 encode
  u8 *encoded_64 = base64_encode(digest, SHA1HashSize, NULL);
  size_t len = strlen((char *)encoded_64);
  *(encoded_64 + len - 1) = '\0';
  // copy into acceptBuffer
  memcpy(acceptBuffer, encoded_64, len);
  free(encoded_64);
}

typedef struct {
  u32 port;
} ServerConfig;

void showUsage(char *program);
void panic(const char *fmt, ...);

typedef struct {
  int listenerfd;
  struct pollfd poll[200];
  size_t poll_size;
} Server;

typedef struct {
  struct sockaddr_in addr;
  int connfd;

  // per client explicit buffer
  u8 buffer[2048];
  u8 rbuffer[2048];
} ClientCon;

Server main_server = {0};
ClientCon clients[2048] = {0};
int total_clients = 0;
threadpool thpool = {0};

/**
 * Perform an asyncronous read on a given file
 **/
struct aiocb *async_read(int fd, u8 *buffer, size_t size) {
  struct aiocb *aio = calloc(sizeof(struct aiocb), 1);
  if (aio == NULL)
    panic("Buy more RAM! {Error allocation AIO}");
  aio->aio_buf = buffer;
  aio->aio_fildes = fd;
  aio->aio_nbytes = size;
  aio->aio_offset = 0; // start at begining

  if (aio_read(aio) < 0) {
    free(aio);
    return NULL;
  }
  return aio;
}

void initServer(void) {
  printf("Starting web server\n");
  // create the server socket
  if ((main_server.listenerfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    panic("Server initialization error");
  }

  // setup server information
  struct sockaddr_in serveraddr = {0};
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(8090);

  // Allow socket descriptor to be resusabel
  int on = 1;
  int rc = setsockopt(main_server.listenerfd, SOL_SOCKET, SO_REUSEADDR,
                      (char *)&on, sizeof(on));
  if (rc < 0) {
    panic("Failed to make socket reusable");
  }

  // Set the socket to be non-blocking
  rc = ioctl(main_server.listenerfd, FIONBIO, (char *)&on);
  if (rc < 0) {
    panic("Unable to set socket non-blocking");
  }

  // try to bind the server to the socket
  if (bind(main_server.listenerfd, (struct sockaddr *)&serveraddr,
           sizeof(serveraddr)) < 0) {
    panic("Server binding erro");
  }

  // start listening for connections
  if (listen(main_server.listenerfd, 32) < 0) {
    panic("Server couldn't receiv any connections");
  }

  // setup initial socket listener
  main_server.poll[0].fd = main_server.listenerfd;
  main_server.poll[0].events = POLLIN;
  main_server.poll_size = 1;
}

int pollConnections(void) {
  // setup initial socket listener
  int timeout = 0; //(16 * 100); //-1: waits until have a connection
  int res = poll(main_server.poll, main_server.poll_size, timeout);

  if (res < 0) {
    panic("Poll connections failed!");
  } else if (res == 0) {
    // NO connection needed to be checked
  } else {
    return res;
  }

  return 0;
}

int validateHttpWSUpgradeHeader(char *buffer, size_t size, char **socket_key) {
  char *tk = strtok(buffer, " :\r\n");

  bool has_ws_key = false;
  while (tk != NULL) {
    tk = strtok(NULL, " :\r\n");
    if (has_ws_key) {
      *socket_key = tk;
      return 1;
    }

    if (strcmp("Sec-WebSocket-Key", tk) == 0) {
      has_ws_key = true;
    }
  }

  return 0;
}

void createWSPingSignal(char *buffer, size_t size) {}

void processClientRequest(ClientCon client) {
  int bytes_read = 0;
  memset(client.rbuffer, 0, sizeof(client.rbuffer));
  do {
    bytes_read = recv(client.connfd, client.rbuffer, sizeof(client.rbuffer),
                      MSG_DONTWAIT);
    if (bytes_read <= 0)
      break; // could not read any data because there was nothing

    if (bytes_read >= sizeof(WSFrameHeader)) {
      printf("WS Request (%d)---------------\n", client.connfd);
      debugWsPacket(client.rbuffer, bytes_read);
      printf("WS end (%d)-------------------\n\n", client.connfd);
    }
  } while (bytes_read > 0);
}

int acceptWSHandshake(ClientCon client) {
  int bytes_read = 0;
  // int handshake_stage = 0;

  // do {
  // clear receive buffer and send buffer
  memset(client.rbuffer, 0, sizeof(client.rbuffer));
  memset(client.buffer, 0, sizeof(client.buffer));
  // receiv messaage
  bytes_read = recv(client.connfd, client.rbuffer, sizeof(client.rbuffer),MSG_DONTWAIT);
  if(bytes_read<=0) return 0; //connection failed
  // if (handshake_stage == 0) { // handle Handshake stage 0
  // printf("Client %d sent %d bytes\n", client.connfd, bytes_read);
  // printf("bytes received: %s",client.rbuffer);
  // TODO: handle HTTPS websockets
  char *ws_key = NULL;
  int ws_ok = validateHttpWSUpgradeHeader((char *)client.rbuffer, bytes_read, &ws_key);

  if (ws_ok) {
    char acceptBuffer[128] = {0}; // calculate handshake key
    calculateWebSocketAccept(ws_key, acceptBuffer, sizeof(acceptBuffer));
    int acceptLen = snprintf((char *)client.buffer, sizeof(client.buffer),
                             "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: %s\r\n"
                             "\r\n",
                             acceptBuffer);
    send(client.connfd, client.buffer, acceptLen,MSG_DONTWAIT);
    //try to process any early client request
    // processClientRequest(client);
  } else {
    printf("- Handshake: FAILED\n");
    return 0;
  }
  printf("- Handshake: OK\n");
  return 1; 
}

void processNewClient(ClientCon client) {
  printf("--{{New client trying connection (%d): %s at %d}}---\n",
         client.connfd, inet_ntoa(client.addr.sin_addr),
         ntohs(client.addr.sin_port));
  int handshake_ok = acceptWSHandshake(client);

  if (handshake_ok) {
    clients[total_clients++] = client;
    // add client to connection pool
    main_server.poll[total_clients].fd = client.connfd;
    main_server.poll[total_clients].events = POLLIN;
    main_server.poll_size = total_clients + 1;

    // AddClient to client buffer
    printf("--{{New client connected {%d}(%d): %s at %d}}--\n", total_clients,
           client.connfd, inet_ntoa(client.addr.sin_addr),
           ntohs(client.addr.sin_port));
  } else {
    // kill connection
    close(client.connfd);
  }
}

void processConnection() {
  /***********************************************************/
  /* One or more descriptors are readable.  Need to          */
  /* determine which ones they are.                          */
  /***********************************************************/
  size_t current_size = main_server.poll_size;
  for (int connId = 0; connId < current_size; connId++) {
    // skip cause there's no data incomming
    struct pollfd pf = main_server.poll[connId];
    if (pf.revents == 0) {
      continue;
    }

    /*********************************************************/
    /* If revents is not POLLIN, it's an unexpected result,  */
    /* log and end the server.                               */
    /*********************************************************/
    if (!(pf.revents & POLLIN)) {
      if (pf.revents & POLLERR) {
        fprintf(stderr, "Connection raised an error: %s\n", strerror(errno));
        continue;
      } else if (pf.revents & POLLHUP) {
        printf("Connection just hungup %s\n", strerror(errno));
        continue;
      }
    }

    if (pf.fd == main_server.listenerfd) {
      printf("\n...\n");
      int clientfd = -1;
      do {
        ClientCon clientconn = {0};
        socklen_t adr_len = sizeof(struct sockaddr_in);
        clientfd = accept(main_server.listenerfd,
                          (struct sockaddr *)&clientconn.addr, &adr_len);

        if (clientfd < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // printf("Missconnection just skiped!\n");
          } else {
            // panic("Error while processing client accept");
          }
        } else {
          clientconn.connfd = clientfd;
          processNewClient(clientconn);
        }

      } while (clientfd != -1);
    } else {
      int clientId = connId - 1;
      assert(clientId >= 0 && "Missuse of global connection listener");
      processClientRequest(clients[clientId]);
    }
  }
}

void initThreadPool(void) {
  printf("Starting pool...\n");
  thpool = thpool_init(2);
}

void processPhysics(void *args) {
  int thread_id = pthread_self();
  printf("Thread #%u started working on physics", thread_id);

  do {
    // printf("physics update!\n");
    usleep(1000 / 2); // force 2fps only
  } while (1);
}

int main(int argc, char *argv[]) {
  initThreadPool();

  int server_running = 1;
  initServer();

  // dispatch thead to start working on physics
  //  thpool_add_work(thpool,processPhysics,NULL);

  do {
    int amount = pollConnections();
    // printf("* amout = %d\n",amount);
    if (amount > 0) {
      // printf("Incomming connection...\n");
      processConnection();
    }

    usleep(1000 / 1); // about 60 FPS
  } while (server_running);

  return 0;
}

void showUsage(char *program) {
  printf("Usage: %s [options]\n"
         "\t-p port=8090 sets the server default port\n",
         program);
}

void panic(const char *fmt, ...) {
  int errno_tmp = errno;
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  if (errno_tmp != 0) {
    fprintf(stderr, "(errno = %d) : %s", errno_tmp, strerror(errno_tmp));
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  exit(1);
}
// basic poll server arch

// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/ioctl.h>
// #include <sys/poll.h>
// #include <sys/socket.h>
// #include <sys/time.h>
// #include <netinet/in.h>
// #include <errno.h>
//
// #define SERVER_PORT  12345
//
// #define TRUE             1
// #define FALSE            0
//
// int main (int argc, char *argv[])
// {
//   int    len, rc, on = 1;
//   int    listen_sd = -1, new_sd = -1;
//   int    desc_ready, end_server = FALSE, compress_array = FALSE;
//   int    close_conn;
//   char   buffer[80];
//   struct sockaddr_in6   addr;
//   int    timeout;
//   struct pollfd fds[200];
//   int    nfds = 1, current_size = 0, i, j;
//
//   /*************************************************************/
//   /* Create an AF_INET6 stream socket to receive incoming      */
//   /* connections on                                            */
//   /*************************************************************/
//   listen_sd = socket(AF_INET6, SOCK_STREAM, 0);
//   if (listen_sd < 0)
//   {
//     perror("socket() failed");
//     exit(-1);
//   }
//
//   /*************************************************************/
//   /* Allow socket descriptor to be reuseable                   */
//   /*************************************************************/
//   rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
//                   (char *)&on, sizeof(on));
//   if (rc < 0)
//   {
//     perror("setsockopt() failed");
//     close(listen_sd);
//     exit(-1);
//   }
//
//   /*************************************************************/
//   /* Set socket to be nonblocking. All of the sockets for      */
//   /* the incoming connections will also be nonblocking since   */
//   /* they will inherit that state from the listening socket.   */
//   /*************************************************************/
//   rc = ioctl(listen_sd, FIONBIO, (char *)&on);
//   if (rc < 0)
//   {
//     perror("ioctl() failed");
//     close(listen_sd);
//     exit(-1);
//   }
//
//   /*************************************************************/
//   /* Bind the socket                                           */
//   /*************************************************************/
//   memset(&addr, 0, sizeof(addr));
//   addr.sin6_family      = AF_INET6;
//   memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
//   addr.sin6_port        = htons(SERVER_PORT);
//   rc = bind(listen_sd,
//             (struct sockaddr *)&addr, sizeof(addr));
//   if (rc < 0)
//   {
//     perror("bind() failed");
//     close(listen_sd);
//     exit(-1);
//   }
//
//   /*************************************************************/
//   /* Set the listen back log                                   */
//   /*************************************************************/
//   rc = listen(listen_sd, 32);
//   if (rc < 0)
//   {
//     perror("listen() failed");
//     close(listen_sd);
//     exit(-1);
//   }
//
//   /*************************************************************/
//   /* Initialize the pollfd structure                           */
//   /*************************************************************/
//   memset(fds, 0 , sizeof(fds));
//
//   /*************************************************************/
//   /* Set up the initial listening socket                        */
//   /*************************************************************/
//   fds[0].fd = listen_sd;
//   fds[0].events = POLLIN;
//   /*************************************************************/
//   /* Initialize the timeout to 3 minutes. If no                */
//   /* activity after 3 minutes this program will end.           */
//   /* timeout value is based on milliseconds.                   */
//   /*************************************************************/
//   timeout = (3 * 60 * 1000);
//
//   /*************************************************************/
//   /* Loop waiting for incoming connects or for incoming data   */
//   /* on any of the connected sockets.                          */
//   /*************************************************************/
//   do
//   {
//     /***********************************************************/
//     /* Call poll() and wait 3 minutes for it to complete.      */
//     /***********************************************************/
//     printf("Waiting on poll()...\n");
//     rc = poll(fds, nfds, timeout);
//
//     /***********************************************************/
//     /* Check to see if the poll call failed.                   */
//     /***********************************************************/
//     if (rc < 0)
//     {
//       perror("  poll() failed");
//       break;
//     }
//
//     /***********************************************************/
//     /* Check to see if the 3 minute time out expired.          */
//     /***********************************************************/
//     if (rc == 0)
//     {
//       printf("  poll() timed out.  End program.\n");
//       break;
//     }
//
//
//     /***********************************************************/
//     /* One or more descriptors are readable.  Need to          */
//     /* determine which ones they are.                          */
//     /***********************************************************/
//     current_size = nfds;
//     for (i = 0; i < current_size; i++)
//     {
//       /*********************************************************/
//       /* Loop through to find the descriptors that returned    */
//       /* POLLIN and determine whether it's the listening       */
//       /* or the active connection.                             */
//       /*********************************************************/
//       if(fds[i].revents == 0)
//         continue;
//
//       /*********************************************************/
//       /* If revents is not POLLIN, it's an unexpected result,  */
//       /* log and end the server.                               */
//       /*********************************************************/
//       if(fds[i].revents != POLLIN)
//       {
//         printf("  Error! revents = %d\n", fds[i].revents);
//         end_server = TRUE;
//         break;
//
//       }
//       if (fds[i].fd == listen_sd)
//       {
//         /*******************************************************/
//         /* Listening descriptor is readable.                   */
//         /*******************************************************/
//         printf("  Listening socket is readable\n");
//
//         /*******************************************************/
//         /* Accept all incoming connections that are            */
//         /* queued up on the listening socket before we         */
//         /* loop back and call poll again.                      */
//         /*******************************************************/
//         do
//         {
//           /*****************************************************/
//           /* Accept each incoming connection. If               */
//           /* accept fails with EWOULDBLOCK, then we            */
//           /* have accepted all of them. Any other              */
//           /* failure on accept will cause us to end the        */
//           /* server.                                           */
//           /*****************************************************/
//           new_sd = accept(listen_sd, NULL, NULL);
//           if (new_sd < 0)
//           {
//             if (errno != EWOULDBLOCK)
//             {
//               perror("  accept() failed");
//               end_server = TRUE;
//             }
//             break;
//           }
//
//           /*****************************************************/
//           /* Add the new incoming connection to the            */
//           /* pollfd structure                                  */
//           /*****************************************************/
//           printf("  New incoming connection - %d\n", new_sd);
//           fds[nfds].fd = new_sd;
//           fds[nfds].events = POLLIN;
//           nfds++;
//
//           /*****************************************************/
//           /* Loop back up and accept another incoming          */
//           /* connection                                        */
//           /*****************************************************/
//         } while (new_sd != -1);
//       }
//
//       /*********************************************************/
//       /* This is not the listening socket, therefore an        */
//       /* existing connection must be readable                  */
//       /*********************************************************/
//
//       else
//       {
//         printf("  Descriptor %d is readable\n", fds[i].fd);
//         close_conn = FALSE;
//         /*******************************************************/
//         /* Receive all incoming data on this socket            */
//         /* before we loop back and call poll again.            */
//         /*******************************************************/
//
//         do
//         {
//           /*****************************************************/
//           /* Receive data on this connection until the         */
//           /* recv fails with EWOULDBLOCK. If any other         */
//           /* failure occurs, we will close the                 */
//           /* connection.                                       */
//           /*****************************************************/
//           rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
//           if (rc < 0)
//           {
//             if (errno != EWOULDBLOCK)
//             {
//               perror("  recv() failed");
//               close_conn = TRUE;
//             }
//             break;
//           }
//
//           /*****************************************************/
//           /* Check to see if the connection has been           */
//           /* closed by the client                              */
//           /*****************************************************/
//           if (rc == 0)
//           {
//             printf("  Connection closed\n");
//             close_conn = TRUE;
//             break;
//           }
//
//           /*****************************************************/
//           /* Data was received                                 */
//           /*****************************************************/
//           len = rc;
//           printf("  %d bytes received\n", len);
//
//           /*****************************************************/
//           /* Echo the data back to the client                  */
//           /*****************************************************/
//           rc = send(fds[i].fd, buffer, len, 0);
//           if (rc < 0)
//           {
//             perror("  send() failed");
//             close_conn = TRUE;
//             break;
//           }
//
//         } while(TRUE);
//
//         /*******************************************************/
//         /* If the close_conn flag was turned on, we need       */
//         /* to clean up this active connection. This            */
//         /* clean up process includes removing the              */
//         /* descriptor.                                         */
//         /*******************************************************/
//         if (close_conn)
//         {
//           close(fds[i].fd);
//           fds[i].fd = -1;
//           compress_array = TRUE;
//         }
//
//
//       }  /* End of existing connection is readable             */
//     } /* End of loop through pollable descriptors              */
//
//     /***********************************************************/
//     /* If the compress_array flag was turned on, we need       */
//     /* to squeeze together the array and decrement the number  */
//     /* of file descriptors. We do not need to move back the    */
//     /* events and revents fields because the events will always*/
//     /* be POLLIN in this case, and revents is output.          */
//     /***********************************************************/
//     if (compress_array)
//     {
//       compress_array = FALSE;
//       for (i = 0; i < nfds; i++)
//       {
//         if (fds[i].fd == -1)
//         {
//           for(j = i; j < nfds-1; j++)
//           {
//             fds[j].fd = fds[j+1].fd;
//           }
//           i--;
//           nfds--;
//         }
//       }
//     }
//
//   } while (end_server == FALSE); /* End of serving running.    */
//
//   /*************************************************************/
//   /* Clean up all of the sockets that are open                 */
//   /*************************************************************/
//   for (i = 0; i < nfds; i++)
//   {
//     if(fds[i].fd >= 0)
//       close(fds[i].fd);
//   }
// }

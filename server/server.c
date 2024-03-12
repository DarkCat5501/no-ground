#include <aio.h>
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/ioctls.h>
#include <core.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <thpool.h>
#include <unistd.h>
#include <ws.h>
#include <conn.h>

void handleMessages(Client* client,u8* message,size_t length,void* _wsserver){
  printf("Received message!\n");
}

ConnStatus handleNewConnection(Client* client,void* _wsserver){
  printf("New connection accepted (%d)!\n",client->fd);
  return CONN_KEEP_ALIVE;//accepts the connection
}

ConnStatus handlePacket(Client* client,WSPacket* packet,void* _wsserver){
  printf("New packet received!\n");
  if(packet->header.opcode==WS_OP_CLOSE) return CONN_KILL;

  return CONN_KEEP_ALIVE;//accepts the packet
}

void handleDisconnect(Client* client,void* _wsserver) {
  printf(
    "Client disconected: %s: %d\n",
    inet_ntoa(client->addr.sin_addr),
    htons(client->addr.sin_port)
  );

  //TODO: signal other payers
}

ConnStatus handlePing(Client* client, void* _wsserver){
  printf(
    "Pinging: %s: %d\n",
    inet_ntoa(client->addr.sin_addr),
    htons(client->addr.sin_port)
  );

  return CONN_KEEP_ALIVE;
}

#define TPS 30

int main(int argc, char *argv[]) {
  // initThreadPool();

  Server skserver = {0};
  WSServer wsserver = {
    .bare_server = &skserver,
    .onconnect = handleNewConnection,
    .onpacket = handlePacket,
    .onmessage = handleMessages
  };

  conInitServer(&skserver,(ServerConfig){
    .port=8090,
    .max_connections=10000
  });

  //binds the websocket server to the socket connection
  skserver.data = &wsserver;
  skserver.onconnect = wsHandleConnection;
  skserver.onmessage = wsHandleMessages;
  skserver.ondisconnect = handleDisconnect;
  skserver.onping = handlePing;
  skserver.ping_interval = 10 * TPS;//every 10 ticks of the server

  do {
    conPollEvents(&skserver);
    //TODO: do server work here
    usleep(1000/TPS);// sleep 
  } while (true);

  conDestroyServer(&skserver);
  return 0;
}

// #include <errno.h>
// #include <string.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <openssl/ssl.h>
//
// int main(int argc,char** argv){
//   int sockfd = socket(AF_INET,SOCK_STREAM, 0);
//   struct sockaddr_in addr = {
//     AF_INET,
//     htons(8080),
//     {0}
//   };
//   int err = bind(sockfd,(struct sockaddr*)&addr, sizeof(addr));
//   if(err!=0){
//     printf("error: %s",strerror(errno));
//     exit(1);
//   }
//   err = listen(sockfd, 10);
//   int clientfd = accept(sockfd, NULL, NULL);
//
//   SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
//   SSL* ssl = SSL_new(ctx);
//   SSL_set_fd(ssl, clientfd);
//
//   SSL_use_certificate_chain_file(ssl,"certificate.cert");
//   SSL_use_PrivateKey_file(ssl,"certificate.key",SSL_FILETYPE_PEM);
//   SSL_accept(ssl);
//
//   char buffer[1024] = {0};
//   SSL_read(ssl,buffer,1023);
//
//   char* file_request = buffer + 5;
//   char response[1024] = {0};
//   char* metadata = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
//   memcpy(response,metadata, strlen(metadata));
//   if(strncmp(file_request,"index.html ",11)==0) {
//     FILE* file = fopen("index.html","r");
//     fread(response + strlen(metadata),1024 - strlen(metadata) - 1, 1, file);
//     fclose(file);
//   } else {
//     char* error = "No page found!";
//     memcpy(response+strlen(metadata),error,strlen(error));
//   }
//
//   SSL_write(ssl,response,1024);
//   SSL_shutdown(ssl);
//   return 0;
// }

// int main(int argc,char** argv){
//
//   int sockfd = socket(AF_INET,SOCK_STREAM, 0);
//   struct sockaddr_in addr = {
//     AF_INET,
//     htons(443),
//     {htonl(0x08080808)},
//   };
//
//   int err = connect(sockfd,(struct sockaddr*)&addr,sizeof(addr));
//   SSL_CTX* ctx = SSL_CTX_new(TLS_method());
//   SSL* ssl = SSL_new(ctx);
//   SSL_set_fd(ssl, sockfd);
//   SSL_connect(ssl);
//
//   const char* request = "GET /\r\n\r\n";
//   SSL_write(ssl,request,strlen(request));
//
//   char buffer[1024] = {0};
//   SSL_read(ssl,buffer,1023);
//   printf("response: %s",buffer);
//
//   // int err = bind(sockfd,(struct sockaddr*)&addr, sizeof(addr));
//   // if(err!=0){
//   //   printf("error: %s",strerror(errno));
//   //   exit(1);
//   // }
//   // err = listen(sockfd, 10);
//   // int clientfd = accept(sockfd, NULL, NULL);
//   //
//   //
//   // SSL_use_certificate_chain_file(ssl,"certificate.cert");
//   // SSL_use_PrivateKey_file(ssl,"certificate.key",SSL_FILETYPE_PEM);
//   // SSL_accept(ssl);
//   //
//   // char buffer[1024] = {0};
//   // SSL_read(ssl,buffer,1023);
//   //
//   // char* file_request = buffer + 5;
//   // char response[1024] = {0};
//   // char* metadata = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
//   // memcpy(response,metadata, strlen(metadata));
//   // if(strncmp(file_request,"index.html ",11)==0) {
//   //   FILE* file = fopen("index.html","r");
//   //   fread(response + strlen(metadata),1024 - strlen(metadata) - 1, 1, file);
//   //   fclose(file);
//   // } else {
//   //   char* error = "No page found!";
//   //   memcpy(response+strlen(metadata),error,strlen(error));
//   // }
//   //
//   // SSL_write(ssl,response,1024);
//   // SSL_shutdown(ssl);
//   return 0;
// }

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

void handleMessages(Client* client,const WSPacket packet,void* _wsserver){
  if(packet.header.opcode == WS_OP_TEXT){
    wsSendToAll(_wsserver,packet);
  } else {
    //TODO: handle binary format
  }
}

ConnStatus handleNewConnection(Client* client,void* _wsserver){
  char buffer[1024] = {0};
  i32 len = snprintf(buffer,1023,
    "New client connected: %s: %d\n",
    inet_ntoa(client->addr.sin_addr),
    htons(client->addr.sin_port)
  );

  WSPacket response = {0};
  response.header.opcode = WS_OP_TEXT;
  response.header.FIN = 1;
  response.header.length = len;
  response.length = len;
  response.payload = (u8*)buffer;

  wsSendToAll(_wsserver,response);

  return CONN_KEEP_ALIVE;//accepts the connection
}

ConnStatus handlePacket(Client* client,WSPacket* packet,void* _wsserver){
  // printf("New packet received!\n");
  switch(packet->header.opcode){
    case WS_OP_CLOSE: return CONN_KILL;
    case WS_OP_PONG: client->last_updated = clock(); break;
    case WS_OP_TEXT:case WS_OP_BIN: break;
    default:
      printf("Unknow packet:");
      wsDebugPacket(packet);
      break;
  }

  return CONN_KEEP_ALIVE;//accepts the packet
}

void handleDisconnect(Client* client,void* _wsserver) {
  char buffer[1024] = {0};
  i32 len = snprintf(buffer,1023,
    "Client disconected: %s: %d\n",
    inet_ntoa(client->addr.sin_addr),
    htons(client->addr.sin_port)
  );

  WSPacket response = {0};
  response.header.opcode = WS_OP_TEXT;
  response.header.FIN = 1;
  response.header.length = len;
  response.length = len;
  response.payload = (u8*)buffer;

  wsSendToAll(_wsserver,response);
}

#define TPS 60

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
  skserver.onping = wsHandlePing;
  skserver.ping_interval = 10 * TPS;//every 10 ticks of the server

  do {
    conPollEvents(&skserver);
    //TODO: do server work here
    usleep(1000/TPS);// sleep 
  } while (true);

  conDestroyServer(&skserver);
  return 0;
}



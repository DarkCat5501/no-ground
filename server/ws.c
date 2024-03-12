#include "conn.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <assert.h>
#include <base64.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <ws.h>

i32 wsMakePingPacket(u8 *buffer, size_t size) {
  // create ping header
  WSFrameHeader header = {0};
  header.FIN = 1; // indicates it is just a single packet
  header.RSV = 0;
  header.opcode = WS_OP_PING;
  header.mask = 0;
  header.length = 0;

  memcpy(buffer, &header, sizeof(WSFrameHeader));

  return sizeof(WSFrameHeader);
}

i32 wsReadPacket(u8 *buffer, size_t size, WSPacket *packet) {
  // this is not a valid packet
  if (size < sizeof(WSFrameHeader))
    return 0;

  // read the frame header
  WSFrameHeader header = {0};
  memcpy(&header, buffer, sizeof(WSFrameHeader));

  i32 offset = sizeof(WSFrameHeader);
  u32 length = 0;
  u8 mask_key[sizeof(u32)] = {0}; // the mask used to obfuscate the data

  if (header.length < 126)
    length = header.length & 0x7f;
  else if (header.length == 126) {
    length = htons(*(i16 *)(buffer + offset));
    offset += sizeof(i16);
  } else {
    u32 ulengthh = htonl(*(u32 *)(buffer + offset));
    u32 ulengthl = htonl(*(u32 *)(buffer + offset + sizeof(u32)));
    if (ulengthh != 0 || ulengthl > MAX_WS_PACKET_LENGTH) {
      printf(
          "[WS] Security issue: Data too long to be loaded, exceeded %dMb\n",
          MAX_WS_PACKET_LENGTH / 1024 / 1024
      );

      return 0;
    }
    // converts a net i64 into a host u64
    length = ulengthl;
    offset += sizeof(i64);
  }

  if (header.mask) {
    memcpy(mask_key, buffer + offset, sizeof(u32));
    offset += sizeof(u32);
  }

  // validate if the data size corresponds to the data found
  i32 len = size - offset;
  // if (offset + length > size) {
  //   printf("[WS READ PACKET] Incomplete packet Expected: %d, found: %d\n",
  //          length, len);
  // }

  if (packet != NULL && length != 0) {
    const char *raw_data = (char *)(buffer + offset);
    u8 *payload = malloc(length * sizeof(u8));

    // only decode if all data is present
    if (header.mask && length != 0) {
      for (size_t i = 0; i < len; i++)
        payload[i] = raw_data[i] ^ mask_key[i % sizeof(u32)];
    }
    packet->payload = payload;
  }

  // copy stuff into the packet
  packet->header = header;
  packet->length = length;
  packet->len = len;
  memcpy(&packet->mask_key, mask_key, sizeof(u32));

  return 1;
}

i32 wsDecodePacketData(WSPacket *packet, u8 *buffer, size_t size) {
  if (packet->payload == NULL)
    return 0;
  if (size > packet->length - packet->len) {
    printf("[WS] PACKET DECODE: Invalid buffer length, to big!\n");
    return 0;
  }

  for (size_t i = 0; i < size; i++) {
    packet->payload[packet->len + i] =
        buffer[i] ^ packet->mask_key[(packet->len + i) % sizeof(u32)];
  }
  return 1;
}

void wsDebugPacket(WSPacket *packet) {
  // Show WSPACKET HEADER
  time_t rawtime = time(NULL);
  char *now = ctime(&rawtime);
  now[strlen(now) - 1] = '\0';

  WSFrameHeader header = packet->header;
  printf("[WS] {%s} WSFrameHeader = { FIN=%d, RSV=0x%x, opcode=%d, mask=0x%x, "
         "length=0x%x } {0x%02x};\n",
         now, header.FIN, header.RSV, header.opcode, header.mask, header.length,
         *(u16 *)&header);
  printf("[WS] WSDataLen = %u { patcket: %d }\n", header.length,
         packet->length);
}

void wsPacketFree(WSPacket *packet) {
  if (packet->payload != NULL)
    free(packet->payload);
  packet->length = 0;
  packet->len = 0;
}

static i32 _wsCheckHttpHeaderForSecKey(char *buffer, size_t size,
                                       char *socket_key) {
  char *tk = strtok(buffer, " :\r\n");

  int has_ws_key = 0;
  while (tk != NULL) {
    tk = strtok(NULL, " :\r\n");
    if (tk == NULL)
      break;
    int len = strlen(tk);

    if (has_ws_key) {
      memcpy(socket_key, tk, len); // copy key into output
      return 1;
    }

    if (strncmp("Sec-WebSocket-Key", tk, 17 /*length of Sec-WebSocket-Key*/) ==
        0)
      has_ws_key = 1;
  }

  return 0;
}

static void _wsCalculateAcceptKey(const char *key, char *acceptBuffer,
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

ConnStatus wsHandshake(Client *client) {
  int bytes_read = 0;

  // clear the buffers of the client
  memset(client->rbuffer, 0, sizeof(client->rbuffer));

  // try to receive any messages without blocking
  bytes_read = recv(client->fd, client->rbuffer, sizeof(client->rbuffer),
                    MSG_WAITFORONE);
  if (bytes_read < 0)
    return CONN_RETRY; // could not read the handshake right away
  else if (bytes_read == 0)
    return CONN_KILL; // connection failed

  char ws_key[128] = {0};
  // client must provide handshake key
  int key_ok =
      _wsCheckHttpHeaderForSecKey((char *)client->rbuffer, bytes_read, ws_key);

  if (key_ok) {
    char acceptBuffer[128] = {0}; // calculate handshake key
    _wsCalculateAcceptKey(ws_key, acceptBuffer, sizeof(acceptBuffer));
    char buffer[512] = {0};
    int acceptLen = snprintf(buffer, sizeof(buffer),
                             "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: %s\r\n"
                             "\r\n",
                             acceptBuffer);

    send(client->fd, buffer, acceptLen, MSG_DONTWAIT);
  } else {
    printf("- Handshake: FAILED\n");
    return CONN_KILL;
  }
  printf("- Handshake: OK\n");
  return CONN_KEEP_ALIVE;
}

ConnStatus wsHandleConnection(Client *client, void *srvptr) {
  WSServer *server = (WSServer *)srvptr;

  printf("--{{New client trying connection: %s at %d}}---\n",
         inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));

  int handshake_ok = wsHandshake(client);

  if (handshake_ok == CONN_KEEP_ALIVE) {
    printf("--{{New client connected: %s at %d}}--\n",
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));
    if (server->onconnect)
      return server->onconnect(client, srvptr);
    return CONN_KEEP_ALIVE;
  } else {
    printf("--{{ REJECTED }}--\n");
  }

  return CONN_KILL;
}

ConnStatus wsHandlePing(Client *client, void *_wsserver) {
  WSFrameHeader header = {
      .opcode = WS_OP_PING,
      .RSV = 0,
      .FIN = 1,
      .length = 0,
      .mask = 0,
  };

  int err = send(client->fd, &header, sizeof(header), MSG_DONTWAIT);
  if (err < 0)
    return CONN_KILL;
  client->last_updated = clock();

  return CONN_KEEP_ALIVE;
}

ConnStatus wsHandleMessages(Client *client, void *wsserver) {
  ConnStatus status = CONN_KILL;
  WSServer *server = (WSServer *)wsserver;
  i32 bytes_read = 0;
  memset(client->rbuffer, 0, sizeof(client->rbuffer)); // clear the read buffer
  WSPacket packet = {0};

  // try to read into the buffer
  bytes_read =
      recv(client->fd, client->rbuffer, sizeof(client->rbuffer), MSG_DONTWAIT);
  if (bytes_read <= 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return CONN_KEEP_ALIVE;
    fprintf(stderr, "[WS] Error reading remaining data from client\n");
    return CONN_KILL;
  }

  int read_ok = wsReadPacket(client->rbuffer, bytes_read, &packet);
  if (!read_ok) {
    fprintf(stderr, "[WS] Invalid packet received!\n");
    goto defer;
  }

  // check for incomplete messages and try to pull the rest
  if (packet.length > 0 && packet.len < packet.length) {
    i32 remain_len = packet.length - packet.len;
    u8 *remain_buff = packet.payload + packet.len;
    bytes_read = recv(client->fd, remain_buff, remain_len,
                      MSG_WAITALL); // NOTE: Waitforone does not work

    if (bytes_read != remain_len) {
      fprintf(stderr, "[WS] Invalid remain: read %d, remain %d\n", bytes_read,
              remain_len);
      goto defer;
    }

    // decode the ramin data
    int decode_ok = wsDecodePacketData(&packet, remain_buff, remain_len);
    if (!decode_ok) {
      printf("[WS] Could not decode reading\n");
      goto defer;
    }

    packet.len += bytes_read;
  } else if (packet.header.FIN == 0) {
    // TODO: handle multipacket messages
    assert(0 && "Not implemented yet!");
  }

  // handle packet message
  if (server->onpacket) {
    // handle packet
    status = server->onpacket(client, &packet, wsserver);
    // handle packet message
    if (status == CONN_KEEP_ALIVE &&
        (packet.header.opcode == WS_OP_TEXT ||
         packet.header.opcode == WS_OP_BIN) &&
        server->onmessage)
      server->onmessage(client, packet, wsserver);
  }

defer:
  wsPacketFree(&packet);
  return status;
}


i32 wsSendPacket(Client *client, const WSPacket packet) {
  u8 buffer[WS_MAX_HEADER_LEN] = {0};

  WSFrameHeader header = packet.header;
  header.mask = 0; //ALL SERVER's PACKETs SHOULD BE UNMASKED
  header.FIN = 1; //send message as complete block

  i32 offset = sizeof(WSFrameHeader);
  //handle variable packet size
  if(packet.length < 126) {
    header.length = packet.length;
  } else if(packet.length < SHRT_MAX) {
    u16 length16 = htons((u16)packet.length);
    memcpy(buffer+offset,&length16,sizeof(length16));
    header.length = 126;
    offset+=sizeof(u16);
  } else {
    //NOTE: only encoded low 32bits instead of 64
    u32 lenlow = htonl(packet.length);
    memcpy(buffer+offset+sizeof(u32),&lenlow,sizeof(u32));
    header.length = 127;
    offset+=sizeof(u64);
  }
  //copy into temporary buffer
  memcpy(buffer,&header,sizeof(WSFrameHeader));
 
  //send controll header
  int r = send(client->fd,buffer,offset,MSG_DONTWAIT);
  if(r!=offset) {
    fprintf(stderr,"[WS] Error writing packet header!\n");
    return 1;
  }
  //send message
  r = send(client->fd,packet.payload,packet.length,MSG_DONTWAIT);
  if(r!=packet.length) {
    fprintf(stderr,"[WS] Error writing packet body!\n");
    return 1;
  }
  return 0;
}

i32 wsSendToAll(WSServer *wsserver, const WSPacket packet) {
  if(wsserver->bare_server==NULL) return 0;//failed to get the bare server
 
  Server *bserver = wsserver->bare_server;

  int has_err = 0;
  for(i32 index=0;index<bserver->clients.len;index++){
    Client* client = &bserver->clients.clients[index];
    has_err += wsSendPacket(client,packet);
  }

  return has_err;
} 

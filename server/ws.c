#include "conn.h"
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <assert.h>
#include <base64.h>
#include <errno.h>
#include <netinet/in.h>
#include <sha1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws.h>

int wsMakePingPacket(u8 *buffer, size_t size) {
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

int wsReadPacket(u8 *buffer, size_t size, WSPacket *packet) {
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
    if (ulengthh != 0) {
      printf("[SECURITY]: Data too long to be loaded\n");
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
  int len = size - offset;
  if (offset + length > size) {
    printf("[WS READ PACKET] Incomplete packet Expected: %d, found: %d\n",
           length, len);
  }

  if (packet != NULL && length != 0) {
    const char *raw_data = (char *)(buffer + offset);
    u8 *payload = malloc(length * sizeof(u8));
    // read and deobfuscate packet data
    if (header.mask && length != 0) {
      for (size_t i = 0; i < len; i++)
        payload[i] = raw_data[i] ^ mask_key[i % sizeof(u32)];
    }

    // copy stuff into the packet
    packet->header = header;
    packet->length = length;
    packet->len = len;
    packet->payload = payload;
    memcpy(&packet->mask_key, mask_key, sizeof(u32));
  }

  return 1;
}

int wsDecodePacketData(WSPacket *packet, u8 *buffer, size_t size) {
  if (packet->payload == NULL)
    return 0;
  if (size > packet->length - packet->len) {
    printf("PACKET DECODE: Invalid buffer length, to big!\n");
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
  WSFrameHeader header = packet->header;
  printf("WSFrameHeader = { FIN=%d, RSV=0x%x, opcode=%d, mask=0x%x, "
         "length=0x%x } {0x%02x};\n",
         header.FIN, header.RSV, header.opcode, header.mask, header.length,
         *(u16 *)&header);
  printf("WSDataLen = %u { patcket: %d }\n", header.length, packet->length);
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
    if (has_ws_key) {
      memcpy(socket_key, tk, strlen(tk)); // copy key into output
      return 1;
    }

    if (strcmp("Sec-WebSocket-Key", tk) == 0) has_ws_key = 1;
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
  memset(client->buffer, 0, sizeof(client->buffer));

  // try to receive any messages without blocking
  bytes_read =
      recv(client->fd, client->rbuffer, sizeof(client->rbuffer), MSG_WAITFORONE);
  if (bytes_read < 0) return CONN_RETRY; //could not read the handshake right away
  else if(bytes_read==0) return CONN_KILL; //connection failed

  char ws_key[128] = {0};
  // client must provide handshake key
  int key_ok =
      _wsCheckHttpHeaderForSecKey((char *)client->rbuffer, bytes_read, ws_key);

  if (key_ok) {
    char acceptBuffer[128] = {0}; // calculate handshake key
    _wsCalculateAcceptKey(ws_key, acceptBuffer, sizeof(acceptBuffer));

    int acceptLen = snprintf((char *)client->buffer, sizeof(client->buffer),
                             "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: %s\r\n"
                             "\r\n",
                             acceptBuffer);

    send(client->fd, client->buffer, acceptLen, MSG_DONTWAIT);
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

  if (handshake_ok==CONN_KEEP_ALIVE) {
    printf("--{{New client connected: %s at %d}}--\n",
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));
    if(server->onconnect)
    	return server->onconnect(client,srvptr);
    return CONN_KEEP_ALIVE;
  } else {
  	printf("--{{ REJECTED }}--\n");
  }

  return CONN_KILL;
}

ConnStatus wsHandleMessages(Client *client, void *wsserver) {
	WSServer* server = (WSServer*)wsserver;
  int bytes_read = 0;
  memset(client->rbuffer, 0, sizeof(client->rbuffer));//clear the read buffer
  WSPacket packet = {0};

	//try to read into the buffer
  bytes_read = recv(client->fd, client->rbuffer, sizeof(client->rbuffer), MSG_DONTWAIT);
  if(bytes_read<=0){
  	if(errno==EAGAIN || errno==EWOULDBLOCK) return CONN_KEEP_ALIVE;
  	return CONN_KILL;
  } 


	wsReadPacket(client->rbuffer, bytes_read, &packet);
	wsDebugPacket(&packet);

  //check for incomplete messages and try to pull the rest
	if(packet.length>0 && packet.len < packet.length){
		i32 remain_len = packet.length - packet.len;
		u8* remain_buff = packet.payload+packet.len;
		bytes_read = recv(
			client->fd, 
			remain_buff,
			remain_len,
			MSG_WAITFORONE);

		if(bytes_read != remain_len) return CONN_KILL;

		//decode the ramin data
		int decode_ok = wsDecodePacketData(&packet, remain_buff, remain_len);
		if(!decode_ok) return CONN_KILL;
		
		packet.len += bytes_read;
	} else if (packet.header.FIN == 0) {
		//TODO: handle multipacket messages
		assert(0 && "Not implemented yet!");
	}

	//handle packet message
	if(server->onpacket){
		ConnStatus status = server->onpacket(client,&packet,wsserver);
		if(
			status==CONN_KEEP_ALIVE && 
			(packet.header.opcode == WS_OP_TEXT ||packet.header.opcode == WS_OP_BIN) &&
			server->onmessage
		) server->onmessage(client,packet.payload,packet.len,wsserver);

		return status;
	}

	return CONN_KILL;
}

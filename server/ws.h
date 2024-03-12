#ifndef __WS_H__
#define __WS_H__

#include <conn.h>
#include <core.h>

enum {
	WS_OP_CONTINUATION = 0,
	WS_OP_TEXT,
	WS_OP_BIN,
	WS_OP_CSM_DT_1,//for custom data frames
	WS_OP_CSM_DT_2,
	WS_OP_CSM_DT_3,
	WS_OP_CSM_DT_4,
	WS_OP_CSM_DT_5,
	WS_OP_CLOSE,
	WS_OP_PING,
	WS_OP_PONG,
	WS_OP_CSM_CTRL_1,//for custom control frames
	WS_OP_CSM_CTRL_2,
	WS_OP_CSM_CTRL_3,
	WS_OP_CSM_CTRL_4,
	WS_OP_CSM_CTRL_5,
};

typedef struct {	
	u16 opcode:4; //determines how to interpret the message
	u16 RSV: 3; //reserved bits
	u16 FIN: 1; //indicates whather the frame is the final fragment in the message
	u16 length: 7; //payload length
	u16 mask: 1;   //indicates wheather the payload is masked or not
} WSFrameHeader;

typedef struct {
	WSFrameHeader header; //operation code
	u8 mask_key[sizeof(u32)];
	u32 length; //size of the message
	u32 len;    //current received length of the message
	u8* payload; //the beguining of the data into the buffer
} WSPacket;

typedef struct {
	WSFrameHeader header;
} WSFrame;


typedef void(*OnWSMessage)(Client* client,u8* message, size_t length,void* server);
typedef ConnStatus(*OnWSConnect)(Client* client,void* server);
typedef ConnStatus(*OnWSPacket)(Client* client,WSPacket* packet,void* server);

typedef struct {
	Server* bare_server;
	OnWSConnect onconnect;
	OnWSMessage onmessage;
	OnWSPacket onpacket;
} WSServer;

//populates the buffer with a ping packet
i32 wsMakePingPacket(u8* buffer, size_t size);

// packet management
i32 wsReadPacket(u8* buffer,size_t size, WSPacket* header);
i32 wsDecodePacketData(WSPacket* packet,u8* buffer,size_t size);
void wsPacketFree(WSPacket* packet);

//protocol
ConnStatus wsHandshake(Client* client);

//handlers
ConnStatus wsHandleConnection(Client* client,void* _wsserver);
ConnStatus wsHandleMessages(Client* client,void* _wsserver);
ConnStatus wsHandlePing(Client* client,void* _wsserver);


void wsDebugPacket(WSPacket* packet); 
#endif //__WS_H__

#ifndef __WS_H__
#define __WS_H__

#include <core.h>
#include <string.h>

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
	WSFrameHeader header;
	// u32 masking_key;
} WSFrame;

//populates the buffer with a ping packet
int wsMakePingPacket(u8* buffer, size_t size);

void debugWsPacket(const u8* buffer,size_t size);

#endif //__WS_H__

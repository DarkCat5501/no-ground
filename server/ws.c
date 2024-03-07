#include <string.h>
#include <ws.h>
#include <stdio.h>

int wsMakePingPacket(u8* buffer, size_t size){
	//create ping header
	WSFrameHeader header = {0};
	header.FIN = 1;//indicates it is just a single packet
	header.RSV = 0;
	header.opcode = WS_OP_PING;
	header.mask = 0;
	header.length = 0;

	memcpy(buffer,&header,sizeof(WSFrameHeader));

	return sizeof(WSFrameHeader);
}

void debugWsPacket(const u8* buffer, size_t size) {
	WSFrameHeader header = {0};
	int offset = sizeof(header);
 	memcpy(&header,buffer,sizeof(WSFrameHeader));
 	printf(
 		"WSFrameHeader = { FIN=%d, RSV=0x%x, opcode=%d, mask=0x%x, length=0x%x } {0x%02x};\n",
 		header.FIN,header.RSV,header.opcode,header.mask,header.length,*(u16*)&header
 	);

	u32 length=0;
	u8 mask_key[sizeof(u32)]={0};

 	if(header.length<126){
		length = header.length & 0x7f;
		printf("WSDataLen = %u\n",length);
 	} else {
		// u16 ext_len = *(buffer+sizeof(WSFrameHeader));
 	}

 	if(header.mask){
		memcpy(mask_key,buffer+offset,sizeof(u32));
 		offset+=sizeof(u32);
 		printf("WSFrameMask = '0x%08x'\n",*(u32*)mask_key);
 	}

	char* raw_data = (char*)(buffer+offset);
	char* payload = raw_data;

	if(header.mask){
		for(size_t i=0;i<length;i++){
			payload[i] = raw_data[i] ^ mask_key[i%sizeof(u32)];
		}
	}
	
	int len = size-offset;
 	if(offset+length > size){
 		printf("Invalid packet body: Expected: %d, found: %zu\n",length,size-offset);
 		return;
 	}
 	printf("WSData = {%.*s}(%d)\n",len,payload,len);
}

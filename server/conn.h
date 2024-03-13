#ifndef __NG_CONN_H__
#define __NG_CONN_H__
#include <core.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <time.h>

enum {
	CONN_FAIL, //just for convenience
	CONN_OK, // just for convenience
	CONN_ERR_ALLOC_FAIL,
	CONN_ERR_SOCKET_FAIL,
	CONN_ERR_SETUP_FAIL,
	CONN_ERR_RECV_FAIL,
	CONN_ERR_SEND_FAIL,
};

typedef enum {
	CONN_KILL,
	CONN_KEEP_ALIVE,
	CONN_RETRY,
} ConnStatus;

typedef struct {
	u16 port;
	i32 max_connections;
} ServerConfig;

typedef struct {
	struct sockaddr_in addr;
	i32 fd;
	ConnStatus status;				//status of a connection
	clock_t last_updated; //how long has the client been updated
	u8 rbuffer[1024]; //buffer to receive data
} Client;

typedef struct {
	struct pollfd* items; //holds the currently polling clients
	size_t len; //current length of the polling
	size_t cap; //the maximum capacity of the poll
} ServerPoll;		

typedef struct {
	Client *items; //holds the list of connected clients
	size_t len; //current length of the polling
	size_t cap; //the maximum capacity of the poll
} ClientPoll;

#define MAX_POLLING_CLIENTS 256

typedef ConnStatus(*ConnectCallback)(Client* client,void* data);
typedef ConnStatus(*MessageCallback)(Client* client,void* data);
typedef ConnStatus(*OnPing)(Client* client,void* data);
typedef void(*DisconnectCallback)(Client* client,void* data);



i32 connCreatePoll(ServerPoll* poll, size_t cap);
void connDestroyPoll(ServerPoll* poll);
i32 connPushToPoll(ServerPoll* poll,i32 fd,i16 flags);

int connCreateClientPoll(ClientPoll* poll, size_t cap);
void connDestroyClientPoll(ClientPoll* poll);
i32 connPushToClientPoll(ClientPoll* poll,Client* client);
/**
 * Handles N amount of socket connections
 **/
typedef struct {
	i32 fd;
	ConnectCallback onconnect;//handler for accepting new connections 
	DisconnectCallback ondisconnect;
	MessageCallback onmessage;//handler for new messages
	OnPing onping; //handle sending a ping to every client
	void* data;
	u32 ping_interval; //the delta in ms to send the ping to client
	ServerPoll poll;
	ClientPoll clients;
} Server;

i32 conInitServer(Server* server,const ServerConfig config);
i32 conAcceptConnections(Server* server);
i32 conHandleReceivData(Server* server,Client* client);
i32 conPollEvents(Server* server);

i32 conHasEvents(Server* server);
void conDestroyServer(Server* server);

/** Safely send a message to a client **/
i32 conSendMessage(const Client* client,const u8* message,size_t len);

/** Safely read a message from a client waiting for all content to be finished **/
i32 conRecvMessage(const Client* client,u8* message,size_t len);
/** Safely read a message from a client nonblocking **/
i32 conCheckRecvMessage(const Client* client,u8* message,size_t len);

#endif //__NG_CONN_H__

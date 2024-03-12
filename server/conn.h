#ifndef __NG_CONN_H__
#define __NG_CONN_H__
#include <core.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <time.h>

enum {
	CONN_OK,
	CONN_ERR_ALLOC_FAIL,
	CONN_ERR_SOCKET_FAIL,
	CONN_ERR_SETUP_FAIL,
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


#define MAX_POLLING_CLIENTS 256

typedef ConnStatus(*ConnectCallback)(Client* client,void* data);
typedef ConnStatus(*MessageCallback)(Client* client,void* data);
typedef ConnStatus(*OnPing)(Client* client,void* data);
typedef void(*DisconnectCallback)(Client* client,void* data);


typedef struct {
	struct pollfd* fds; //holds the currently polling clients
	size_t len; //current length of the polling
	size_t cap; //the maximum capacity of the poll
} ServerPoll;		

typedef struct {
	Client *clients; //holds the list of connected clients
	size_t len; //current length of the polling
	size_t cap; //the maximum capacity of the poll
} ClientPoll;

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
i32 conCheckConnections(Server* server);
i32 conHandleReceivData(Server* server,i32 clientIndex);
i32 conPollEvents(Server* server);

i32 conHasEvents(Server* server);
void conDestroyServer(Server* server);

#endif //__NG_CONN_H__

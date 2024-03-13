#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <core.h>
#include <conn.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>


/** 'Safe' abstractions **/
i32 conSendMessage(const Client *client, const u8 *message, size_t len) {
	//check if the client file descriptor if valid
	int fd_ok = fcntl(client->fd,F_GETFD);
	if(fd_ok < 0){ 
		fprintf(stderr,"[CONN] Invalid client: %x!\n",fd_ok);
		return 0;//invalid client!
	}
	int sent = send(client->fd,message,len, MSG_NOSIGNAL);
	if(sent == 0){
		fprintf(stderr,"[CONN] Error during sending: sent %d!\n",sent);
		return -1;//error sending file
	}
	return sent;
}
i32 conRecvMessage(const Client *client,u8 *message, size_t len) {
	//check if the client file descriptor if valid
	int fd_ok = fcntl(client->fd,F_GETFD);
	if(fd_ok < 0){ 
		fprintf(stderr,"[CONN] Invalid client: %x!\n",fd_ok);
		return 0;//invalid client!
	}
	int recved = recv(client->fd,(void*)message,len, MSG_WAITFORONE|MSG_NOSIGNAL);
	if(recved == 0){
		fprintf(stderr,"[CONN] Error during receving: recved %d!\n",recved);
		return 0;//error sending file
	}
	return recved;
}
i32 conCheckRecvMessage(const Client *client, u8 *message, size_t len){
	//check if the client file descriptor if valid
	int fd_ok = fcntl(client->fd,F_GETFD);
	if(fd_ok < 0){ 
		fprintf(stderr,"[CONN] Invalid client: %x!\n",fd_ok);
		return 0;//invalid client!
	}
	int recved = recv(client->fd,(void*)message,len, MSG_DONTWAIT|MSG_NOSIGNAL);
	if(recved == 0){
		fprintf(stderr,"[CONN] Error during receving: recved %d!\n",recved);
		return 0;//error sending file
	}
	return recved;
}

/* Poll related functions */
i32 connCreatePoll(ServerPoll *poll, size_t cap){
	poll->items = calloc(cap,sizeof(struct pollfd));
	if(poll->items==NULL) return CONN_ERR_ALLOC_FAIL;
	poll->len = 0; poll->cap = cap;
	return CONN_OK;
}

void connDestroyPoll(ServerPoll *poll){
	if(poll->items) free(poll->items);
	poll->cap = 0; poll->len = 0;
}

i32 connPushToPoll(ServerPoll *poll, i32 fd, i16 events){
	if(poll->len>=poll->cap) return -1;

	i32 crr = poll->len++;
	poll->items[crr].fd = fd;
	poll->items[crr].events = events;
	poll->items[crr].revents = 0x0;

	return crr;
}

i32 connCreateClientPoll(ClientPoll *poll, size_t cap){
	poll->cap = cap;
	poll->items = calloc(cap,sizeof(Client));
	if(poll->items == NULL) return CONN_ERR_ALLOC_FAIL;
	poll->len = 0;
	return CONN_OK;
}

void connDestroyClientPoll(ClientPoll *poll){
	poll->cap = 0;
	if(poll->items) free(poll->items);
	poll->len = 0;
}

i32 connPushToClientPoll(ClientPoll *poll,Client* client){
	if(poll->len>=poll->cap) return -1;
	i32 crr = poll->len++;
	memcpy(&poll->items[crr],client,sizeof(Client));

	return crr;
}

/* server related functions */
i32 conInitServer(Server *server,const ServerConfig config){
  if ((server->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
  	return CONN_ERR_SOCKET_FAIL; //server has failed to acquire socket
  }

  struct sockaddr_in serveraddr = {0};
  serveraddr.sin_family = AF_INET; //ipv4
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(config.port);

	if(connCreatePoll(&server->poll, config.max_connections) != CONN_OK){
		return CONN_ERR_ALLOC_FAIL;
	}

	if(connCreateClientPoll(&server->clients,config.max_connections-1) != CONN_OK){
		return CONN_ERR_ALLOC_FAIL;
	}
	// setup server to reuse address
  i32 on = 1;
  if(setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR,(char *)&on, sizeof(on))){
  	return CONN_ERR_SETUP_FAIL;
  }
	// trying to setup teh server as non-blocking
  i32 setup_non_blocking = ioctl(server->fd, FIONBIO, (char *)&on);
  if (setup_non_blocking < 0) {
    fprintf(stderr,"[CONN]: Unable to set socket non-blocking\n");
  }
	// try to bind the server to the socket
  if (bind(server->fd, (struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) {
    return CONN_ERR_SETUP_FAIL;
  }

  // start listening for connections
  if (listen(server->fd, config.max_connections) < 0) {
  	return CONN_ERR_SETUP_FAIL;
  }

  // setup initial server to be always handled 
  connPushToPoll(&server->poll,server->fd,POLLIN);
	return 0;
}

i32 conHasEvents(Server *server){ 
  i32 timeout = 0; 
  i32 res = poll(server->poll.items, server->poll.len, timeout);

	return res;
}

static ConnStatus conHandleConnect(Server* server,Client* client){
	if(server->onconnect){
		ConnStatus status = server->onconnect(client,server->data);
		if(status!=CONN_KILL) {
			printf("New client in!\n");
			client->status=status;
			client->last_updated = clock();
			connPushToClientPoll(&server->clients,client);
			connPushToPoll(&server->poll,client->fd, POLLIN);
			return status;
		}
	}
	return CONN_KILL;
}

i32 conAcceptConnections(Server* server){
	struct pollfd listener = server->poll.items[0];
	int err = 0;
  if (listener.revents & POLLERR){
    err = listener.revents;
    goto defer;
  } else if (listener.revents & POLLHUP || listener.revents & POLLNVAL){
  	err = listener.revents;
    goto defer;
  } else if(listener.revents & POLLIN) {
		Client client;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		client.fd = accept(server->fd, (struct sockaddr*)&client.addr,&addr_len);

		if(client.fd < 0) {
			if(errno == EWOULDBLOCK || errno == EAGAIN) goto defer;
			else goto defer;
		} else {
			if(conHandleConnect(server, &client)==CONN_KILL)
				close(client.fd);
		}
	}

	defer:
	return err;
}

i32 conHandleReceivData(Server *server, Client* client){	
	client->last_updated = clock();//updates client clock

	if(server->onmessage){
		return server->onmessage(client,server->data);
	} else {
		//clear the recv buffer
		char discard_buffer[1024] = {0};
		while(
			recv(client->fd,discard_buffer,1024,MSG_DONTWAIT|MSG_NOSIGNAL)>0
		){}
	}
	return CONN_KILL;
}

i32 conPollEvents(Server *server){
	i32 num_events = conHasEvents(server);
	if(num_events>0){
		//loop through every connection trying to solve the event
		//check for server new connections
		int err = 0;
		if((err = conAcceptConnections(server)!=0)){
			fprintf(stderr,"And erros has ocurred on connection listener: %d\n",err);
			exit(1);
		}

		//check for client messages
		for(i32 index = 1;index<server->poll.len;index++){
			struct pollfd *pfd = &server->poll.items[index];
			Client *client = &server->clients.items[index-1];

			//check if cliend file descriptor is still alive
			char peek_buff[1]  ={0};
			int peek = recv(pfd->fd,&peek_buff,1,MSG_DONTWAIT|MSG_PEEK|MSG_NOSIGNAL);
			
			if(peek==0){
				printf("Client disconnected (%d)!\n",pfd->fd);
				pfd->events = 0x0;//marks as delete
			} else if (pfd->revents & POLLERR || pfd->revents & POLLNVAL) {
    		printf("Client err (%d)!\n",pfd->fd);
    		pfd->events = 0x0;//marks as delete
    	} else if (pfd->revents & POLLHUP) {
    		printf("Client pollhup\n");
    		pfd->events = 0x0;//marks as delete
    	} else if (pfd->revents & POLLIN && peek>0) {
    		if(client->status == CONN_RETRY){
    			if(conHandleConnect(server,client))
    				pfd->events =0x0;//marks as dele
    		}
    		if(server->onmessage!=NULL){
    			client->last_updated = 0;//resets the last updated counter
    			if(conHandleReceivData(server,client)==CONN_KILL)
						pfd->events = 0x0;//markes as delete 
    		}
  		} else {
  			//for sure the client is still alive!
  			client->last_updated = clock();
  		}
		}
	} else if(server->onping) {
		// printf("Sending pings:\n");
		//loops over every client trying to
		for(i32 index =1;index<server->poll.len;index++){
			struct pollfd *pfd = &server->poll.items[index];
			Client *client = &server->clients.items[index-1];

			clock_t now = clock();
			clock_t delta = now - client->last_updated;

			// printf("delta clock: %ld\n",delta);
			//CLOCKS_PER_SEC
			if( (delta/1000) >= server->ping_interval){
				ConnStatus status = server->onping(client,server->data);
				client->status = status;
				if(status==CONN_KILL)
					pfd->events = 0x0;//marks as delete
				client->last_updated = now;
			}
		}
	}


	int deleted = 0;
	//loop over clients checking who is still alive
	for(i32 index = 1;index<server->poll.len;){
		//swap client with the last
		struct pollfd *pfd = &server->poll.items[index];
		Client *client = &server->clients.items[index-1];

		if(pfd->events==0x0 || client->status==CONN_KILL){
			printf("Deleting connection!\n");
			struct pollfd *last = &server->poll.items[server->poll.len-1];
			Client *current = &server->clients.items[index-1];
			Client *last_client = &server->clients.items[server->clients.len-1];

			//call disconnect callback
			if(server->ondisconnect) server->ondisconnect(current,server->data);

			close(pfd->fd);//closes the connection
			
			//swaps current with last element
			memcpy(pfd,last,sizeof(struct pollfd));
			memcpy(current,last_client,sizeof(Client));

			//decrease lengths
			server->poll.len-=1;
			server->clients.len-=1;
			deleted++;
		} else index++;//goto next
	}
	
	if(deleted){
		printf("SERVER: >> %ld connections alive\n", server->poll.len-1);
	}

	return 0;
}

void conDestroyServer(Server *server){
	//TODO: properly destroy server data
}

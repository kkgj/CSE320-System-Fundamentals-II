#include <time.h>
#include "server.h"
#include "client_registry.h"
#include "protocol.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "debug.h"
#include "csapp.h"
#include "debug.h"

// extern CLIENT_REGISTRY *client_registry; include gloabl

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives packets from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  It also maintains information about whether
 * the client has logged in or not.  Until the client has logged in,
 * only LOGIN packets will be honored.  Once a client has logged in,
 * LOGIN packets will no longer be honored, but other packets will be.
 * The service loop ends when the network connection shuts down and
 * EOF is seen.  This could occur either as a result of the client
 * explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *jeux_client_service(void *arg) {
	int fd = *((int*)arg);
	Free(arg);
	Pthread_detach(Pthread_self());
	CLIENT *client = creg_register(client_registry, fd);
	int isLogin = 0;

	while (1) {
		JEUX_PACKET_HEADER	*receiveHeader = Malloc(sizeof(JEUX_PACKET_HEADER));
        JEUX_PACKET_HEADER	*sendHeader = Malloc(sizeof(JEUX_PACKET_HEADER));
        void **datap = Malloc(sizeof(void**));
        int receiveSuccess =  proto_recv_packet(fd,receiveHeader,datap);
        if(receiveSuccess < 0){
        	Free(receiveHeader);
            Free(sendHeader);
            Free(datap);
            datap = NULL;
            break;
        }
        if (receiveHeader->type == JEUX_LOGIN_PKT) {
        	// *datap = Realloc(*datap, receiveHeader->size + 1);
        	// ((char*)*datap)[receiveHeader->size] = '\0';
        	PLAYER *player = player_create(*datap);
        	if (player != NULL && client_login(client, player) == 0) {
        		isLogin = 1;
        		client_send_ack(client, NULL, 0);
        	} else {
        		client_send_nack(client);
        	}
        } else if (isLogin == 0) {
        	client_send_nack(client);
        } else if (receiveHeader->type == JEUX_USERS_PKT) {
        	PLAYER **players = creg_all_players(client_registry);
        	PLAYER **temp = players;
        	size_t size = 0;
        	int i = 0;
        	while (*temp != NULL) {
        		size += sizeof(player_get_name(players[i])) + 7;
        		temp++;
        	}
        	char *datap = Calloc(size, sizeof(char));
        	char fmtbuf[30];
        	while (*players != NULL) {
        		sprintf(fmtbuf, "%s\t%d\n", player_get_name(*players), player_get_rating(*players));
        		strcat(datap, fmtbuf);
        		players++;
        	}
        	//debug("%s", buffer);
        	struct timespec time;
            clock_gettime(CLOCK_REALTIME, &time);
            sendHeader->timestamp_sec = htonl(time.tv_sec);
            sendHeader->timestamp_nsec = htonl(time.tv_nsec);
        	sendHeader->size = htons(size);
        	sendHeader->type = JEUX_ACK_PKT;
        	sendHeader->id = 0;
        	sendHeader->role = 0;
        	client_send_packet(client, sendHeader, datap);
        } else if (receiveHeader->type == JEUX_INVITE_PKT) {
        	CLIENT *target = creg_lookup(client_registry, *datap);
        	GAME_ROLE source_role = receiveHeader->role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
        	int id = client_make_invitation(client, target, source_role, receiveHeader->role);
        	if (id == -1) { // Failed
        		client_send_nack(client);
        	} else {
        		struct timespec time;
	            clock_gettime(CLOCK_REALTIME, &time);
	            sendHeader->timestamp_sec = htonl(time.tv_sec);
	            sendHeader->timestamp_nsec = htonl(time.tv_nsec);
	        	sendHeader->size = 0;
	        	sendHeader->type = JEUX_ACK_PKT;
	        	sendHeader->id = id;
	        	sendHeader->role = 0;
	        	client_send_packet(client, sendHeader, datap);
        	}
        } else if (receiveHeader->type == JEUX_REVOKE_PKT) {
        	if (client_revoke_invitation(client, receiveHeader->id)) { //Fails
        		client_send_nack(client);
        	} else {
        		client_send_ack(client,NULL,0);
        		// struct timespec time;
	         //    clock_gettime(CLOCK_REALTIME, &time);
	         //    sendHeader->timestamp_sec = htonl(time.tv_sec);
	         //    sendHeader->timestamp_nsec = htonl(time.tv_nsec);
	        	// sendHeader->size = 0;
	        	// sendHeader->type = JEUX_REVOKED_PKT;
	        	// sendHeader->id = 0;
	        	// sendHeader->role = 0;
	        	// client_send_packet(client, sendHeader, NULL);
        	}
        } else if (receiveHeader->type == JEUX_ACCEPT_PKT) {
        	char *state = NULL;
        	if (client_accept_invitation(client, receiveHeader->id, &state)) { //Fails
        		client_send_nack(client);
        	} else {
        		if (state == NULL) {
        			client_send_ack(client, NULL, 0);
        		} else {
        			struct timespec time;
		            clock_gettime(CLOCK_REALTIME, &time);
		            sendHeader->timestamp_sec = htonl(time.tv_sec);
		            sendHeader->timestamp_nsec = htonl(time.tv_nsec);
		        	sendHeader->size = htons(strlen(state));
		        	sendHeader->type = JEUX_ACK_PKT;
		        	sendHeader->id = 0;
		        	sendHeader->role = 0;
		        	client_send_packet(client, sendHeader, state);
		        	Free(state);
        		}
        	}
        } else if (receiveHeader->type == JEUX_DECLINE_PKT) {
        	if (client_decline_invitation(client, receiveHeader->id)) { //Fails
        		client_send_nack(client);
        	} else {
        		client_send_ack(client,NULL,0);
        	}
        } else if (receiveHeader->type == JEUX_MOVE_PKT) {
        	if (client_make_move(client, receiveHeader->id, *datap)) { //Fails
        		client_send_nack(client);
        	} else {
        		client_send_ack(client,NULL,0);
        	}
        } else if (receiveHeader->type == JEUX_RESIGN_PKT) {
        	if (client_resign_game(client, receiveHeader->id)) { //Fails
        		client_send_nack(client);
        	} else {
        		client_send_ack(client,NULL,0);
        	}
        } else {
        	client_send_nack(client);
        }
        if (sendHeader != NULL) {
        	Free(sendHeader);
        }
        if (receiveHeader != NULL) {
        	Free(receiveHeader);
        }
        Free(datap);
        datap = NULL;
	}
	creg_unregister(client_registry, client);
	Close(fd);
	return NULL;
}

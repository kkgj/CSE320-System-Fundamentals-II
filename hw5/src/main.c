#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#include "csapp.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);

void handler_sighup() {
    terminate(EXIT_SUCCESS);
}

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    int option;
    while((option = getopt(argc, argv, "p:")) != -1) { //"p:h::d"
        switch(option){
            case 'p':
                if(argc != 3) {
                    printf("%s\n", "Usage: bin/jeux -p <port>");
                    exit(EXIT_FAILURE);
                }
                break;
            // case 'h':
            //     if(argc < 4) {
            //         printf("%s\n", "Usage: bin/jeux -p <port>");
            //         exit(EXIT_FAILURE);
            //     }
            //     break;
            // case 'd':
            //     if(argc < 4) {
            //         printf("%s\n", "Usage: bin/jeux -p <port>");
            //         exit(EXIT_FAILURE);
            //     }
            //     break;
            default:
                printf("%s\n", "Usage: bin/jeux -p <port>");
                exit(EXIT_FAILURE);
        }
    }
    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    Signal(SIGHUP, handler_sighup);

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(argv[2]);
    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd,(SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, jeux_client_service, connfdp);
    }

    // fprintf(stderr, "You have to finish implementing main() "
	   //  "before the Jeux server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}

#include "client_registry.h"
#include "csapp.h"
#include "debug.h"


// The CLIENT_REGISTRY type is a structure that defines the state of a
// client registry.
typedef struct client_registry {
	sem_t mutex; //mutex
	int count;
	sem_t wait;
    CLIENT* client_array[MAX_CLIENTS]; // client array
} CLIENT_REGISTRY;


/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init() {
	CLIENT_REGISTRY* cr = Malloc(sizeof(CLIENT_REGISTRY)); //initialize the client_registry object
	if (cr == NULL) {
		return NULL;
	}
	cr->count = 0;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        cr->client_array[i] = NULL;
    }
    Sem_init(&cr->mutex, 0, 1); //intialize semaphore
    Sem_init(&cr->wait, 0, 1);
	return cr;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr) {
	for(int i = 0; i < MAX_CLIENTS; i++) {
        if (cr->client_array[i] != NULL) {
        	Free(cr->client_array[i]);
        	cr->client_array[i] = NULL;
        }
    }
	Free(cr);
	cr = NULL;
}

/*
 * Register a client file descriptor.
 * If successful, returns a reference to the the newly registered CLIENT,
 * otherwise NULL.  The returned CLIENT has a reference count of one;
 * this corresponds to the reference held by the registry itself for as
 * long as the client remains connected.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return a reference to the newly registered CLIENT, if registration
 * is successful, otherwise NULL.
 */
CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
	P(&cr->mutex);
	if (cr->count == MAX_CLIENTS) {
		V(&cr->mutex);
		return NULL;
	}
	CLIENT *client = client_create(cr, fd);
	cr->client_array[fd] = client;
	cr->count++;
	if (cr->count == 1) {
		P(&cr->wait);
	}
	V(&cr->mutex);
	return client;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
	P(&cr->mutex);
	int fd = client_get_fd(client);
	if (cr->client_array[fd] == NULL) {
		V(&cr->mutex);
		return -1;
	}
    client_unref(client, "Unregister");
    cr->client_array[fd] = NULL;
    cr->count--;
    if (cr->count == 0) {
		V(&cr->wait);
	}
    V(&cr->mutex);
    return 0; //if unregistration succeeds
}

/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
	P(&cr->mutex);
	if (cr == NULL || user == NULL) {
		V(&cr->mutex);
		return NULL;
	}
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (cr->client_array[i] != NULL) {
			PLAYER *player = client_get_player(cr->client_array[i]);
			if (player == NULL) {
				V(&cr->mutex);
				return NULL;
			}
			if (strcmp(player_get_name(player), user) == 0) {
				CLIENT *client_return = client_ref(cr->client_array[i], "creg_lookup");
				V(&cr->mutex);
				return client_return;
			}
		}
	}
	V(&cr->mutex);
	return NULL;
}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of players is to be
 * obtained.
 * @return the list of players.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
	P(&cr->mutex);
	PLAYER **players_pointer = Malloc(cr->count * sizeof(PLAYER*) + 1);
	int position = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (cr->client_array[i] != NULL) {
			PLAYER *player = client_get_player(cr->client_array[i]);
			if (player != NULL) {
				players_pointer[position] = player_ref(player, "creg_all_players");
				position++;
			}
		}
	}
	players_pointer[cr->count] = NULL;
	V(&cr->mutex);
	return players_pointer;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
	P(&cr->wait);
	V(&cr->wait);
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr) {
	P(&cr->mutex);
	for(int i = 0; i < MAX_CLIENTS; i++) {
        if(cr->client_array[i] != NULL) {
            shutdown(client_get_fd(cr->client_array[i]), SHUT_RD);
        }
    }
    V(&cr->mutex);
}

#include "player_registry.h"
#include "csapp.h"
#include "debug.h"

/*
 * A player registry maintains a mapping from usernames to PLAYER objects.
 * Entries persist for as long as the server is running.
 */

/*
 * The PLAYER_REGISTRY type is a structure type that defines the state
 * of a player registry.  You will have to give a complete structure
 * definition in player_registry.c. The precise contents are up to
 * you.  Be sure that all the operations that might be called
 * concurrently are thread-safe.
 */
typedef struct player_registry {
	sem_t mutex; //mutex
	int count;
    PLAYER* player_array[1000]; // client array
} PLAYER_REGISTRY;

/*
 * Initialize a new player registry.
 *
 * @return the newly initialized PLAYER_REGISTRY, or NULL if initialization
 * fails.
 */
PLAYER_REGISTRY *preg_init(void) {
	PLAYER_REGISTRY *player_registry = Malloc(sizeof(PLAYER_REGISTRY));
	if (player_registry == NULL) {
		return NULL;
	}
	player_registry->count = 0;
    for(int i = 0; i < 1000; i++) {
        player_registry->player_array[i] = NULL;
    }
    Sem_init(&player_registry->mutex, 0, 1); //intialize semaphore
	return player_registry;
}

/*
 * Finalize a player registry, freeing all associated resources.
 *
 * @param cr  The PLAYER_REGISTRY to be finalized, which must not
 * be referenced again.
 */
void preg_fini(PLAYER_REGISTRY *preg) {
	for(int i = 0; i < 1000; i++) {
        if (preg->player_array[i] != NULL) {
        	Free(preg->player_array[i]);
        	preg->player_array[i] = NULL;
        }
    }
	Free(preg);
	preg = NULL;
}

/*
 * Register a player with a specified user name.  If there is already
 * a player registered under that user name, then the existing registered
 * player is returned, otherwise a new player is created.
 * If an existing player is returned, then its reference count is increased
 * by one to account for the returned pointer.  If a new player is
 * created, then the returned player has reference count equal to two:
 * one count for the pointer retained by the registry and one count for
 * the pointer returned to the caller.
 *
 * @param name  The player's user name, which is copied by this function.
 * @return A pointer to a PLAYER object, in case of success, otherwise NULL.
 *
 */
PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
	P(&preg->mutex);
	for (int i = 0; i < 1000; i++) { // already exists
		if (preg->player_array[i] != NULL) {
			if (strcmp(player_get_name(preg->player_array[i]), name) == 0) {
				PLAYER *player = player_ref(preg->player_array[i], "preg_register");
				V(&preg->mutex);
				return player;
			}
		}
	}
	// Create a new player
	PLAYER *player = player_create(name);
	if (player == NULL) {
		V(&preg->mutex);
		return NULL;
	}
	preg->player_array[preg->count] = player_ref(player, "preg_register");
	player_ref(player, "preg_register");
	preg->count++;
	V(&preg->mutex);
	return player;
}
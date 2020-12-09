#include "client_registry.h"
#include "invitation.h"
#include "csapp.h"
#include "debug.h"

/*
 * The INVITATION type is a structure type that defines the state of
 * an invitation.  You will have to give a complete structure
 * definition in invitation.c.  The precise contents are up to you.
 * Be sure that all the operations that might be called concurrently
 * are thread-safe.
 */
typedef struct invitation {
	INVITATION_STATE state;
	CLIENT *source;
	CLIENT *target;
	GAME *game;
	GAME_ROLE source_role;
	GAME_ROLE target_role;
	int reference_count;
	sem_t mutex;
} INVITATION;

/*
 * Create an INVITATION in the OPEN state, containing reference to
 * specified source and target CLIENTs, which cannot be the same CLIENT.
 * The reference counts of the source and target are incremented to reflect
 * the stored references.
 *
 * @param source  The CLIENT that is the source of this INVITATION.
 * @param target  The CLIENT that is the target of this INVITATION.
 * @param source_role  The GAME_ROLE to be played by the source of this INVITATION.
 * @param target_role  The GAME_ROLE to be played by the target of this INVITATION.
 * @return a reference to the newly created INVITATION, if initialization
 * was successful, otherwise NULL.
 */
INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
	INVITATION *invitation = Malloc(sizeof(INVITATION));
	if (invitation == NULL || source == target) {
		return NULL;
	}
	invitation->state = INV_OPEN_STATE;
	invitation->source = client_ref(source, "inv_create");
	invitation->target = client_ref(target, "inv_create");
	invitation->source_role = source_role;
	invitation->target_role = target_role;
	invitation->reference_count = 1;
	invitation->game = NULL;
	Sem_init(&invitation->mutex, 0, 1);
	return invitation;
}

/*
 * Increase the reference count on an invitation by one.
 *
 * @param inv  The INVITATION whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same INVITATION object that was passed as a parameter.
 */
INVITATION *inv_ref(INVITATION *inv, char *why) {
	P(&inv->mutex);
	inv->reference_count++;
	debug("%s", why);
	V(&inv->mutex);
	return inv;
}

/*
 * Decrease the reference count on an invitation by one.
 * If after decrementing, the reference count has reached zero, then the
 * invitation and its contents are freed.
 *
 * @param inv  The INVITATION whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 */
void inv_unref(INVITATION *inv, char *why) {
	P(&inv->mutex);
	inv->reference_count--;
	if (inv->reference_count == 0) {
		client_unref(inv->source, "because invitation is being freed");
		client_unref(inv->target, "because invitation is being freed");
		// game_unref(inv->game, "because invitation is being freed");
		V(&inv->mutex);
		Free(inv);
		inv = NULL;
		return;
	}
	debug("%s", why);
	V(&inv->mutex);
}

/*
 * Get the CLIENT that is the source of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid as
 * long as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the source of the INVITATION.
 */
CLIENT *inv_get_source(INVITATION *inv) {
	P(&inv->mutex);
	CLIENT *client = inv->source;
	V(&inv->mutex);
	return client;
}

/*
 * Get the CLIENT that is the target of an INVITATION.
 * The reference count of the returned CLIENT is NOT incremented,
 * so the CLIENT reference should only be regarded as valid if
 * the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the CLIENT that is the target of the INVITATION.
 */
CLIENT *inv_get_target(INVITATION *inv) {
	P(&inv->mutex);
	CLIENT *client = inv->target;
	V(&inv->mutex);
	return client;
}

/*
 * Get the GAME_ROLE to be played by the source of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the source of the INVITATION.
 */
GAME_ROLE inv_get_source_role(INVITATION *inv) {
	P(&inv->mutex);
	GAME_ROLE source_role = inv->source_role;
	V(&inv->mutex);
	return source_role;
}

/*
 * Get the GAME_ROLE to be played by the target of an INVITATION.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME_ROLE played by the target of the INVITATION.
 */
GAME_ROLE inv_get_target_role(INVITATION *inv) {
	P(&inv->mutex);
	GAME_ROLE target_role = inv->target_role;
	V(&inv->mutex);
	return target_role;
}

/*
 * Get the GAME (if any) associated with an INVITATION.
 * The reference count of the returned GAME is NOT incremented,
 * so the GAME reference should only be regarded as valid as long
 * as the INVITATION has not been freed.
 *
 * @param inv  The INVITATION to be queried.
 * @return the GAME associated with the INVITATION, if there is one,
 * otherwise NULL.
 */
GAME *inv_get_game(INVITATION *inv) {
	P(&inv->mutex);
	GAME *game = inv->game;
	V(&inv->mutex);
	return game;
}

/*
 * Accept an INVITATION, changing it from the OPEN to the
 * ACCEPTED state, and creating a new GAME.  If the INVITATION was
 * not previously in the the OPEN state then it is an error.
 *
 * @param inv  The INVITATION to be accepted.
 * @return 0 if the INVITATION was successfully accepted, otherwise -1.
 */
int inv_accept(INVITATION *inv) {
	P(&inv->mutex);
	if (inv->state != INV_OPEN_STATE) {
		V(&inv->mutex);
		return -1;
	}
	inv->state = INV_ACCEPTED_STATE;
	inv->game = game_create();
	//game_ref(inv->game, "inv_accept");
	V(&inv->mutex);
	return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role) {
	P(&inv->mutex);
	if (inv->state != INV_OPEN_STATE && inv-> state != INV_ACCEPTED_STATE) {
		V(&inv->mutex);
		return -1;
	}
	if (inv->game == NULL || game_is_over(inv->game)) {
		inv->state = INV_CLOSED_STATE;
		V(&inv->mutex);
		return 0;
	}
	if (role != NULL_ROLE) {
		inv->state = INV_CLOSED_STATE;
		game_resign(inv->game, role);
		V(&inv->mutex);
		return 0;
	}
	V(&inv->mutex);
	return -1;
}

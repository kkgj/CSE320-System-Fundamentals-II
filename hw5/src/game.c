#include "game.h"
#include "player.h"
#include "csapp.h"
#include "debug.h"

/*
 * A GAME represents the current state of a game between participating
 * players.  So that a GAME object can be passed around
 * without fear of dangling references, it has a reference count that
 * corresponds to the number of references that exist to the object.
 * A GAME object will not be freed until its reference count reaches zero.
 */

/*
 * The GAME type is a structure type that defines the state of a game.
 * You will have to give a complete structure definition in game.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct game {
	int chosen;
	GAME_ROLE role;
	GAME_ROLE game_array[9];
	int reference;
	sem_t mutex;
	int over;
} GAME;

/*
 * The GAME_MOVE type is a structure type that defines a move in a game.
 * The details are up to you.  A GAME_MOVE is immutable.
 */
typedef struct game_move {
	int position;
	GAME_ROLE game_role;
	sem_t mutex;
} GAME_MOVE;

/*
 * The GAME_ROLE type is an enumeration type whose value identify the
 * possible roles of players in a game.  For this assignment, we are
 * considering only two-player games with alternating moves, in which
 * the player roles are distinguished by which player is the first to
 * move.  The constant NULL_ROLE does not refer to a player role.  It
 * is used when it is convenient to have a sentinel value of the
 * GAME_ROLE type.
 */
// typedef enum game_role {
//     NULL_ROLE,
//     FIRST_PLAYER_ROLE,
//     SECOND_PLAYER_ROLE
// } GAME_ROLE;

/*
 * Create a new game in an initial state.  The returned game has a
 * reference count of one.
 *
 * @return the newly created GAME, if initialization was successful,
 * otherwise NULL.
 */
GAME *game_create(void) {
	GAME *game = Malloc(sizeof(GAME));
	if (game == NULL) {
		return NULL;
	}
	for (int i = 0; i < 9; i++) {
		game->game_array[i] = NULL_ROLE;
	}
	game->chosen = 0;
	game->reference = 0;
	game->role = FIRST_PLAYER_ROLE;
	Sem_init(&game->mutex, 0, 1);
	game->over = 0;
	return game;
}

/*
 * Increase the reference count on a game by one.
 *
 * @param game  The GAME whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same GAME object that was passed as a parameter.
 */
GAME *game_ref(GAME *game, char *why) {
	P(&game->mutex);
	game->reference++;
	debug("%s", why);
	V(&game->mutex);
	return game;
}

/*
 * Decrease the reference count on a game by one.  If after
 * decrementing, the reference count has reached zero, then the
 * GAME and its contents are freed.
 *
 * @param game  The GAME whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being decreased.  This is used for debugging printout, to help trace
 * the reference counting.
 */
void game_unref(GAME *game, char *why) {
	P(&game->mutex);
	game->reference--;
	if (game->reference == 0) {
		V(&game->mutex);
		Free(game);
		game = NULL;
		return;
	}
	debug("%s", why);
	V(&game->mutex);
}

/*
 * Apply a GAME_MOVE to a GAME.
 * If the move is illegal in the current GAME state, then it is an error.
 *
 * @param game  The GAME to which the move is to be applied.
 * @param move  The GAME_MOVE to be applied to the game.
 * @return 0 if application of the move was successful, otherwise -1.
 */
int game_apply_move(GAME *game, GAME_MOVE *move) {
	P(&game->mutex);
	P(&move->mutex);
	if (move == NULL) {
		V(&move->mutex);
		V(&game->mutex);
		return -1;
	}
	if (move->game_role == NULL_ROLE || move->position < 0 || move->position > 8 || game->game_array[move->position] != NULL_ROLE) {
		V(&move->mutex);
		V(&game->mutex);
		return -1;
	}
	game->game_array[move->position] = move->game_role;
	game->chosen++;
	V(&move->mutex);
	V(&game->mutex);
	return 0;
}

/*
 * Submit the resignation of the GAME by the player in a specified
 * GAME_ROLE.  It is an error if the game has already terminated.
 *
 * @param game  The GAME to be resigned.
 * @param role  The GAME_ROLE of the player making the resignation.
 */
int game_resign(GAME *game, GAME_ROLE role) {
	P(&game->mutex);
	if (game == NULL || game->chosen == 9) {
		V(&game->mutex);
		return -1;
	}
	game->role = role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
	V(&game->mutex);
	return game->role;
}

/*
 * Get a string that describes the current GAME state, in a format
 * appropriate for human users.  The returned string is in malloc'ed
 * storage, which the caller is responsible for freeing when the string
 * is no longer required.
 *
 * @param game  The GAME for which the state description is to be
 * obtained.
 * @return  A string that describes the current GAME state.
 */
char *game_unparse_state(GAME *game) {
	P(&game->mutex);
	char *state = Malloc(30);
	int temp = 0;
	for (int i = 0; i < 30; i+=12) {
		if (game->game_array[temp] == FIRST_PLAYER_ROLE) {
			state[i] = 'X';
		} else if (game->game_array[temp] == SECOND_PLAYER_ROLE) {
			state[i] = 'O';
		} else {
			state[i] = ' ';
		}
		state[i+1] = '|';
		temp++;
		if (game->game_array[temp] == FIRST_PLAYER_ROLE) {
			state[i+2] = 'X';
		} else if (game->game_array[temp] == SECOND_PLAYER_ROLE) {
			state[i+2] = 'O';
		} else {
			state[i+2] = ' ';
		}
		state[i+3] = '|';
		temp++;
		if (game->game_array[temp] == FIRST_PLAYER_ROLE) {
			state[i+4] = 'X';
		} else if (game->game_array[temp] == SECOND_PLAYER_ROLE) {
			state[i+4] = 'O';
		} else {
			state[i+4] = ' ';
		}
		temp++;
		if (i != 24) {
			state[i+5] = '\n';
			for (int j = 1; j < 6; j++) {
				state[i+j+5] = '-';
			}
			state[i+11] = '\n';
		}
	}
	state[29] = 0;
	V(&game->mutex);
	return state;
}

/*
 * Determine if a specifed GAME has terminated.
 *
 * @param game  The GAME to be queried.
 * @return 1 if the game is over, 0 otherwise.
 */
int game_is_over(GAME *game) {
	P(&game->mutex);
	if (game->chosen == 9) {
		V(&game->mutex);
		return 1;
	}
	if (game == NULL) {
		game->over = 1;
		game->role = game->role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
		V(&game->mutex);
		return 1;
	}
	for (int i = 0; i < 9; i+=3) {
		if (game->game_array[i] != NULL_ROLE && game->game_array[i] == game->game_array[i+1] && game->game_array[i] == game->game_array[i+2]) {
			game->role = game->game_array[i];
			game->over = 1;
			V(&game->mutex);
			return 1;
		}
	}
	for (int i = 0; i < 3; i++) {
		if (game->game_array[i] != NULL_ROLE && game->game_array[i] == game->game_array[i+3] && game->game_array[i] == game->game_array[i+6]) {
			game->role = game->game_array[i];
			game->over = 1;
			V(&game->mutex);
			return 1;
		}
	}
	if (game->game_array[0] != NULL_ROLE && game->game_array[0] == game->game_array[4] && game->game_array[0] == game->game_array[8]) {
		game->role = game->game_array[0];
		game->over = 1;
		V(&game->mutex);
		return 1;
	}
	if (game->game_array[2] != NULL_ROLE && game->game_array[2] == game->game_array[4] && game->game_array[2] == game->game_array[6]) {
		game->role = game->game_array[2];
		game->over = 1;
		V(&game->mutex);
		return 1;
	}
	V(&game->mutex);
	return 0;
}

/*
 * Get the GAME_ROLE of the player who has won the game.
 *
 * @param game  The GAME for which the winner is to be obtained.
 * @return  The GAME_ROLE of the winning player, if there is one.
 * If the game is not over, or there is no winner because the game
 * is drawn, then NULL_PLAYER is returned.
 */
GAME_ROLE game_get_winner(GAME *game) {
	P(&game->mutex);
	// if (game == NULL) {
	// 	game->over = 1;
	// 	V(&game->mutex);
	// 	return game->role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
	// }
	// GAME_ROLE game_role;
	// for (int i = 0; i < 9; i+=3) {
	// 	if (game->game_array[i] == game->game_array[i+1] && game->game_array[i] == game->game_array[i+2]) {
	// 		game_role = game->game_array[i];
	// 		game->over = 1;
	// 		V(&game->mutex);
	// 		return game_role;
	// 	}
	// }
	// for (int i = 0; i < 3; i++) {
	// 	if (game->game_array[i] == game->game_array[i+3] && game->game_array[i] == game->game_array[i+6]) {
	// 		game_role = game->game_array[i];
	// 		game->over = 1;
	// 		V(&game->mutex);
	// 		return game_role;
	// 	}
	// }
	// if (game->game_array[0] == game->game_array[4] && game->game_array[0] == game->game_array[8]) {
	// 	game_role = game->game_array[0];
	// 	game->over = 1;
	// 	V(&game->mutex);
	// 	return game_role;
	// }
	// if (game->game_array[2] == game->game_array[4] && game->game_array[2] == game->game_array[6]) {
	// 	game_role = game->game_array[0];
	// 	game->over = 1;
	// 	V(&game->mutex);
	// 	return game_role;
	// }
	GAME_ROLE game_role = game->role;
	V(&game->mutex);
	return game_role;
}

/*
 * Attempt to interpret a string as a move in the specified GAME.
 * If successful, a GAME_MOVE object representing the move is returned,
 * otherwise NULL is returned.  The caller is responsible for freeing
 * the returned GAME_MOVE when it is no longer needed.
 * Refer to the assignment handout for the syntax that should be used
 * to specify a move.
 *
 * @param game  The GAME for which the move is to be parsed.
 * @param role  The GAME_ROLE of the player making the move.
 * If this is not NULL_ROLE, then it must agree with the role that is
 * currently on the move in the game.
 * @param str  The string that is to be interpreted as a move.
 * @return  A GAME_MOVE described by the given string, if the string can
 * in fact be interpreted as a move, otherwise NULL.
 */
GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
	P(&game->mutex);
	if (str[0] > '9' || str[0] < '1') {
		V(&game->mutex);
		return NULL;
	}
	if (game->game_array[str[0] - 49] != NULL_ROLE) {
		V(&game->mutex);
		return NULL;
	}
	if (role != NULL_ROLE && role != game->role) {
		V(&game->mutex);
		return NULL;
	}
	game->role = role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
	GAME_MOVE *move = Malloc(sizeof(GAME_MOVE));
	Sem_init(&move->mutex, 0, 1);
	move->position = str[0]-49;
	move->game_role = role;
	V(&game->mutex);
	return move;
}

/*
 * Get a string that describes a specified GAME_MOVE, in a format
 * appropriate to be shown to human users.  The returned string should
 * be in a format from which the GAME_MOVE can be recovered by applying
 * game_parse_move() to it.  The returned string is in malloc'ed storage,
 * which it is the responsibility of the caller to free when it is no
 * longer needed.
 *
 * @param move  The GAME_MOVE whose description is to be obtained.
 * @return  A string describing the specified GAME_MOVE.
 */
char *game_unparse_move(GAME_MOVE *move) {
	P(&move->mutex);
	char str[5];
	sprintf(str, "%d<-%c", move->position+1, move->game_role == FIRST_PLAYER_ROLE ? 'X' : 'O');
	char* ret = strdup(str);
	V(&move->mutex);
	return ret;
}
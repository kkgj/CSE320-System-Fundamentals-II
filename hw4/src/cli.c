/*
 * Legion: Command-line interface
 */

#include "legion.h"
#include <string.h>
#include <sys/wait.h> // wait() added
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

extern char **environ;

typedef struct sf_daemon {
    char* name;
    pid_t process_id;
    enum daemon_status status;
    char* command;
    char** optional_args;
    int optarg_len;
    struct {
        struct sf_daemon *next;
        struct sf_daemon *prev;
    } links;
} sf_daemon;

struct sf_daemon daemon_list;


void removeDamon(struct sf_daemon *dp) {
	dp->links.prev->links.next = dp->links.next;
	dp->links.next->links.prev = dp->links.prev;
	for (int i = 0; i < dp->optarg_len; i++) {
		free(dp->optional_args[i]);
	}
	if (dp->optarg_len > 0) {
		free(dp->optional_args);
	}
	free(dp->name);
	free(dp->command);
	free(dp);
}

void unRegister(char* name) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (strcmp(dp->name, name) == 0) {
			if (dp->status != status_inactive) {
				sf_error("unregister error");
				return;
			}
			sf_unregister(name);
			removeDamon(dp);
			return;
		}
		dp = dp->links.next;
	}
	sf_error("unregister error");
}

int isExisted(char* name) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (strcmp(dp->name, name) == 0) {
			// for (int i = 0; i < dp->optarg_len; i++) {
			// 	printf("optional_args: %s\n", dp->optional_args[i]);
			// }
			return 1;
		}
		dp = dp->links.next;
	}
	return 0;
}

struct sf_daemon* checkStart(char* name) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (strcmp(dp->name, name) == 0) {
			if (dp->status == status_inactive) {
				return dp;
			} else {
				return NULL;
			}
		}
		dp = dp->links.next;
	}
	return NULL;
}

struct sf_daemon* checkStop(char* name) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (strcmp(dp->name, name) == 0) {
			if (dp->status == status_exited || dp->status == status_crashed) {
				dp->status = status_inactive;
				sf_reset(dp->name);
				return NULL;
			} else if (dp->status != status_active) {
				sf_error("stop error");
				return NULL;
			} else {
				dp->status = status_stopping;
				sf_stop(name, dp->process_id);
				return dp;
			}
		}
		dp = dp->links.next;
	}
	sf_error("stop error");
	return NULL;
}

struct sf_daemon* checkLog(char* name) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (strcmp(dp->name, name) == 0) {
			return dp;
		}
		dp = dp->links.next;
	}
	return NULL;
}

volatile pid_t daemon_id = 0;
volatile sig_atomic_t timer = 0;

void killStart(int sig) {
	timer = 1;
	kill(daemon_id, SIGKILL);
}

void interrupt_handler(int sig) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (dp->status != status_inactive) {
			// terminate daemons! TODO:
			sf_stop(dp->name, dp->process_id);
		// sf_term(dp->name, dp->process_id, status_unknown);
		}
		struct sf_daemon *temp = dp->links.next;
		removeDamon(dp);
		dp = temp;
	}
	sf_fini();
	exit(EXIT_SUCCESS);
}

struct sf_daemon *current_child(pid_t id) {
	struct sf_daemon *dp = daemon_list.links.next;
	while (dp != &daemon_list) {
		if (dp->process_id == id) {
			return dp;
		}
		dp = dp->links.next;
	}
	return dp;
}

void child_handler(int sig) {
	pid_t pid;
	int status;
	// pid , status,
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		struct sf_daemon *dp = current_child(pid);
		if (WIFEXITED(status)) {
			dp->process_id = 0;
			dp->status = status_inactive;
        } else if (WIFSIGNALED(status)) {
        	dp->status = status_exited;
        	dp->process_id = 0;
           	//printf("killed by signal %d\n", WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
        	dp->status = status_stopping;
        	dp->process_id = 0;
           	//printf("stopped by signal %d\n", WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            //printf("continued\n");
        }
	}
}

void *Signal(int signum, void* handler) {
	struct sigaction action, old_action;
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
	action.sa_flags = SA_RESTART; /* Restart syscalls if possible */
	if (sigaction(signum, &action, &old_action) < 0) {
		sf_error("Signal error");
	}
	return (old_action.sa_handler);
}

// volatile sig_atomic_t flag = 0;
// void term_handler(int signum) {
// 	flag = 1;
// }

void run_cli(FILE *in, FILE *out) {
	char* line = NULL;
	size_t len = 0;
	int status = 1;
	daemon_list.links.next = &daemon_list;
	daemon_list.links.prev = &daemon_list;
	// signal(SIGALRM, killStart);
	Signal(SIGALRM, killStart);
	Signal(SIGINT, interrupt_handler);
	Signal(SIGCHLD, child_handler);
	//Signal(SIGTERM, term_handler);
	do {
		fprintf(out, "%s", "legion> ");
		fflush(out);
		int ret = getline(&line, &len, in);
		if (ret == -1 || line == NULL || line[0] == '\n') {
			if (line != NULL) {
				free(line);
				line = NULL;
			}
			sf_error("command execution");
			status = 0;
			break;
		}
		char** matrix = malloc(ret * sizeof(char*));
		for (int i = 0; i < ret; i++) {
			matrix[i] = NULL;
		}
		int matrixLen = 0;
		char* start = NULL;
		int state = ' ';
		char* temp = line;
		while (*temp) {
			switch (state) {
				case ' ':
					if (*temp == '\'') {
						start = temp + 1;
						state = '\'';
					} else if (*temp != ' ') {
						start = temp;
						state = 'T';
					}
					break;
				case 'T':
					if (*temp == ' ' || *temp == '\n' || *temp == '\'') {
						matrix[matrixLen] = malloc(temp-start+1);
						int count = 0;
						while (start < temp) {
							matrix[matrixLen][count++] = *start++;
						}
						matrix[matrixLen][count] = 0;
						state = ' ';
						matrixLen++;
					}
					if (*temp == '\'') {
						start = temp + 1;
						state = '\'';
					}
					break;
				case 'Q':
					matrix[matrixLen] = malloc(temp-start+1);
					int count = 0;
					while (start < temp) {
						matrix[matrixLen][count++] = *start++;
					}
					matrix[matrixLen][count] = 0;
					state = ' ';
					matrixLen++;
					break;
				case '\'':
					if (*temp == '\'' || *temp == '\n') {
						state = 'Q';
						temp--;
					}
					break;
			}
			temp++;
	    } // end while
	    if (matrixLen == 0) {
	    	if (line != NULL) {
	    		free(line);
	    		line = NULL;
			}
			for (int j = 0; j < ret; j++) {
				if (matrix[j] != NULL) {
					free(matrix[j]);
				}
			}
			if (matrix != NULL) {
				free(matrix);
				matrix = NULL;
			}
			sf_error("command execution");
			continue;
	    }
		if (matrix[0] != NULL && strcmp(matrix[0], "help") == 0) {
			fprintf(out, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n", "Available commands:", "help (0 args) Print this help message", "quit (0 args) Quit the program",
				"register (0 args) Register a daemon", "unregister (1 args) Unregister a daemon", "status (1 args) Show the status of a daemon",
				"status-all (0 args) Show the status of all daemons", "start (1 args) Start a daemon", "stop (1 args) Stop a daemon", "logrotate (1 args) Rotate log files for a daemon");
			fflush(out);
		} else if (matrix[0] != NULL && strcmp(matrix[0], "quit") == 0) {
			struct sf_daemon *dp = daemon_list.links.next;
			while (dp != &daemon_list) {
				if (dp->status != status_inactive) {
					// terminate daemons! TODO:
					sf_stop(dp->name, dp->process_id);
					// sf_term(dp->name, dp->process_id, status_unknown);
				}
				struct sf_daemon *temp = dp->links.next;
				// sf_unregister(dp->name); // ??? Wait for piazza
				removeDamon(dp);
				dp = temp;
			}
			status = 0;
		} else if (matrix[0] != NULL && strcmp(matrix[0], "register") == 0 && matrixLen > 2) {
			if (!isExisted(matrix[1])) {
				struct sf_daemon *daemonp = malloc(sizeof(struct sf_daemon));
				daemonp->status = status_inactive;
				daemonp->name = malloc(strlen(matrix[1])+1);
				strcpy(daemonp->name, matrix[1]);
				daemonp->command = malloc(strlen(matrix[2])+1);
				strcpy(daemonp->command, matrix[2]);
				daemonp->process_id = 0; // ???
				daemonp->optarg_len = matrixLen - 2;
				if (daemonp->optarg_len > 0) {
					daemonp->optional_args = malloc(sizeof(char*) * (daemonp->optarg_len + 1));
					daemonp->optional_args[0] = malloc(strlen(matrix[2])+1);
					for (int i = 1; i < daemonp->optarg_len; i++) {
						daemonp->optional_args[i] = malloc(strlen(matrix[i+2])+1);
						strcpy(daemonp->optional_args[i], matrix[i+2]);
					}
					daemonp->optional_args[daemonp->optarg_len] = NULL;
				} else {
					daemonp->optional_args = NULL;
				}
				daemonp->links.next = daemon_list.links.next; // Add it to linkedlist
				daemonp->links.prev = &daemon_list;
				daemon_list.links.next->links.prev = daemonp;
				daemon_list.links.next = daemonp;
				sf_register(daemonp->name, daemonp->command);
			} else {
				sf_error("register error");
			}
		} else if (matrix[0] != NULL && strcmp(matrix[0], "unregister") == 0 && matrixLen == 2) {
			unRegister(matrix[1]);
		} else if (matrix[0] != NULL && strcmp(matrix[0], "status") == 0 && matrixLen == 2) {
			int found = -1;
			struct sf_daemon *dp = daemon_list.links.next;
			while (dp != &daemon_list) {
				if (strcmp(dp->name, matrix[1]) == 0) {
					found = 0;
					fprintf(out, "%s\t", dp->name);
					fflush(out);
					if (dp->status == status_active) {
						fprintf(out, "%d\t%s\n", dp->process_id, "active");
						fflush(out);
						break;
					}
					fprintf(out, "%d\t", 0);
					fflush(out);
					if (dp->status == status_unknown) {
						fprintf(out, "%s\n", "unknown");
					} else if (dp->status == status_inactive) {
						fprintf(out, "%s\n", "inactive");
					} else if (dp->status == status_starting) {
						fprintf(out, "%s\n", "starting");
					} else if (dp->status == status_stopping) {
						fprintf(out, "%s\n", "stopping");
					} else if (dp->status == status_exited) {
						fprintf(out, "%s\n", "exited");
					} else {
						fprintf(out, "%s\n", "crashed");
					}
					fflush(out);
					break;
				}
				dp = dp->links.next;
			}
			if (found) {
				sf_error("unregister error");
			}
		} else if (matrix[0] != NULL && strcmp(matrix[0], "status-all") == 0 && matrixLen == 2) {
			struct sf_daemon *dp = daemon_list.links.next;
			while (dp != &daemon_list) {
				fprintf(out, "%s\t", dp->name);
				fflush(out);
				if (dp->status == status_active) {
					fprintf(out, "%d\t%s\n", dp->process_id, "active");
				} else {
					fprintf(out, "%d\t", 0);
					if (dp->status == status_unknown) {
						fprintf(out, "%s\n", "unknown");
					} else if (dp->status == status_inactive) {
						fprintf(out, "%s\n", "inactive");
					} else if (dp->status == status_starting) {
						fprintf(out, "%s\n", "starting");
					} else if (dp->status == status_stopping) {
						fprintf(out, "%s\n", "stopping");
					} else if (dp->status == status_exited) {
						fprintf(out, "%s\n", "exited");
					} else {
						fprintf(out, "%s\n", "crashed");
					}
				}
				fflush(out);
				dp = dp->links.next;
			}
		} else if (matrix[0] != NULL && strcmp(matrix[0], "start") == 0 && matrixLen == 2) {
			struct sf_daemon *dp = checkStart(matrix[1]);
			if (dp == NULL) {
				sf_error("start error");
				for (int j = 0; j < ret; j++) {
					if (matrix[j] != NULL) {
						free(matrix[j]);
						matrix[j] = NULL;
					}
				}
				if (ret > 0) {
					free(matrix);
					matrix = NULL;
				}
				if (line != NULL) {
					free(line);
					line = NULL;
				}
				continue;
			}
			dp->status = status_starting;
			int pipefd[2];
			//int status;
			//char buf;
			if (pipe(pipefd) == -1) {
				sf_error("pipe error");
				for (int j = 0; j < ret; j++) {
					if (matrix[j] != NULL) {
						free(matrix[j]);
						matrix[j] = NULL;
					}
				}
				if (ret > 0) {
					free(matrix);
					matrix = NULL;
				}
				if (line != NULL) {
					free(line);
					line = NULL;
				}
				continue;
			}
			sf_start(dp->name);
			pid_t cpid = fork();
			if (cpid == -1) {
				dp->status = status_inactive;
				sf_error("fork error");
				for (int j = 0; j < ret; j++) {
					if (matrix[j] != NULL) {
						free(matrix[j]);
						matrix[j] = NULL;
					}
				}
				if (ret > 0) {
					free(matrix);
					matrix = NULL;
				}
				if (line != NULL) {
					free(line);
					line = NULL;
				}
				continue;
			}
			if (cpid == 0) { // Child
				close(pipefd[0]); // Close read end
				setpgid(0, 0);
				dup2(pipefd[1], SYNC_FD); ////
				mkdir(LOGFILE_DIR, 0777);
				char str[strlen(LOGFILE_DIR) + strlen(dp->name) + 8];
				sprintf(str, "%s/%s.log.%c", LOGFILE_DIR, dp->name, '0');
				freopen(str, "a+", stdout);
				dp->process_id = getpid();
				char* path = getenv(PATH_ENV_VAR);
				char newPath[strlen(DAEMONS_DIR) + strlen(path) + 2];
				sprintf(newPath, "%s:%s", DAEMONS_DIR, path);
				setenv(PATH_ENV_VAR, newPath, 1);
				execvpe(dp->command, dp->optional_args, environ);
				// finished
				close(pipefd[1]);
			} else {
				close(pipefd[1]); // Close write end
				dp->process_id = cpid;
				daemon_id = cpid;
				alarm(CHILD_TIMEOUT); // Timeout
				char message[2];
				int readReturn = read(pipefd[0], message, 1);
				sigset_t mask;
				sigemptyset(&mask);
				sigaddset(&mask, SIGTERM);
				sigprocmask(SIG_BLOCK, &mask, NULL);
				while (1) {
					if (readReturn == 1) {
						dp->status = status_active;
						sf_active(dp->name, dp->process_id);
						sigprocmask(SIG_UNBLOCK, &mask, NULL);
						timer = 0;
						break;
					}
					if (timer == 1) {
						sf_kill(dp->name, dp->process_id);
						dp->process_id = 0;
						dp->status = status_crashed;
						sf_crash(dp->name, dp->process_id, dp->status);
						sf_error("command execution: start");
						break;
					}
				}
				// finished
				timer = 0;
				alarm(0);
				close(pipefd[0]);
				//waitpid(cpid, &status, WUNTRACED | WCONTINUED);
			}
		} else if (matrix[0] != NULL && strcmp(matrix[0], "stop") == 0 && matrixLen == 2) {
			struct sf_daemon *dp = checkStop(matrix[1]);
			if (dp != NULL) {
				kill(dp->process_id, SIGTERM);
				sigset_t myset;
				sigfillset (&myset);
				sigdelset (&myset, SIGCHLD); // temporarily not block
				sigdelset (&myset, SIGALRM);
				timer = 0;
				alarm(CHILD_TIMEOUT);
				sigsuspend(&myset);
				alarm(0);
				if (timer != 1) {
					dp->status = status_unknown;
					sf_term(dp->name, dp->process_id, dp->status);
					dp->status = status_inactive;
				} else {
					timer = 0;
					kill(dp->process_id, SIGKILL);
					sf_error("command execution: stop error");
				}
			}
		} else if (matrix[0] != NULL && strcmp(matrix[0], "logrotate") == 0 && matrixLen == 2) {
			mkdir(LOGFILE_DIR, 0777);
			struct sf_daemon *dp = checkLog(matrix[1]);
			if (dp != NULL) {
				for (int i = LOG_VERSIONS - 1; i >= 0; i--) {
					char path[strlen(LOGFILE_DIR) + strlen(dp->name) + 8];
					sprintf(path, "%s/%s.log.%c", LOGFILE_DIR, dp->name, '0' + i);
					if (i + 1 == LOG_VERSIONS) {
						unlink(path);
					} else {
						char newPath[strlen(LOGFILE_DIR) + strlen(dp->name) + 8];
						sprintf(newPath, "%s/%s.log.%c", LOGFILE_DIR, dp->name, '0' + i + 1);
						rename(path, newPath);
					}
				}
				sf_logrotate(dp->name);
			} else {
				sf_error("executing command: logrotate error");
			}

		} else {
			sf_error("command execution");
		}
		for (int j = 0; j < ret; j++) {
			if (matrix[j] != NULL) {
				free(matrix[j]);
				matrix[j] = NULL;
			}
		}
		if (ret > 0) {
			free(matrix);
			matrix = NULL;
		}
		if (line != NULL) {
			free(line);
			line = NULL;
		}
	} while (status);
}

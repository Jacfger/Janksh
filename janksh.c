#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if 0 // Enable debugging output
	#define _debugf2(f, line, ...)                   \
		fprintf(f, "DEBUG " #line ": " __VA_ARGS__); \
		fprintf(f, "\n")
	#define trace() printf("TRACE %d\n", __LINE__);
#else
	#define _debugf2(...)
	#define trace()
#endif
#define _debugf(line, ...) _debugf2(stdout, line, __VA_ARGS__)
#define debugf(...)		   _debugf(__LINE__, __VA_ARGS__);

#define _errorf(line, ...) _debugf2(stderr, line, __VA_ARGS__)
#define errorf(...)		   _errorf(__LINE__, __VA_ARGS__)

char CURR_PATH[4096];
char PREV_PATH[4096];
int childExitStatus = 0;
int interrupted = 0;

struct job_struct {
	pid_t pid;
	char* commands;
	struct job_struct* next;
	struct job_struct* prev;
	int jobId;
};

struct job_struct* jobListHead = 0;
struct job_struct* jobListEnd = 0;
int bgJobCounts = 0;

void addJob(struct job_struct* leJob) {
	debugf("add job %d\n", leJob->pid);
	bgJobCounts++;
	if (jobListHead == NULL) {
		jobListHead = jobListEnd = leJob;
		return;
	}
	jobListEnd->next = leJob;
	leJob->prev = jobListEnd;
	jobListEnd = leJob;
}

void removeJob(struct job_struct* leJob) {
	if (leJob == NULL) {
		debugf("Job doesn't exist");
		return;
	}
	if (jobListHead == NULL) {
		debugf("Nothing can be removed in jobList!");
		return;
	}
	debugf("remove job %d", leJob->pid);
	bgJobCounts--;
	if (leJob->prev == NULL) {
		jobListHead = leJob->next;
	} else {
		leJob->prev->next = leJob->next;
	}
	if (leJob->next == NULL) {
		jobListEnd = leJob->prev;
	} else {
		leJob->next->prev = leJob->prev;
	}
	free(leJob->commands);
	free(leJob);
}

struct job_struct* searchByPid(pid_t pid) {
	struct job_struct* node;
	for (node = jobListHead; node != NULL; node = node->next) {
		if (node->pid == pid)
			return node;
	}
	return NULL;
}

void printJob(struct job_struct* node) {
	printf("%10d %30s", node->pid, node->commands);
}

void printJobList() {
	struct job_struct* node;
	int count = 0;
	for (node = jobListHead; node != NULL; node = node->next) {
		printf("[%d]%10d     %s\n", ++count, node->pid, node->commands);
	}
}

void translateString(char* src, char* dst, int n) {
	int i;
	int inQuote = 0;
	int inDQuote = 0;
	int j = 0;
	for (i = 0; i < n; i++) {
		if (src[i] == '\'' && !inDQuote) {
			inQuote ^= 1;
		} else if (src[i] == '"' && !inQuote) {
			inDQuote ^= 1;
		}
		dst[j++] = src[i];
	}
}

void theReaper(int signal) {
	int ret;
	char _buf[] = "A job Ended\n";
	char _buf2[] = "handler is called\n";
	while ((ret = waitpid(-1, 0, WNOHANG)) > 0) {
		// write(STDOUT_FILENO, _buf2, sizeof(_buf2));
		if (ret > 0) {
			struct job_struct* temp = searchByPid(ret);
			removeJob(temp);
			// write(STDOUT_FILENO, _buf, sizeof(_buf));
		}
	}
}

void typeExitYouDumbAss(int signal) {
	char idiotProof[] = "\nPlease use command exit to exit the terminal\n";
	write(STDOUT_FILENO, idiotProof, sizeof(idiotProof));
}

unsigned int tokenize(char* str, char** tokenize, char* separators) {
	char* token;
	unsigned int i = 0;
	for (token = strtok(str, separators); token != NULL; token = strtok(NULL, separators)) {
		tokenize[i++] = token;
	}
	return i;
}

unsigned int tokenize2(char* str, char** tokenize, char* separators) {
	unsigned int i = 0;
	int __state = 0;
	int tokenIndex = 0;
#define dquote 1
#define quote  2
#define normal 0
#define space  3
	char* currentToken = str;
	char* copyString = malloc(strlen(str) + 1);
	int csIndex = 0;
	strcpy(copyString, str);
	tokenize[tokenIndex++] = currentToken;
	for (; copyString[i] != NULL; i++) {
		volatile char c = copyString[i];
		if (copyString[i] == ' ' || copyString[i] == '\t') {
			switch (__state) {
				case normal:
					str[csIndex++] = '\0';
					__state = space;
					break;
				default:
					str[csIndex++] = copyString[i];
					break;
			}
		} else if (copyString[i] == '"') {
			switch (__state) {
				case dquote:
					__state = normal;
					break;
				case space:
					currentToken = str + csIndex;
					tokenize[tokenIndex++] = currentToken;
				case normal:
					__state = dquote;
					break;
				default:
                    str[csIndex++] = copyString[i];
					break;
			}
		} else if (copyString[i] == '\'') {
			switch (__state) {
				case quote:
					__state = normal;
					break;
				case space:
					currentToken = str + csIndex;
					tokenize[tokenIndex++] = currentToken;
				case normal:
					__state = quote;
					break;
				default:
                    str[csIndex++] = copyString[i];
					break;
			}
		} else if (copyString[i] == '$') {
			if (__state == space) {
				currentToken = str + csIndex;
				tokenize[tokenIndex++] = currentToken;
				__state = normal;
			}
			if (strncmp("$$", &copyString[i], 2) == 0 && __state != quote) {
				sprintf(str + csIndex, "%d", getpid());
				csIndex += strnlen(str + csIndex, 10);
				i++;
			} else if (strncmp("$?", &copyString[i], 2) == 0 && __state != quote) {
				sprintf(str + csIndex, "%d", childExitStatus);
				csIndex += strnlen(str + csIndex, 5);
				i++;
			} else {
				str[csIndex++] = copyString[i];
			}
		} else {
			if (copyString[i] != '\n') {
				str[csIndex++] = copyString[i];
				if (__state == space) {
					currentToken = str + csIndex - 1;
					tokenize[tokenIndex++] = currentToken;
					__state = normal;
				}
			} else {
				str[csIndex++] = '\0';
			}
		}
	}

	// add offset to every element in tokenize
	debugf("%d     ", tokenIndex);
	i = 0;
	for (; i < tokenIndex; i++) {
		debugf("%s ", tokenize[i]);
	}
    free(copyString);
	debugf("\n");
	return tokenIndex;
	// char* token;
	// i = 0;
	// for (token = strtok(str, separators); token != NULL; token = strtok(NULL, separators)) {
	// 	tokenize[i++] = token;
	// }
	// return i;
}

int inBuiltCommands(char** tokenized, int length) {
	if (strcmp(tokenized[0], "cd") == 0) {
		if (length == 1) {
			if (chdir(getpwuid(getuid())->pw_dir) != 0) {
				fprintf(stderr, "cd: %s: %s\n", strerror(errno), tokenized[1]);
				return -1;
			}
			strncpy(PREV_PATH, CURR_PATH, 4096);
			return 0;
		} else if (length > 2) {
			printf("cd: Too many arguments");
			return -1;
		} else {
			if (strcmp(tokenized[1], "-") == 0) {
				tokenized[1] = PREV_PATH;
				printf("%s\n", PREV_PATH);
				return -1;
			}
			if (chdir(tokenized[1]) != 0) {
				fprintf(stderr, "cd: %s: %s\n", strerror(errno), tokenized[1]);
				return -1;
			}
			strncpy(PREV_PATH, CURR_PATH, 4096);
			if (getcwd(CURR_PATH, 4096) == NULL) {
				perror("Get current directory failed");
				return -1;
			}
		}
		return 0;
	} else if (strcmp(tokenized[0], "jobs") == 0) {
		printJobList();
		return 0;
	} else if (strcmp(tokenized[0], "exit") == 0) {
		exit(0);
	}
	return 1;
}

int commandLength(char** tokenized, int length) {
	int i;
	int _len = 0;
	for (i = 0; i < length; i++) {
		_len += strlen(tokenized[i]) + 1;
	}
	debugf("Command length: %d\n", _len);
	return _len;
}

char* cpyCommand(char** tokenized, int length) {
	char* __copy = malloc(commandLength(tokenized, length));
	int i;
	int _len = 0;
	for (i = 0; i < length; i++) {
		strcpy(__copy + _len, tokenized[i]);
		if (i != length - 1) {
			_len += strlen(tokenized[i]) + 1;
			*(__copy + _len - 1) = ' ';
		}
	}
	debugf("Copied command: %s", __copy);
	return __copy;
}

int expandCommand(char** tokenized, int length) {
	int i;
	for (i = 0; i < length; i++) {
	}
}
int execCommand(char** tokenized, int length) {
	if (length <= 0) {
		return 0;
	}
	int isBackground = 0;
	pid_t pid;
	if (strcmp(tokenized[length - 1], "&") == 0) {
		if (length == 1) {
			return 0;
		}
		struct job_struct* new = malloc(sizeof(struct job_struct));
		new->commands = cpyCommand(tokenized, length);
		tokenized[--length] = 0;
		isBackground = 1;
		if ((pid = fork()) > 0) {
			new->pid = pid;
			new->next = 0;
			new->prev = 0;
			new->jobId = bgJobCounts + 1;
			printf("[%d]%10d\n", bgJobCounts, new->pid);
			addJob(new);
			return 0;
		}
	}
	int ret;
	if ((ret = inBuiltCommands(tokenized, length)) <= 0) {
		if (isBackground && pid == 0) {
			exit(ret);
		}
		return ret;
	}
	pid_t pid2;
	if (!isBackground) {
		if ((pid2 = fork()) <= 0 && execvp(tokenized[0], tokenized)) {
			fprintf(stderr, "janksh: %s: %s\n", strerror(errno), *tokenized);
			exit(-1);
		} else {
			waitpid(pid2, &childExitStatus, 0);
            childExitStatus = WEXITSTATUS(childExitStatus);
			debugf("Child exited: %d\n", childExitStatus);
		}
	} else {
		// int i = 0;
		// for (; i < length; i++) {
		// 	printf("%s\n", tokenized[i]);
		// }
		if (execvp(tokenized[0], tokenized)) {
			fprintf(stderr, "janksh: %s: %s\n", strerror(errno), tokenized[0]);
			exit(-1);
		} // path to exe, tokenized
	}
	return 0;
}

int main() {
	signal(SIGCHLD, theReaper);
	struct sigaction action = {0};
	action.sa_handler = &typeExitYouDumbAss;
	sigaction(SIGINT, &action, NULL);
	// signal(SIGINT, typeExitYouDumbAss);

	if (getcwd(PREV_PATH, 4096) == NULL)
		perror("Get current directory failed");
	printf("Welcome to JankSh\n");
	char* command = malloc(sysconf(_SC_ARG_MAX) * sizeof(char));
	size_t _size = 0;
	char** tokenized = malloc(sysconf(_SC_ARG_MAX) * sizeof(char*));
	while (1) {
		if (getcwd(CURR_PATH, 4096) == NULL)
			perror("Get current directory failed");
		if (strcmp(CURR_PATH, getpwuid(getuid())->pw_dir) == 0)
			printf("[%s] janksh > ", "~");
		else if (strcmp(CURR_PATH, "/") == 0)
			printf("[%s] janksh > ", "/");
		else
			printf("[%s] janksh > ", strrchr(CURR_PATH, '/') + 1);
		if (fgets(command, sysconf(_SC_ARG_MAX), stdin) == NULL) {
			// *command = '\0';
			continue;
		}
		int tokenCount = tokenize2(command, tokenized, " \t\n");
		tokenized[tokenCount] = 0;
		expandCommand(tokenized, tokenCount);
		execCommand(tokenized, tokenCount);
	}
	free(command);
	free(tokenized);
	return 0;
}
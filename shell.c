#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

struct job {
	int jobID;
	pid_t processID;
	char status[100];
	char program[100];
	unsigned char background;
};

int numJobs = 0;
struct job listjobs[100];

char* ridSpace(char* input) {
    int i,j;
    int x = 1;
    char *output = input;
    for (i = 0, j = 0; i < strlen(input); i++,j++) {
        if(input[i] != ' ') {
	    if(input[i] == '&')
		    j--;
	    else 
            	output[j] = input[i];
	    x = 0;
	}
	else if(x)
		j--;
        else if(input[i] == ' ') {
		if((i+1) >= strlen(input))
			j--;
		else if(input[i+1] == ' ')
      			j--;
		else
			output[j] = input[i];
	}

    }
    output[j]= '\0';

    return output;
}

int numArgs(char *arr) {
	int count = 1;
	for(int i = 0; i < strlen(arr); i++) {
		if(arr[i] == ' ')
			count++;
	}
	return count;
}

void handle_sigint_sigtstp(int sig) {
	pid_t x;
	int flag = 0;
	for(int i = 0; i < numJobs; i++) {
		if(strcmp(listjobs[i].status, "Running") == 0 && (listjobs[i].background == ' ')) {
			x = listjobs[i].processID;
			flag = 1;
			break;
		}
	}
	if(flag) {
		kill((-1*x), sig);
	}
}

void fg(int ID, pid_t PGID) {
	if(strcmp(listjobs[ID-1].status, "Stopped") == 0) {
		kill(PGID, SIGCONT);
		strcpy(listjobs[ID-1].status, "Running");
	}
	if(listjobs[ID-1].background == '&')
		listjobs[ID-1].background = ' ';
	int child_status;
        waitpid(-1*PGID, &child_status, WUNTRACED);
        if(WIFSIGNALED(child_status) && WTERMSIG(child_status) == 2) {
        	strcpy(listjobs[ID-1].status, "Terminated");
		printf("[%d] %ld terminated by signal %d\n", listjobs[ID-1].jobID, (long)(listjobs[ID-1].processID), WTERMSIG(child_status));
        	if(ID == numJobs)
			numJobs--;
        }
        else if(WIFEXITED(child_status)) {
                strcpy(listjobs[ID-1].status, "Terminated");
                if(ID == numJobs)
			numJobs--;
        }
        else if(WIFSTOPPED(child_status))
        	strcpy(listjobs[ID-1].status, "Stopped");
}

int main(int argc, char **argv) {
	char *arr = NULL;
	size_t n = 0;
	int args;
	int index;
	int j = 0;
	char **arg;
	char filter[1000];
	signal(SIGINT, handle_sigint_sigtstp);
	signal(SIGTSTP, handle_sigint_sigtstp);

	printf("> ");
	while(getline(&arr, &n, stdin) != EOF) {
		pid_t pid1;
                int child_status1;
                while((pid1 = waitpid(-1, &child_status1, WNOHANG)) > 0) {
                	for(int i = 0; i < numJobs; i++) {
                        	if(listjobs[i].processID == pid1) {
                                	strcpy(listjobs[i].status, "Terminated");
                                        if(i == numJobs-1)
                                                numJobs--;
                                        break;
                                }
                        }
                }

		for(int i = 0; i < strlen(arr)-1; i++) {
                        filter[i] = arr[i];
                }
		filter[strlen(arr)-1] = '\0';
		numJobs++;
		listjobs[numJobs-1].jobID = numJobs;
                strcpy(listjobs[numJobs-1].status, "Running");

                if(strstr(filter, "&")!=NULL)
                        listjobs[numJobs-1].background = '&';
                else
                        listjobs[numJobs-1].background = ' ';

                ridSpace(filter);
		args = numArgs(filter);
		strcpy(listjobs[numJobs-1].program, filter);

		arg = malloc(args * sizeof(char*));
		for(int i = 0; i < args; i++) {
			arg[i] = malloc(1000);
		}

		index = 0;
		for(int i = 0; i < args; i++) {
			while((filter[index] != ' ') && (index < strlen(filter))) {
				arg[i][j] = filter[index];
				index++;
				j++;
			}
			arg[i][j] = '\0';
			index++;
			j = 0;
		}	

		if(strcmp(arg[0], "exit") == 0) {
			numJobs--;
			for(int i = 0; i < numJobs; i++) {
				if(strcmp(listjobs[i].status, "Stopped") == 0) {
					kill(-1*listjobs[i].processID, SIGHUP);
					kill(-1*listjobs[i].processID, SIGCONT);
				}
				else if(strcmp(listjobs[i].status, "Running") == 0) 
					kill(-1*listjobs[i].processID, SIGHUP);
			}
			for(int i = 0; i < args; i++) {
                        	free(arg[i]);
                	}
                	free(arg);
                	free(arr);
			return EXIT_SUCCESS;
		}	
		if(strncmp(filter, "kill %", 6) == 0) {
			numJobs--;
			int ID = ((filter[6])-'0');
			int PGID = -1*listjobs[ID-1].processID;
			if(ID > 0 && ID <= numJobs) {
				if(numJobs == 1 || ID == numJobs)
					numJobs--;
				kill(PGID, SIGTERM);
				if(strcmp(listjobs[ID-1].status, "Stopped") == 0)
					kill(PGID, SIGCONT);
				waitpid(-1*PGID, NULL, 0);
				strcpy(listjobs[ID-1].status, "Terminated");
				printf("[%d] %ld terminated by signal %d\n", listjobs[ID-1].jobID, (long)(listjobs[ID-1].processID), 15);
			}
			else
				printf("Invalid jobID\n");
		}
		else if(strcmp(filter, "cd") == 0) {
			numJobs--;
			char set[1000];
			if(chdir(getenv("HOME")) == -1)
				printf("Invalid directory\n");
			else {
				getcwd(set, sizeof(set));
				setenv("PWD", set, 1);
			}
		}
		else if(strncmp(filter, "cd ", 3) == 0) {
			numJobs--;
			char path[1000];
			char set[1000];
			int pathI = 0;
			for(int i = 3; i < strlen(filter); i++) {
				path[pathI] = filter[i];
				pathI++;
			}
			path[pathI] = '\0';
			if(chdir(path) == -1)
				printf("Invalid directory\n");
			else {
				getcwd(set, sizeof(set));
				setenv("PWD", set, 1);
			}
		}
		else if(strncmp(filter, "fg %", 4) == 0) {
			numJobs--;
			int ID = ((filter[4]) - '0');
			int PGID = -1*listjobs[ID-1].processID;
			if(ID > 0 && ID <= numJobs) 
				 fg(ID, PGID);	
	 		else
				printf("Invalid jobID\n");
		}
		else if(strncmp(filter, "bg %", 4) == 0) {
			numJobs--;
			int ID = ((filter[4]) - '0');
			int PGID = -1*listjobs[ID-1].processID;
			if(ID > 0 && ID <= numJobs) {
				if(strcmp(listjobs[ID-1].status, "Stopped") == 0) {
					kill(PGID, SIGCONT);
					strcpy(listjobs[ID-1].status, "Running");
					listjobs[ID-1].background = '&';
				}
			}
			else
				printf("Invalid jobID\n");
		}
		else if(arg[0][0] == '.' || arg[0][0] == '/') {
			pid_t pid;
			if((pid = fork()) == 0) {
				setpgid(0,0);
				if(execv(arg[0], arg) == -1) {
					printf("%s: No such file or directory\n", arg[0]);
					kill(getpid(), SIGTERM);
				}
			}
			listjobs[numJobs-1].processID = pid;
			if(listjobs[numJobs-1].background == '&') 
                                printf("[%d] %d\n", listjobs[numJobs-1].jobID, listjobs[numJobs-1].processID);
			else {
				int child_status;
	                	waitpid(pid, &child_status, WUNTRACED);
				if(WIFSIGNALED(child_status) && WTERMSIG(child_status) == 2) {
                       			printf("[%d] %ld terminated by signal %d\n", listjobs[numJobs-1].jobID, (long)(listjobs[numJobs-1].processID), WTERMSIG(child_status));
               				numJobs--;
				}	
				if(WIFSIGNALED(child_status) && WTERMSIG(child_status) == 15)			
					numJobs--;
				else if(WIFEXITED(child_status)) {
					strcpy(listjobs[numJobs-1].status, "Terminated");
					numJobs--;
				}
				else if(WIFSTOPPED(child_status))	
					strcpy(listjobs[numJobs-1].status, "Stopped");
			}
		}	
		else if(strcmp(arg[0], "jobs") == 0) {
			numJobs--;
			for(int i = 0; i < numJobs; i++) {
				if(strcmp(listjobs[i].status, "Terminated") != 0)
					printf("[%d] %ld %s %s %c\n", listjobs[i].jobID, (long)listjobs[i].processID, listjobs[i].status, listjobs[i].program, listjobs[i].background);
			}
		}
		else if((strncmp(filter, "fg %", 4) != 0) && (strncmp(filter, "bg %", 4) != 0) && (strncmp(filter, "cd ", 3) != 0) && (strcmp(filter, "jobs") != 0)) {
			char check1[100] = "/usr/bin/";
        		char command[100];
        		char check2[100] = "/bin/";
			strcat(check1, arg[0]);
			strcpy(command, arg[0]);
                        strcat(check2, arg[0]);
			strcpy(arg[0], check1);
			pid_t pid;
			if((pid = fork()) == 0) {
				setpgid(0,0);
				if(execv(arg[0], arg) == -1) {
					strcpy(arg[0], check2);
					if(execv(arg[0], arg) == -1) {
						printf("%s: command not found\n", command);
						kill(getpid(), SIGTERM);
					}
				}
			}
			listjobs[numJobs-1].processID = pid;
			if(listjobs[numJobs-1].background == '&')
                                printf("[%d] %d\n", listjobs[numJobs-1].jobID, listjobs[numJobs-1].processID);
			else {
				int child_status;
                        	waitpid(pid, &child_status, WUNTRACED);
                        	if(WIFSIGNALED(child_status) && WTERMSIG(child_status) == 2) {
                        	        printf("[%d] %ld terminated by signal %d\n", listjobs[numJobs-1].jobID, (long)(listjobs[numJobs-1].processID), WTERMSIG(child_status));
                        	        numJobs--;
                        	}
                        	if(WIFSIGNALED(child_status) && WTERMSIG(child_status) == 15)
                        	        numJobs--;
                        	else if(WIFEXITED(child_status)) {
                        	        strcpy(listjobs[numJobs-1].status, "Terminated");
                                	numJobs--;
                        	}
                        	else if(WIFSTOPPED(child_status))
                                	strcpy(listjobs[numJobs-1].status, "Stopped");
			}
		}
		pid_t pid2;
        	int child_status2;
        	while((pid2 = waitpid(-1, &child_status2, WNOHANG)) > 0) {
        	        for(int i = 0; i < numJobs; i++) {
                	        if(listjobs[i].processID == pid2) {
                        	        strcpy(listjobs[i].status, "Terminated");
                        	        if(i == numJobs-1)
                                	        numJobs--;
                                	break;
                        	}
                	}
        	}
		printf("> ");
		for(int i = 0; i < args; i++) {
			free(arg[i]);
		}
		free(arg);
		free(arr);
		n = 0;
	}
	for(int i = 0; i < numJobs; i++) {
        	if(strcmp(listjobs[i].status, "Stopped") == 0) {
                	kill(-1*listjobs[i].processID, SIGHUP);
                        kill(-1*listjobs[i].processID, SIGCONT);
                }
		else if(strcmp(listjobs[i].status, "Running") == 0)
                        kill(-1*listjobs[i].processID, SIGHUP);
        }
        free(arr);
	printf("\n");
	
	return EXIT_SUCCESS;

}	

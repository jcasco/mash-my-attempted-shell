//  shell.c
//
//  Josue Casco on 11/29/14.
//  CS360
//
//  Unix shell with redirection and piping.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include "fields.h"
#include "jrb.h"
#include "dllist.h"

typedef struct cmd_struct{
	char *exec;
	char *redIn;
	char *redOut;
	char *appdOut;
	int front;
	int back;
	int isBack;
	int numArgs;
	Dllist newargs;
}Cmd;

Cmd * new_cmd(){
	Cmd *cmd = (Cmd *) malloc(sizeof(Cmd));
	cmd->exec = NULL;
	cmd->redIn = NULL;
	cmd->redOut = NULL;
	cmd->appdOut = NULL;
	cmd->front = 0;
	cmd->back = 0;
	cmd->numArgs = 0;
	cmd->newargs = new_dllist();
	cmd->isBack = 0;

	return cmd;
}

//creates array of args executes program
int exec(Dllist l, int newArgc){
	char **newargv;
	int i = 0;
	Dllist dnode;

	newargv = (char **) malloc(sizeof(char *) * newArgc+1);

	dll_traverse(dnode, l){
		newargv[i] = dnode->val.v;
		i++;
	}
	newargv[i] = NULL;
	execvp(newargv[0], newargv);
	perror(newargv[0]);
	exit(1);
}


//kill child processes
void kill_chlds(Dllist chldPids){
	pid_t tmpPid;
	int status;
	Dllist tmpNode;

	dll_traverse(tmpNode, chldPids){
		tmpPid = tmpNode->val.i;
		kill(tmpPid, SIGKILL);
		wait(&status);
	}

	exit(1);

}

process_ln(JRB cmds, Dllist chldPids){
	JRB rnode;
	Cmd *cmdNd;
	char *tmpStr;
	int pid, status;

	//iterate through commands in JRB
	jrb_traverse(rnode, cmds){
		int fd[2];
		int pipeOut, pipeIn;
		int nextIn;
		int fdIn, fdOut;

		cmdNd = (Cmd *)jrb_val(rnode).v;
		
		//if not last command in pipline
		if(cmdNd->back == 0){
			//call pipe to allocate two file desc
			if(pipe(fd) < 0){
				perror("pipe");
				exit(1);
			}

			pipeOut = fd[1];
			nextIn = fd[0];
		}

		pid = fork();

		//if child
		if(pid == 0){
			//if first command in pipeline
			if(cmdNd->front == 1){
				//if there is input redirection
				if(cmdNd->redIn != NULL ){
					//open the redirect file (replaces stdin)
					fdIn = open(cmdNd->redIn, O_RDONLY);
					if(dup2(fdIn, 0) != 0){
						perror(cmdNd->redIn);
						exit(1);
					}
					close(fdIn);
				}
			}
			//else
			else{
				//dup2 file desc pipeIn
				if(dup2(pipeIn, 0) != 0){
					perror("pipe");
					exit(1);
				}
				
				close(pipeIn);
			}
			//if last command in pipeline
			if(cmdNd->back == 1){
				//if output or append redirect
				if(cmdNd->redOut != NULL || cmdNd->appdOut != NULL){
					//open redirect file
					if(cmdNd->redOut != NULL){
						fdOut = open(cmdNd->redOut, O_WRONLY | O_TRUNC | O_CREAT, 0644);
					}
					else{
						fdOut = open(cmdNd->appdOut, O_WRONLY | O_APPEND | O_CREAT, 0644);
					}
					if(dup2(fdOut, 1) != 1){
						//perror(cmdNd->redOut);
						exit(1);
					}
					close(fdOut);
				}
			}
			//else
			else{
				close(nextIn);
				//dup PIPEOUT (replaces stdout)
				if(dup2(pipeOut, 1) != 1){
					perror("pipe");
					exit(1);
				}

				close(pipeOut);
			}

			exec(cmdNd->newargs, cmdNd->numArgs);
		}
		//parent
		else{
			//record child process ID
			dll_append(chldPids, new_jval_v(pid));
			//if not first process
			if(cmdNd->front == 0){
				//close pipeIn (child has it open, don't need)
				close(pipeIn);
			}
			//if not last
			if(cmdNd->back == 0){
				close(pipeOut);
				pipeIn = nextIn;
			}
			if(cmdNd->isBack == 0)
				wait(&status);
		}
	}
}

int main(int argc, char **argv){
    IS is;
    char *prompt;
    Cmd *cmd;
    int i;
    JRB cmds = make_jrb();
    int status;
    Dllist chldPids = new_dllist();


	//prompt
    if(argc < 2){
    	prompt = argv[0];
	}
	else{
		prompt = argv[1];
	}
	printf("%s: ", prompt);
	
	is = new_inputstruct(NULL);

	//get commands
	while(get_line(is) >= 0){
		//new cmd node
		JRB cmds = make_jrb();
		cmd = new_cmd();

		//loop through fields
		for(i=0; i < is->NF; i++){
			if(strcmp(is->fields[i], "exit") == 0){
				kill_chlds(chldPids);
				
				return 0;
			}

			if(strcmp(is->fields[i], "|") == 0){
				i++;
				jrb_insert_int(cmds, i, new_jval_v(cmd));
				cmd = new_cmd();
			}
			if(i == 0){
				cmd->front = 1;	
			}

			//if redirect
			if(strcmp(is->fields[i], "<") == 0){
				i++;
				//add input 
				cmd->redIn = strdup(is->fields[i]);
			}
			else if(strcmp(is->fields[i], ">") == 0){
				i++;
				//add output 
				cmd->redOut = strdup(is->fields[i]);
			}
			else if(strcmp(is->fields[i], ">>") == 0){
				i++;
				//add output 
				cmd->appdOut = strdup(is->fields[i]);
			}
			else if(strcmp(is->fields[i], "&") == 0 && i == is->NF-1){
				cmd->isBack = 1;
			}
			else{
				dll_append(cmd->newargs, new_jval_s(strdup(is->fields[i])));
				cmd->numArgs++;
			}

			//last 
			if(i == is->NF-1){
				cmd->back = 1;
			}
		}
		jrb_insert_int(cmds, i, new_jval_v(cmd));

		process_ln(cmds, chldPids);
		
		jrb_free_tree(cmds);
		
		printf("%s: ", prompt);
	}
	return 0;
}

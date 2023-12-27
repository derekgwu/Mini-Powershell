#include <msh.h>
#include <msh_parse.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>


struct proc_data{
	pid_t proc_pid;
};

struct msh_pipeline* background[MSH_MAXBACKGROUND] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL}; //make these null
unsigned int pl_count = 0;
struct msh_pipeline* foreground;
pid_t pid;
pid_t child_pid;
pid_t data_pid;
int cmd_count;
struct proc_data* wait_c_pd;
pid_t wait_c_pd_pid;
struct msh_command* wait_c;

char* std_out = NULL;
char* std_err = NULL;

int msh_wait(pid_t process_id, int block){
	pid_t ret;
	int options = 0;
	if(block == 0){
		options = WNOHANG;
	}
	ret = waitpid(process_id, NULL, options);


	//if waitpid returned 0, there's no more child to reap
	if(ret == 0){
		return -1;
	}

	//if waitpid returned >0, child has been reaped and background process has finished
	else if(ret > 0){
		return 0;
	}

	//check for interrupt?
	else if(errno == EINTR){
		return 1;
	}

	else if (errno == ECHILD) {
		return -1;
	}

	return 0;
}

void foreground_wait(){
	cmd_count = 0;
	wait_c = msh_pipeline_command(foreground, cmd_count);
	while(wait_c != NULL && foreground != NULL){
		//wait for the foreground
		wait_c_pd = msh_command_getdata(wait_c);
		wait_c_pd_pid = wait_c_pd->proc_pid;

		while(msh_wait(wait_c_pd_pid, 1) == 1 && foreground != NULL);
		if(errno == EINTR){
			return;
		}
		else{
			//increment
			cmd_count++;
			wait_c = msh_pipeline_command(foreground, cmd_count);
		}
		
	}
}
void check_bg(pid_t reaped_pid){
	struct msh_command* wait_c;
	struct proc_data* pd;

	//check every background pipeline
	for(unsigned int i = 0; i < MSH_MAXBACKGROUND; i++ ){
		if(background[i] == NULL){
			continue;
		}
		unsigned int cmd_count = 0;
		wait_c = msh_pipeline_command(background[i], cmd_count);

		//iterate through every command
		while(wait_c != NULL){

			//get the pid of the command
			pd = msh_command_getdata(wait_c);

			//if we reaped that command,set the pid to -1
			if(pd->proc_pid == reaped_pid){
				msh_command_putdata(wait_c, NULL, free);
			}

			//if we didn't reap it, break out we are still waiting
			if((pd = msh_command_getdata(wait_c)) != NULL){
				break;
			}

			//if we reaped everything, free the background
			if(msh_command_final(wait_c) == 1){
				msh_pipeline_free(background[i]);
				background[i] = NULL;
				pl_count--;
				break;
				
			}

			//get next command
			cmd_count++;
			if(background[i] != NULL){
				wait_c = msh_pipeline_command(background[i], cmd_count);
			}
		}
	}
}

void wait_but_dont_block(){
	while((data_pid = waitpid(0, NULL, WNOHANG)) != -1 && data_pid != 0){
		//printf("reaped %d\n", data_pid);
		check_bg(data_pid);
		fflush(stdout);	
	}
}

int append_or_trunc(char** args){
	unsigned int i = 0;
	while(args[i] != NULL){
		if(strcmp(args[i], "1>>") == 0 || strcmp(args[i], "2>>") == 0){
			return 1;
		}

		if(strcmp(args[i], "1>") == 0 || strcmp(args[i], "2>") == 0){
			return 0;
		}
		i++;
	}
	return -1;
}

void check_file_output(struct msh_command* c){
	int fd;
	int action;
	msh_command_file_outputs(c, &std_out, &std_err);
	if(std_out == NULL && std_err == NULL){
		return;
	}

	if(std_out != NULL){
		action = append_or_trunc(msh_command_args(c));
		//truncate
		if(action == 0){
			fd = open(std_out, O_WRONLY | O_CREAT | O_TRUNC, 0700);
			dup2(fd, STDOUT_FILENO);
			char** new_args = malloc(256);
			for(unsigned int i = 0; msh_command_args(c)[i] != NULL; i++){
				if(strcmp(msh_command_args(c)[i], "1>") == 0){
					break;
				}
				new_args[i] = msh_command_args(c)[i];
			}
			execvp(msh_command_program(c), new_args);
		}
		//append
		if(action == 1){
			fd = open(std_out, O_WRONLY | O_CREAT, 0700);
			lseek(fd, 0, SEEK_END);
			dup2(fd, STDOUT_FILENO);
			char** new_args = malloc(256);
			for(unsigned int i = 0; msh_command_args(c)[i] != NULL; i++){
				if(strcmp(msh_command_args(c)[i], "1>>") == 0){
					break;
				}
				new_args[i] = msh_command_args(c)[i];
			}
			execvp(msh_command_program(c), new_args);
		}
	}

	else if(std_err != NULL){
		action = append_or_trunc(msh_command_args(c));
		if(action == 0){
			fd = open(std_err, O_WRONLY | O_CREAT | O_TRUNC, 0700);
			dup2(fd, STDERR_FILENO);
			char** new_args = malloc(256);
			for(unsigned int i = 0; msh_command_args(c)[i] != NULL; i++){
				if(strcmp(msh_command_args(c)[i], "2>") == 0){
					break;
				}
				new_args[i] = msh_command_args(c)[i];
			}
			execvp(msh_command_program(c), new_args);
		}
		//append
		if(action == 1){
			fd = open(std_err, O_WRONLY | O_CREAT, 0700);
			lseek(fd, 0, SEEK_END);
			dup2(fd, STDERR_FILENO);
			char** new_args = malloc(256);
			for(unsigned int i = 0; msh_command_args(c)[i] != NULL; i++){
				if(strcmp(msh_command_args(c)[i], "2>>") == 0){
					break;
				}
				new_args[i] = msh_command_args(c)[i];
			}
			execvp(msh_command_program(c), new_args);
		}
	}

	
}



void fork_and_exec(struct msh_pipeline* p){
	struct proc_data* data;
	int i = 0;
	struct msh_command* c =  msh_pipeline_command(p, i);
	//change directories

	//if there is only one command, just execute
	if(msh_command_final(c) == 1){
		pid = fork();
		if(pid == -1){
			exit(1);
		}

		//assign the pid to the command
		data = calloc(1, sizeof(struct proc_data));
		data->proc_pid = pid;
		msh_command_putdata(c, data, free);
		
		
		//check_file_output(c);
		if(pid== 0){
			signal(SIGTSTP, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			check_file_output(c);
			execvp(msh_command_program(c), msh_command_args(c));
			exit(1);
		}
		
		//printf("%d\n", pid);
		fflush(stdout);	
		
		
	} 
	else{
		//else there has to be multiple commands
		int fd[2];
		int ret = pipe(fd);
    	int carry = 0;

		//fork
		pid = fork();

		if(pid == -1){
			exit(1);
		}
		//assign the pid to the command
		data = calloc(1, sizeof(struct proc_data));
		data->proc_pid = pid;
		msh_command_putdata(c, data, free);


		//if in the child
		if(pid == 0){
			signal(SIGTSTP, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			//redirect stdout to the write end of the pipe
			dup2(fd[1], STDOUT_FILENO);

    	    close(fd[0]);

			//execute
			check_file_output(c);
			execvp(msh_command_program(c), msh_command_args(c));
			exit(1);
		} 

		//in parent
		if(pid != 0){
			//close the write end of the pipe
			close(fd[1]);
		}
		//carry over the input of the previous 
		carry = fd[0];
		//get the next command
		i = i + 1;
		c = msh_pipeline_command(p, i);


		//excecute each command that's not the first or middle one
		while(msh_command_final(c) != 1){
			//create a new pipe
			ret = pipe(fd);
			if(ret == -1){
				exit(1);
			}

			//fork
			pid = fork();
			if(pid == -1){
				exit(1);
			}

			//assign the pid to the command
			data = calloc(1, sizeof(struct proc_data));
			data->proc_pid = pid;
			msh_command_putdata(c, data, free);


			if(pid == 0){
				signal(SIGTSTP, SIG_IGN);
				signal(SIGINT, SIG_IGN);
				dup2(carry, STDIN_FILENO);
    	        close(carry);
    	        dup2(fd[1], STDOUT_FILENO);
    	        close(fd[0]);
				check_file_output(c);
				execvp(msh_command_program(c), msh_command_args(c));
				exit(1);
			} else{
				//close the write end of the pipe
    	        carry = fd[0];
				close(fd[1]);
			}

			i++;
			c = msh_pipeline_command(p, i);
		}

		//last command 
		int new_fd[2];
		ret = pipe(new_fd);
		if(ret == -1){
			exit(1);
		}



		pid = fork();
		if(pid == -1){
			exit(1);
		}
		//assign the pid to the command
		data = calloc(1, sizeof(struct proc_data));
		data->proc_pid = pid;
		msh_command_putdata(c, data, free);

		if(pid == 0){
			signal(SIGTSTP, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			//redirect stdin to the read end
			dup2(carry, STDIN_FILENO);
    	    close(fd[0]);
			//no need to block the stdout here
			check_file_output(c);
			execvp(msh_command_program(c), msh_command_args(c));
			exit(1);
		} 

		//close the neccesary pipes
		if(pid != 0){
			close(carry);
			close(new_fd[1]);
		}


		if(fd[0] != STDIN_FILENO){
			close(new_fd[0]);
		}
		if(fd[1] != STDOUT_FILENO){
			close(new_fd[1]);
		}

	}
}



void msh_execute(struct msh_pipeline *p){
	if(p == NULL){
		for(unsigned int i = 0; i < MSH_MAXBACKGROUND; i++){
			if(background[i] != NULL){
				free(background[i]);
				background[i] = NULL;
			}
		}
		return;
	}
	struct msh_command* c = msh_pipeline_command(p, 0);

	if(strcmp(msh_command_program(c), "cd") == 0){
		if(strcmp(msh_command_args(c)[1], "~") == 0){
			chdir(getenv("HOME"));
		} else{
			chdir(msh_command_args(c)[1]);
		}
		msh_pipeline_free(p);
		return;
	}

	//exit
	else if(strcmp(msh_command_program(c), "exit") == 0){
		exit(1);
	}

	//bg
	else if(strcmp(msh_command_program(c), "bg") == 0){
	
	}

	//fg
	else if(strcmp(msh_command_program(c), "fg") == 0){
		if(pl_count == 0){
			return;
		}
		
		if(msh_command_args(c)[1] != NULL){
			foreground = background[atoi(msh_command_args(c)[1])];
			background[atoi(msh_command_args(c)[1])] = NULL;

		}
		else{
			foreground = background[pl_count - 1];
			background[pl_count - 1] = NULL;
		}
		if(foreground == NULL){
			return;
		}
		
		cmd_count = 0;
		wait_c = msh_pipeline_command(foreground, cmd_count);
		while(wait_c != NULL){
			wait_c_pd = msh_command_getdata(wait_c);
			wait_c_pd_pid = wait_c_pd->proc_pid;
			printf("%s\n", msh_pipeline_input(foreground));
			while((data_pid = waitpid(wait_c_pd_pid, NULL, 0) == 1));
			if(errno == EINTR){
				msh_pipeline_free(foreground);
				if(pl_count > 0){
					pl_count--;
				}
				return;
			}
			cmd_count++;
			wait_c = msh_pipeline_command(foreground, cmd_count);
			
		}
		msh_pipeline_free(foreground);
		if(pl_count > 0){
			pl_count--;
		}
		return;
	
	}

	//jobs
	else if(strcmp(msh_command_program(c), "jobs") == 0){
		if(pl_count == 0){
			return;
		}
		for(unsigned int i = 0; i < MSH_MAXBACKGROUND; i++){
			if(background[i] == NULL || msh_pipeline_input(background[i]) == NULL){
				continue;
			}
			printf("[%d] %s\n", i, msh_pipeline_input(background[i]));
		}
		wait_but_dont_block();
		return;
	}
	else{
		fork_and_exec(p);
	}

	
	foreground = NULL;
	
	

	//determine what's the foreground
	if(msh_pipeline_background(p) == 0){
		foreground = p;
	}
	
	//if the pipeline is in the foreground
	if(msh_pipeline_background(p) == 1){
		//put it in the background
		background[pl_count] = p;
		pl_count++;
		
	}

	//foreground blocking wait
	if(foreground != NULL){
		foreground_wait();
	}


	//reap background
	wait_but_dont_block();

	if(foreground != NULL && p != NULL){
		msh_pipeline_free(p);
		p = NULL;
	}

	


	//free the foreground pipeline
	/*
	if(p != NULL){
		msh_pipeline_free(p);
		p = NULL;
	}
	return;
	*/
	
}
//signal handler
void sig_handler(int signal_number, siginfo_t *info, void *context){
	(void)(context);
	(void)(info);
    switch(signal_number){
    case SIGTSTP: {
		//ctrl + z
		background[pl_count] = foreground;
		pl_count++;
		foreground = NULL;

		while((data_pid = waitpid(0, NULL, WNOHANG)) != -1 && data_pid != 0){
			check_bg(data_pid);
		}
		
		//while(regular_wait() != -1);
		

		//whatever was in the foreground stop it
		fflush(stdout);
        break;
		
    }
	case SIGINT: {
		struct msh_command* c;
		struct proc_data* pd;
		if(foreground == NULL){
			return;
		}
		

		//terminate the foreground process
		size_t cmd_count = 0;
		c = msh_pipeline_command(foreground, cmd_count);
		if(c == NULL){
			return;
		}
		while(msh_command_final(c) != 1){
				
			//kill the child
			pd = msh_command_getdata(c);
			kill(pd->proc_pid, SIGTERM);

			//increment the count
			cmd_count = cmd_count + 1;
			c = msh_pipeline_command(foreground, cmd_count);
		}

		//kill the final child
		pd = msh_command_getdata(c);
		if(pd == NULL || pd->proc_pid == 0){
			return;
		}
		kill(pd->proc_pid, SIGTERM);
		foreground = NULL;
		fflush(stdout);
		break;

	}}

}

//setups a signal
void setup_signal(int signo, void (*fn)(int , siginfo_t *, void *)){
    sigset_t masked;
    struct sigaction siginfo;

    sigemptyset(&masked);
    sigaddset(&masked, signo);
    siginfo = (struct sigaction) {
        .sa_sigaction = fn,
        .sa_mask      = masked,
        .sa_flags     = SA_SIGINFO
    };
	

    if (sigaction(signo, &siginfo, NULL) == -1) {
        perror("sigaction error");
        exit(EXIT_FAILURE);
    }
}





void msh_init(void){
	//set signals
	setup_signal(SIGTSTP, sig_handler);
	setup_signal(SIGINT, sig_handler);
}

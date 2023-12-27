#include <msh.h>
#include <msh_parse.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Maximum number of background pipelines */
#define MSH_MAXBACKGROUND 16
/* each command can have MSH_MAXARGS or fewer arguments */
#define MSH_MAXARGS  16
/* each pipeline has MSH_MAXCMNDS or fewer commands */
#define MSH_MAXCMNDS 16


struct msh_command{
	//boolean value if the command is the last in the pipeline
	int last_cmd;

	//an array of strings for the args
	char* args[MSH_MAXARGS];

	//how many args we have 
	unsigned int args_count;

	struct proc_data* p_data;

};

struct msh_pipeline{
	//boolean value if the pipeline is running in the foreground or the background
	int background;

	//a string that holds the parsed pipeline
	char* parsed_cmd;

	//the file directory if we have to redirect to a file
	int open_fd;

	//the std we redirect from
	int redirect;

	//an array of commands
	struct msh_command* commands[MSH_MAXCMNDS];

	//count of the commands
	unsigned int cmd_count;

	//index counter for inserting new indices
	unsigned int cmd_index;
};

struct msh_sequence{
	//an array of pipelines
	struct msh_pipeline* pipelines[MSH_MAXBACKGROUND];

	//a count of pipelines
	unsigned int pl_count;

	//index counter for inserting new pipelines
	unsigned int pl_index;
};



//frees an individual command
static void msh_command_free(struct msh_command* c){
	//if the command is null, there's nothing to free
	if(c == NULL){
		return;
	}
	if(c->p_data != NULL){
		free(c->p_data);
	}

	//iterates through each args, frees the args if possible
	for(unsigned int i = 0; i < MSH_MAXARGS; i++){
		//check if the args is null
		if(c->args[i] != NULL){
			//free the args
			free(c->args[i]);
		}
		

		//sets the free'd args to null
		c->args[i] = NULL;
	}

	//sets the command to null when everything in the command is freed
	c = NULL;
	return;
}

//frees an individual pipeline
void msh_pipeline_free(struct msh_pipeline *p){
	//if the pipeline is null, there's nothing to free
	if(p == NULL){
		return;
	}

	//free the parsed pipeline
	if(p->parsed_cmd != NULL){
		free(p->parsed_cmd);
		p->parsed_cmd = NULL;
	}

	//iterates through each command
	for(unsigned int i = 0; i < MSH_MAXCMNDS; i++){

		//commands are queued, so if we run into null command, the subsequent commands are also null
		if(p->commands[i] == NULL){
			break;
		}

		//free the commands
		if(p->commands[i] != NULL){
			msh_command_free(p->commands[i]);
			free(p->commands[i]);
			p->commands[i] = NULL;
		}
	}

	//set the pipeline to null
	free(p);
	p = NULL;
}



//frees an individual sequence
void msh_sequence_free(struct msh_sequence *s){
	//if the sequence is null, there is nothing to free
	if(s == NULL){
		return;
	}

	//iterates through and free each pipeline
	for(unsigned int i = 0; i < MSH_MAXBACKGROUND; i++){

		//check if the pipeline is null
		if(s->pipelines[i] != NULL){
			msh_pipeline_free(s->pipelines[i]);
		}
	}

	//free the sequence
	free(s);
}

//allocates one command
struct msh_command* msh_command_alloc(){

	//calloc one command
	struct msh_command* command = calloc(1, sizeof(struct msh_command));

	//if calloc failed
	if(command == NULL){
		return NULL;
	}
	
	//intialize the defaults values
	command->args_count = 0;
	command->last_cmd = 0;

	//return the command
	return command;
}

//allocates one pipeline
static struct msh_pipeline* msh_pipeline_alloc(){

	//callocs the size of one pipeline
	struct msh_pipeline* pipeline = calloc(1, sizeof(struct msh_pipeline));

	//if calloc failed
	if(pipeline == NULL){
		return NULL;
	}

	//calloc each command in the array of commands
	for(unsigned int i = 0; i < MSH_MAXCMNDS; i++){
		pipeline->commands[i] = msh_command_alloc();
	}

	//intialize the counts
	pipeline->cmd_count = 0;
	pipeline->cmd_index = 0;

	//return the pipeline
	return pipeline;
}

//allocs an individual sequence
struct msh_sequence *msh_sequence_alloc(void){
	//calloc a sequence
	struct msh_sequence* sequence = calloc(1, sizeof(struct msh_sequence));

	//if sequence callocing failed, return
	if(sequence == NULL){
		return NULL;
	}

	//intialize the default values
	sequence->pl_count = 0;
	sequence->pl_index = 0;

	//allocates each pipeline in the pipeline alloc
	for(unsigned int i = 0; i < MSH_MAXBACKGROUND; i++){
		sequence->pipelines[i] = msh_pipeline_alloc();
		if(sequence->pipelines[i] == NULL){
			return NULL;
		}
	}

	//return the sequence
	return sequence;
}


char *msh_pipeline_input(struct msh_pipeline *p){
	return p->parsed_cmd;
}


//parses the sequence
msh_err_t msh_sequence_parse(char *str, struct msh_sequence *seq){
	if(str == NULL || seq == NULL || strcmp(str, "") == 0){
		return MSH_ERR_PIPE_MISSING_CMD;
	}
	//strdup the str so we can mutilate the str args
	char* tokenRest = strdup(str);

	//store the pointer so we can free it later (strdup mallocs space)
	char* free_ptr = tokenRest;


	//parses the sequence into pipelines
	for(char *token1 = strtok_r(tokenRest, ";", &tokenRest); token1 != NULL; token1 = __strtok_r(NULL, ";", &tokenRest)){

		//allocates space for the parsed pipeline string
		if(seq->pipelines[seq->pl_index]->parsed_cmd == NULL){
			seq->pipelines[seq->pl_index]->parsed_cmd = malloc(256);
		}

		//if malloc failed
		if(seq->pipelines[seq->pl_index]->parsed_cmd == NULL){
			return MSH_ERR_NOMEM;
		}

		strcpy(seq->pipelines[seq->pl_index]->parsed_cmd, token1);

		//last index to determine which command is the last in the pipeline
		unsigned int last_idx = 0;
		(void)last_idx;

		for(char* token2 = strtok_r(token1, "|", &token1); token2 != NULL; token2 = strtok_r(NULL, "|", &token1)){
			if(strcmp(token2, " ") == 0){
				free(free_ptr);
				return MSH_ERR_PIPE_MISSING_CMD;
			}
			
			//short hand name for the pipe index
			unsigned int pipe_idx = seq->pl_index;

			//if the parse command is a &, set the pipeline into the background
			

			for(char* token3 = strtok_r(token2, " ", &token2); token3 != NULL; token3 = strtok_r((NULL), " ", &token2)){
				//short hand name for the command index
				unsigned int cmd_idx = seq->pipelines[pipe_idx]->cmd_index;
				//number of arguments in the command to be added
				unsigned int args_cnt = seq->pipelines[pipe_idx]->commands[cmd_idx]->args_count;
				//check for bg or fg
				if(strcmp(token3, "&") == 0 && seq->pipelines[pipe_idx]->background == 1){
					free(free_ptr);
					return MSH_ERR_MISUSED_BACKGROUND;
				}
				if(strcmp(token3, "&") == 0 && seq->pipelines[pipe_idx]->parsed_cmd[strlen(seq->pipelines[pipe_idx]->parsed_cmd) - 1] != '&'){
					free(free_ptr);
					return MSH_ERR_MISUSED_BACKGROUND;
				}

				if(strcmp(token3, "&") == 0){
					seq->pipelines[pipe_idx]->background = 1;
					continue;
				}

				

				//check for 
				//add the argument to the command
				if(seq->pipelines[pipe_idx]->commands[cmd_idx]->args[args_cnt] == NULL){
					seq->pipelines[pipe_idx]->commands[cmd_idx]->args[args_cnt] = malloc(128);
				}

				//if malloc failed, return the no memory error
				if(seq->pipelines[pipe_idx]->commands[cmd_idx]->args[args_cnt] == NULL){
					free(free_ptr);
					return MSH_ERR_NOMEM;
				}

				//if there's too many args, return the max args error
				if(seq->pipelines[pipe_idx]->commands[cmd_idx]->args_count >= MSH_MAXARGS){
					free(free_ptr);
					return MSH_ERR_TOO_MANY_ARGS;
				}

				//strcpy the args into the array of args
				strcpy(seq->pipelines[pipe_idx]->commands[cmd_idx]->args[args_cnt], token3);

				//update the count
				seq->pipelines[pipe_idx]->commands[cmd_idx]->args_count = seq->pipelines[pipe_idx]->commands[cmd_idx]->args_count + 1;
				
			}

			//update the last index
			last_idx = seq->pipelines[seq->pl_index]->cmd_index;

			//update the command counts
			seq->pipelines[seq->pl_index]->cmd_count = seq->pipelines[seq->pl_index]->cmd_count + 1;
			seq->pipelines[seq->pl_index]->cmd_index = seq->pipelines[seq->pl_index]->cmd_index + 1;

			//if there's too many commands, return the max command error
			if(seq->pipelines[seq->pl_index]->cmd_count >= MSH_MAXCMNDS){
				free(free_ptr);
				return MSH_ERR_TOO_MANY_CMDS;
			}
			

		}
		//change the last command boolean value
		seq->pipelines[seq->pl_index]->commands[last_idx]->last_cmd = 1;

		//update the counts
		seq->pl_count = seq->pl_count + 1;
		seq->pl_index = seq->pl_index + 1;

	
	}

	//free the strdup
	free(free_ptr);

	//return 0 if nothing happened
	return 0;
}

//gets the first pipeline and holds it
struct msh_pipeline *msh_sequence_pipeline(struct msh_sequence *s){
	(void)s;
	//dequeue the first pipeline and hold it
	struct msh_pipeline* holder = s->pipelines[0];

	//if the holder is null, then the pipeline is empty
	if(holder == NULL){
		return NULL;
	}

	//shift everything down the queue
	for(unsigned int i = 0; i < s->pl_count - 1; i++){
		s->pipelines[i] = s->pipelines[i + 1];
	}

	//set the end to null
	s->pipelines[s->pl_count - 1] = NULL;
	

	//return the holder
	return holder;
}

//return the n-th command in a pipeline
struct msh_command *msh_pipeline_command(struct msh_pipeline *p, size_t nth){
	//if the nth command does not exist

	if(nth >= (size_t)p->cmd_count){
		return NULL;
	}

	//return the nth command
	if(p->commands[nth] != NULL){
		return p->commands[nth];
	}

	//return NULL for errors
	return NULL;
}

int msh_pipeline_background(struct msh_pipeline *p){
	return p->background;
}

int msh_command_final(struct msh_command *c){
	return c->last_cmd;
}

void msh_command_file_outputs(struct msh_command *c, char **stdout, char **stderr){
	*stdout = NULL;
	*stderr = NULL;
	int args_count = 0;
	
	char** args = msh_command_args(c);
	while(args[args_count] != NULL){
		if(strcmp(args[args_count], "1>") == 0 || strcmp(args[args_count], "1>>") == 0){
			*stdout = args[args_count + 1];
			return;
		}
		else if(strcmp(args[args_count], "2>") == 0 || strcmp(args[args_count], "2>>") == 0){
			*stderr = args[args_count + 1];
			return;
		}
		args_count++;
	}
	return;
	
}

//returns the program in a given command
char *msh_command_program(struct msh_command *c){
	//checks if the command
	if(c == NULL){
		return NULL;
	}

	//returns the first args in the command
	if(c->args[0] != NULL){
		return c->args[0];
	}

	return NULL;
}

//returns the args in the command
char ** msh_command_args(struct msh_command *c){
	(void)c;
	if(c == NULL){
		return NULL;
	}
	return c->args;
}

void msh_command_putdata(struct msh_command *c, void *data, msh_free_data_fn_t fn){
	if(c->p_data != NULL){
		fn(c->p_data);
		c->p_data = NULL;
	}
	c->p_data = data;
}

void * msh_command_getdata(struct msh_command *c){
	if(c->p_data != NULL){
		return c->p_data;
	}

	return NULL;
}

void print_sequence(struct msh_sequence* s){
	for(unsigned int i = 0; i < s->pl_count; i++){
		for(unsigned int j = 0; i < s->pipelines[i]->cmd_count; j++){
			printf("PARSED PIPELINE: %s\n", s->pipelines[i]->parsed_cmd);
			for(unsigned int k = 0; k < s->pipelines[i]->commands[j]->args_count; k++){
				printf("	ARGS: %s\n", s->pipelines[i]->commands[j]->args[k]);
			}
		}
	}
}

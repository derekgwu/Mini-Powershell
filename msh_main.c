#include <msh.h>
#include <msh_parse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "ln/linenoise.h"
#include <signal.h>
#include <sys/wait.h>
#include <ptrie.h>
#include <dirent.h>

//ptrie to hold past entries
struct ptrie* past;

//ptrie to hold path variable program
struct ptrie* path_vars;

char* difference;
int count;

void get_path_vars(){
	path_vars = ptrie_allocate();
	DIR *dr;
	struct dirent *en;
	char* path = strdup(getenv("PATH"));
	char* free_ptr;
	for(char *token1 = strtok_r(path, ":", &free_ptr); token1 != NULL; token1 = __strtok_r(NULL, ":", &free_ptr)){
		dr = opendir(token1);
		if(dr){
			while ((en = readdir(dr)) != NULL) {
            	ptrie_add(path_vars, en->d_name);
        	}
		}
		closedir(dr);
	}
	free(en);
	free(path);
}



void completion(const char *buf, linenoiseCompletions *lc) {
	if(strcmp(ptrie_autocomplete(past, buf), buf) != 0 && ptrie_autocomplete(past,buf)[0] == *buf){
		linenoiseAddCompletion(lc, ptrie_autocomplete(past, buf));
	}
	if(strcmp(ptrie_autocomplete(path_vars, buf), buf) != 0){
		linenoiseAddCompletion(lc, ptrie_autocomplete(path_vars, buf));
	}
    
	
}

char *hints(const char *buf, int *color, int *bold) {
	*color = 35;
	*bold = 0;
	if(buf == NULL || strcmp(buf, "") == 0){
		return NULL;
	}
	//try suggesting prev entry
	char* suggestion = ptrie_autocomplete(past, buf);
	if(*suggestion != *buf){
		return NULL;
	}
	if(strcmp(suggestion, buf) != 0){
		count = 0;

		//get the difference
		while(1){
			if(*(buf + count) != *(suggestion + count)){
				break;
			}
			count++;
		}
		strncpy(difference, suggestion + count, strlen(suggestion));
    	return difference;
	}

	//else try suggesting a path variable
	char* suggestion2 = ptrie_autocomplete(path_vars, buf);
	if(strcmp(suggestion2, buf) != 0){
		count = 0;

		//get the difference
		while(1){
			if(*(buf + count) != *(suggestion2 + count)){
				break;
			}
			count++;
		}
		strncpy(difference, suggestion2 + count, strlen(suggestion2));
    	return difference;
	}

	return NULL;

	
}




	
	
char *msh_input(void){
	char *line;

	/* You can change this displayed string to whatever you'd like ;-) */
	line = linenoise("(ネン) > ");
	if (line && strlen(line) == 0) {
		free(line);

		return NULL;
	}
	if (line) linenoiseHistoryAdd(line);

	return line;
}

int main(int argc, char *argv[]){
	struct msh_sequence *s;
	if (argc > 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);

		return EXIT_FAILURE;
	}
	past = ptrie_allocate();
	get_path_vars();
	difference = malloc(128);

	/*
	 * See `ln/README.markdown` for linenoise usage. If you don't
	 * see the `ln` directory, do a `make`.
	 */
	linenoiseHistorySetMaxLen(1 << 16);
	linenoiseSetCompletionCallback(completion);
	linenoiseSetHintsCallback(hints);
	msh_init();

	

	/* Lets keep getting inputs! */
	while (1){
		fflush(stdout);
		s = msh_sequence_alloc();
		if (s == NULL) {
			printf("MSH Error: Could not allocate msh sequence at initialization\n");
			return EXIT_FAILURE;
		}
		char *str;
		struct msh_pipeline *p;
		msh_err_t err;
		str = msh_input();
		
		
		if (!str){
			break; 
		} /* you must maintain this behavior: an empty command exits */
	
		err = msh_sequence_parse(str, s);
		ptrie_add(past, str);
		if (err != 0) {
			printf("MSH Error: %s\n", msh_pipeline_err2str(err));

			return err;
		}
		
		
		/* dequeue pipelines and sequentially execute them */
		while ((p = msh_sequence_pipeline(s)) != NULL) {
			msh_execute(p);	
			
		}
		//i++;
		free(str);
		msh_sequence_free(s);
		s = NULL;
	}
	//frees
	if(s != NULL){
		msh_sequence_free(s);
	}

	if(past != NULL){
		ptrie_free(past);
	}

	if(path_vars != NULL){
		ptrie_free(path_vars);
	}

	

	
	

	exit(0);
}

#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <stdio.h>
#include <assert.h>
#include <ptrie.h>

struct ptrie{
    struct ptrie_node* root;
};

struct ptrie_entry{
    struct ptrie_node* next;
    unsigned int count;
    unsigned int max;
    char* pointer; 
};


struct ptrie_node{
    struct ptrie_entry entries[256];
};


//creates the ptrie
struct ptrie *ptrie_allocate(void){
    //malloc the tree
    struct ptrie* tree = calloc(1, sizeof(struct ptrie));
    if(tree == NULL){
        return NULL;
    }

    //malloc the root of the tree
    tree->root = calloc(1, sizeof(struct ptrie_node));
    if(tree->root == NULL){
        return NULL;
    }

    //return the tree
    return tree;
}

//the recursive portion of the free method, frees a node and the node's content
static void recursive_free(struct ptrie_node *node){
    //iterate through each entry
    for(unsigned int i = 0; i < 256; i++){

        //free the string attach to an entry
        if(node->entries[i].pointer != NULL){
            free(node->entries[i].pointer);
        }

        //recurse to free the next node
        if(node->entries[i].next != NULL){
            recursive_free(node->entries[i].next);
        }
        
    }

    //free the node itself
    free(node);
}

//frees the ptrie 
void ptrie_free(struct ptrie *pt){
    //call recursive function on the root 
    if(pt->root != NULL){
        recursive_free(pt->root);
    }

    //free the ptrie pointer itself
    free(pt);
}

//this creates a new ptrie node
static struct ptrie_node* create_node(){

    //calloc a new node
    struct ptrie_node* node = calloc(1, sizeof(struct ptrie_node));

    //sanity check
    if(node == NULL){
        return NULL;
    }

    

    //return the node itself
    return node;
}

static void adjust_max(struct ptrie* pt, const char* str, int count){
     if(pt == NULL || pt->root == NULL){
        return;
    }

    //if given an invalid input
    if(*str == '\0' || str == NULL){
        return;
    }


    //create a temp node
    struct ptrie_node* temp_node = pt->root;
    //iterate through the string
    for(unsigned int i = 0; i < strlen(str); i++){

        //use the ascii value to get the index
        int index = (int)str[i];

        //if the index is invalid, there's some other issue as we cover every ascii value
        if(index < 0 || index >= 256){
            return;
        }

        //check for prior allocation issues before we do anything
        if(temp_node == NULL){
            return;
        }

        temp_node->entries[index].max = count;

        if(temp_node->entries[index].next != NULL){
            temp_node = temp_node->entries[index].next;
        }
    }
}

//given a tree and string, ptrie_add() the string to the ptrie, returns 0 for successful insertion
//returns -1 for allocation issues
int ptrie_add(struct ptrie *pt, const char *str){

    //if the tree or root is null, there was an allocation issue at ptrie_allocate()
    if(pt == NULL || pt->root == NULL){
        return -1;
    }

    //if given an invalid input
    if(*str == '\0' || str == NULL){
        return -1;
    }


    //create a temp node
    struct ptrie_node* temp_node = pt->root;
    //iterate through the string
    for(unsigned int i = 0; i < strlen(str); i++){

        //use the ascii value to get the index
        int index = (int)str[i];

        //if the index is invalid, there's some other issue as we cover every ascii value
        if(index < 0 || index >= 256){
            return -1;
        }

        //check for prior allocation issues before we do anything
        if(temp_node == NULL){
            return -1;
        }

        /*if this is the last character in the string, we append the string to the correct entry*/
        if(str[i + 1] == '\0'){
            //increase the correct entry count by 1
            temp_node->entries[index].count = temp_node->entries[index].count + 1;
            

            //allocate space for the string in the correct entry
            if(temp_node->entries[index].pointer == NULL){
                temp_node->entries[index].pointer = malloc(128);
            }

            //check for allocation issues
            if(temp_node->entries[index].pointer == NULL){
                return -1;
            }

            //copy the string to the correct entry
            strcpy(temp_node->entries[index].pointer, str);

            //readjust the max to the highest count
            if(temp_node->entries[index].count > temp_node->entries[index].max){
                adjust_max(pt, str, temp_node->entries[index].count);
            }
        }

        

        //if the next node is empty
        if(temp_node->entries[index].next == NULL){

            //create a new next for that entry
            temp_node->entries[index].next = create_node();

            //check for allocation issues
            if(temp_node->entries[index].next == NULL){
                return -1;
            }
        }

        //iterate down a level in the tree
        temp_node = temp_node->entries[index].next;

        
    }

    //return 0 if we had no issues
    return 0;
};




//given a tree and string, ptrie_autocomplete will generate a completed string based on the incomplete
//string given in th argument
char *ptrie_autocomplete(struct ptrie *pt, const char *str){

    //iterate to the end of string
    struct ptrie_node* temp_node =  pt->root;

    //declare some variables to be used outside the for loop
    size_t i = 0;
    size_t compare = -1;
    //intial traversal in the ptrie of the user input
    for(i = 0; i < strlen(str) - 1; ++i){

        //edge case if the user gave us an empty string
        if(strlen(str) - 1 == compare){
            break;
        }

        //use the ascii value to get the index
        int index = (int)str[i];

        //if the next node exists, traverse down a level in the ptrie
        if(temp_node->entries[index].next != NULL){
            temp_node = temp_node->entries[index].next;
        }
        //else the given input is not a prefix for any word in the ptrie 
        else{
            //return the user's input
            return strdup(str);
        }
    }

    //if the last letter has a string attached to it, and it's the highest count, return it
    if(temp_node->entries[(int)str[i]].count == temp_node->entries[(int)str[i]].max && 
    temp_node->entries[(int)str[i]].pointer != NULL){
        return strdup(temp_node->entries[(int)str[i]].pointer);
    }

    //iterate down another level
    if(temp_node->entries[(int)str[i]].next != NULL){
        temp_node = temp_node->entries[(int)str[i]].next;
    } 

    
    //iterate through each entry in the node to find the alphabetically correct autocomplete
    //entry
    unsigned int largest_max = 0;
    int largest_max_index = 0;

    //iterate through each entry to find the highest max
    for(unsigned int i = 0; i < 256; i++){

        //if its the highest max we have seen so far, save the index
        if(temp_node->entries[i].max > largest_max){
            largest_max = temp_node->entries[i].max;
            largest_max_index = i;
        }

        //on the last iteration take necessary actions
        if(i == 255){
            //if the highest max is also the highest count, return the string attached at that entry
            if(temp_node->entries[largest_max_index].max == temp_node->entries[largest_max_index].count
            && temp_node->entries[largest_max_index].pointer != NULL
            && strlen(temp_node->entries[largest_max_index].pointer) != strlen(str)){
                return strdup(temp_node->entries[largest_max_index].pointer);
            }

            //else if the largest max has a next pointer, traverse down a level
            if(temp_node->entries[largest_max_index].next != NULL){
                temp_node = temp_node->entries[largest_max_index].next;
                largest_max = 0;
                largest_max_index = 0;
                i = 0;

            }
        }
        
    }

    //if we exit the loop, it means we found no autocompletable entry for the users input, so we
    //return the users input
    return strdup(str);
}

//this is the recursive portion of the ptrie_print
static void recursive_print(struct ptrie_node* node){

    //iterate through the ptrie_node
    for(unsigned int i = 0; i < 256; i++){

        //print out any entry we see
        if(node->entries[i].pointer != NULL){
            printf("%s \n\n", node->entries[i].pointer);
        }

        //traverse down any next pointer we see
        if(node->entries[i].next != NULL){
            recursive_print(node->entries[i].next);
        }
    }
    return;
}



void ptrie_print(struct ptrie *pt){
    for(unsigned int i = 0; i < 256; ++i){
        if(pt->root->entries[i].next != NULL){
            recursive_print(pt->root);
        }
    }
    return;

}

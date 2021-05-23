#include "svc.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "queue.h"
#include "structures.h"

#define MAX_QUEUE_SIZE 200


int num_bytes(FILE* file);
char* get_commit_id(struct Commit* commit, struct Changes* changes, size_t num_changes);
void add_to_parent(struct Commit* child, struct Commit* parent);
void post_order_recursion(struct Commit* commit, struct Queue* queue);
void organise_files(struct System* system, int index);
struct Changes* detect_changes(struct System* system, size_t* num_changes);
int check_validity(char* name);
int check_uncommitted_changes(struct System* system);
int store_content(struct System* system, FILE* file);
void resolve_file_clashes(struct System* system, struct resolution *resolutions, int n_resolutions);
int check_branch_for_file(struct System* system, size_t branch, char* file_name);

void *svc_init(void) {
    
    struct System* system = (struct System*)malloc(sizeof(struct System));

    // Initialise commits
    system->initial_commit = NULL;
    system->head_commit = NULL;

    // Initialise files
    // Allocate enough space for 1 struct File pointer
    system->files = (struct File**)malloc(sizeof(struct File*));

    // Allocate space inside that pointer to store a single struct File
    system->files[0] = (struct File*)malloc(sizeof(struct File));
    system->num_files = (size_t*)malloc(sizeof(size_t));
    system->num_files[0] = 0;
    system->cap_files = (size_t*)malloc(sizeof(size_t));
    system->cap_files[0] = 1;

    // Initialise file_contents
    system->file_contents = (char**)malloc(sizeof(char*));
    system->num_content = 0;
    system->cap_content = 1;

    // Initialise branches
    system->num_branches = 1;
    system->branches = (char**)malloc(sizeof(char*));
    system->branches[0] = strdup("master");
    system->active_branch_id = 0;
    system->branch_ptrs = (struct Commit**)malloc(sizeof(struct Commit*));


    return system;
}

void cleanup(void *helper) {

    struct System* system = (struct System*)helper;

    // Clean up file allocations
    for(int h = 0; h < system->num_branches; h++){


        for(int i = 0; i < system->num_files[h]; i++){

            free(system->files[h][i].file_name);
        }

        free(system->files[h]);


    }

    free(system->num_files);

    free(system->cap_files);

    free(system->files);

    // Clean up file_content allocations
    for(int v = 0; v < system->num_content; v++){

        free(system->file_contents[v]);

    }

    free(system->file_contents);


    // Clean up branch allocations
    for(int j = 0; j < system->num_branches; j++){
        free(system->branches[j]);
    }

    free(system->branches);

    free(system->branch_ptrs);


    // Clean up commit allocations

    struct Queue* queue = createQueue(MAX_QUEUE_SIZE);

    post_order_recursion(system->initial_commit, queue);

    while(!isEmpty(queue)){

        struct Commit* cursor = dequeue(queue);


        free(cursor->message);

        free(cursor->id);

        if(cursor->child_allocated){
            free(cursor->child_commits);
        }

        for(int i = 0; i < cursor->num_files; i++){
            free(cursor->files[i].file_name);
        }

        free(cursor->files);

        for(int j = 0; j < cursor->num_changes; j++){
            free(cursor->changes[j].file_name);
        }

        free(cursor->changes);

    }

    free(system->initial_commit);

    free(system);

    freeQueue(queue);


}

// Post order recursion using ciruclar queue
void post_order_recursion(struct Commit* commit, struct Queue* queue){

    if(commit == NULL){
        return;
    }

    for(int i = 0; i < commit->num_childs; i++){

        post_order_recursion(&commit->child_commits[i], queue);
    }

    enqueue(queue, commit);

    return;

}

// File hashing algorithm as described 
int hash_file(void *helper, char *file_path) {
    
    if(file_path == NULL){
        return -1;
    }

    FILE* file = fopen(file_path, "rb");

    if(file == NULL){
        return -2;
    }

    // Hashing algorithm as described

    int file_length = num_bytes(file);

    size_t hash = 0;

    for(int i = 0; i < strlen(file_path); i++){
        hash = (hash + file_path[i]) % 1000;
    }


    for(int j = 0; j < file_length; j++){
        
        size_t buf = fgetc(file);

        hash = (hash + buf) % 2000000000;

    }

    fclose(file);
    

    return hash;
}

// Return the number of bytes inside file
int num_bytes(FILE* file){

    fseek(file, 0, SEEK_SET);

    int length;

    fseek(file, 0, SEEK_END);

    length = ftell(file);

    fseek(file, 0, SEEK_SET);

    return length;

}


char *svc_commit(void *helper, char *message) {

    struct System* system = (struct System*)helper;

    int branch = system->active_branch_id;

    if(message == NULL){
        return NULL;
    }


    size_t num_changes = 0;
    struct Changes* changes = detect_changes(system, &num_changes);

    if(num_changes == 0){
        free(changes);
        return NULL;
    }



    struct Commit* commit = NULL;

    // Check whether the first commit has occured
    if(system->initial_commit == NULL){

        // If first commit has not occured
        // Initialise commit structure

        system->initial_commit = (struct Commit*)malloc(sizeof(struct Commit)); 
        system->head_commit = system->initial_commit;
        commit = system->initial_commit;
        commit->branch_name = system->branches[0];
        commit->branch_id = branch;
        commit->parent_commit = NULL;
        commit->num_parents = 0;

    } else {

        // First commit has occured before this
        // Append to parent commit

        struct Commit* parent = system->head_commit;

        // There has been a previous commit
        // Check if there are childs
        if(!parent->child_allocated){
            // If no children has been allocated before for this commit
            // We need to malloc
            parent->child_commits = (struct Commit*)malloc(sizeof(struct Commit));
            parent->child_allocated = true;
        } else {
            // If there has been children allocated to this commit before
            // We need to realloc

            struct Commit** child_pointers;

            size_t num_child = parent->num_childs;

            child_pointers = (struct Commit**)malloc(sizeof(struct Commit*)*num_child);

            for(int k = 0; k < num_child; k++){
                
                child_pointers[k] = &parent->child_commits[k];

            }

            parent->child_commits = (struct Commit*)realloc(parent->child_commits, sizeof(struct Commit)*(num_child+1));
        
            // Check if any of the child_commits are branch_ptrs
            // And update the branch_ptrs if they were

            for(int i = 0; i < system->num_branches; i++){

                for(int j = 0; j < num_child; j++){

                    if(system->branch_ptrs[i] == child_pointers[j]){
                        // Child j is a branch_ptr for branch i
                        // Update the branch_ptrs to point at the new position
                        system->branch_ptrs[i] = &parent->child_commits[j];

                    }

                }

            }


            free(child_pointers);


        }

        // Store the new commit as a child for the head_commit
        commit = &parent->child_commits[parent->num_childs];
        parent->num_childs++;

        // Store the current head commit as the parent of our new commit
        commit->parent_commit = parent;
        commit->num_parents = 1;

        // Make the current commit as the new head commit
        system->head_commit = commit;

        commit->branch_id = branch;
        
    }

    // Begin common set up despite status of initial commit
    // update all fields of the commit struct

    commit->message = strdup(message);

    commit->files = (struct File*)malloc(sizeof(struct File)*system->num_files[branch]);

    memcpy(commit->files, system->files[branch], sizeof(struct File)*system->num_files[branch]);

    commit->num_files = system->num_files[branch];

    // String duplicate all filenames across
    for(int i = 0; i < commit->num_files; i++){
        commit->files[i].file_name = strdup(system->files[branch][i].file_name);
        commit->files[i].hash = hash_file(system, commit->files[i].file_name);
    }

    commit->child_commits = NULL;

    commit->num_childs = 0;

    commit->id = get_commit_id(commit, changes, num_changes);

    commit->changes = changes;

    commit->num_changes = num_changes;

    // Update branch ptr for this branch
    system->branch_ptrs[branch] = commit;

    return commit->id;
}

// Reorder the struct Changes array into alphabetical order
void sort_changes(struct Changes* changes, size_t num_changes){
    
    // Perform bubble sort
    bool sorted = false;

    while(!sorted){

        sorted = true;

        for(size_t i = 0; i < num_changes - 1; i++){

            if(strcasecmp(changes[i].file_name, changes[i+1].file_name) > 0){
                sorted = false;

                // Swap their positions
                struct Changes temp;
                memcpy(&temp, &changes[i], sizeof(struct Changes));
                memcpy(&changes[i], &changes[i+1], sizeof(struct Changes));
                memcpy(&changes[i+1], &temp, sizeof(struct Changes));

            }


        }


    }


}

// Detect changes between head commit and current state of the system on the active branch
struct Changes* detect_changes(struct System* system, size_t* num_changes){

    int branch = system->active_branch_id;

    struct Changes* changes = (struct Changes*)malloc(sizeof(struct Changes));

    size_t change_count = 0;

    // If the head_commit is null, that should mean this is the first commit
    // Therefore all current files are new, and no deletion and modifications needs to be checked
    if(system->head_commit == NULL){
        
        int num_checks = system->num_files[branch];

        int check_count = 0;

        int index = 0;

        while(check_count < num_checks){

            // Check if file still exists
            FILE* file_check = fopen(system->files[branch][index].file_name, "rb");

            if(file_check != NULL){

                // File still exists

                // Append into name and change into changes array
                changes = (struct Changes*)realloc(changes, (change_count+1) * sizeof(struct Changes));
                changes[change_count].file_name = strdup(system->files[branch][index].file_name);
                changes[change_count].deletion = false;
                changes[change_count].addition = true;
                changes[change_count].modification = false;
                change_count++;
                
                index++;

                check_count++;

                fclose(file_check);

            } else {
                // File has been removed oustide of SVC

                // Remove file from svc system
                svc_rm(system, system->files[branch][index].file_name);

                check_count++;

            }



        }


        memcpy(num_changes, &change_count, sizeof(size_t));

        // If there is more than 1 change, sort the array
        if(change_count > 1){
            sort_changes(changes, change_count);
        }

        return changes;

    }


    // If the head commit was not null
    // This indicates that this is not the first commit
    // Perform checks for the file against the head commit

    // Detect removals from svc
    for(int i = 0; i < system->head_commit->num_files; i++){

        bool file_found = false;

        for(int j = 0; j < system->num_files[branch]; j++){

            if(strcmp(system->files[branch][j].file_name, system->head_commit->files[i].file_name) == 0){
                // Found equivalent
                file_found = true;
                break;
            }


        }

        if(!file_found){
            // A deletion has occured
            // Append into name and change into changes array
            changes = (struct Changes*)realloc(changes, (change_count+1) * sizeof(struct Changes));
            changes[change_count].file_name = strdup(system->head_commit->files[i].file_name);
            changes[change_count].deletion = true;
            changes[change_count].addition = false;
            changes[change_count].modification = false;
            change_count++;

        }


    }

    // Detect removal outside svc

    for(int m = 0; m < system->head_commit->num_files; m++){
        
        FILE* file_check = fopen(system->head_commit->files[m].file_name, "rb");

        if(file_check == NULL){

            // A force removal has occured
            // Add this as a change
            changes = (struct Changes*)realloc(changes, (change_count+1) * sizeof(struct Changes));
            changes[change_count].file_name = strdup(system->head_commit->files[m].file_name);
            changes[change_count].deletion = true;
            changes[change_count].addition = false;
            changes[change_count].modification = false;
            change_count++;

            svc_rm(system, system->head_commit->files[m].file_name);

        } else {
            fclose(file_check);
        }


    }


    // Detect additions
    for(int i = 0; i < system->num_files[branch]; i++){

        bool file_found = false;

        for(int j = 0; j < system->head_commit->num_files; j++){

            if(strcmp(system->files[branch][i].file_name, system->head_commit->files[j].file_name) == 0){
                // Found equivalent
                file_found = true;
                break;
            }


        }

        if(!file_found){
            // An addition has occured
            //Check if file still exists
            FILE* file_check = fopen(system->files[branch][i].file_name,"rb");

            if(file_check != NULL){

                // File still exists

                // Append into name and change into changes array
                changes = (struct Changes*)realloc(changes, (change_count+1) * sizeof(struct Changes));
                changes[change_count].file_name = strdup(system->files[branch][i].file_name);
                changes[change_count].deletion = false;
                changes[change_count].addition = true;
                changes[change_count].modification = false;
                change_count++;

                int hash_check = hash_file(system, system->files[branch][i].file_name);

                // Check whether content has been updated since added
                if(system->files[branch][i].hash != hash_check){
                    // The file has been changed since added
                    // Store this version of the file into the system
                    FILE* file = fopen(system->files[branch][i].file_name, "rb");
                    // Update the pointer to file content in the system
                    system->files[branch][i].fc_index = store_content(system, file);
                    system->files[branch][i].fc_length = num_bytes(file);
                    fclose(file);
                    // Store the new hash into the file
                    system->files[branch][i].hash = hash_check;
                }

                fclose(file_check);

            } else {
                // File has been removed manually
                // Remove file from svc system
                svc_rm(system, system->files[branch][i].file_name);

            }


        }


    }

    // Detect modifications
    for(int i = 0; i < system->num_files[branch]; i++){

        // bool modified = false;

        for(int j = 0; j < system->head_commit->num_files; j++){

            if(strcmp(system->files[branch][i].file_name, system->head_commit->files[j].file_name) == 0){
                // Found file with the same name
                
                int system_hash = hash_file(system, system->files[branch][i].file_name);
                int head_hash = system->head_commit->files[j].hash;


                if(system_hash != head_hash){
                    // Found modified file
                    
                    changes = (struct Changes*)realloc(changes, (change_count+1) * sizeof(struct Changes));
                    changes[change_count].file_name = strdup(system->head_commit->files[j].file_name);
                    changes[change_count].deletion = false;
                    changes[change_count].addition = false;
                    changes[change_count].modification = true;

                    changes[change_count].prev_hash = head_hash;
                    changes[change_count].new_hash = system_hash;

                    change_count++;

                    // Store this version of the file into the system
                    FILE* file = fopen(system->files[branch][i].file_name, "rb");
                    // Update the pointer to file content in the system
                    system->files[branch][i].fc_index = store_content(system, file);
                    system->files[branch][i].fc_length = num_bytes(file);
                    fclose(file);

                    // Store the new hash into the file
                    system->files[branch][i].hash = system_hash;

                }
            }


        }


    }


    memcpy(num_changes, &change_count, sizeof(size_t));

    if(change_count > 1){
        sort_changes(changes, change_count);
    }

    return changes;



}


// Perform hash algorithm as prescribed
char* get_commit_id(struct Commit* commit, struct Changes* changes, size_t num_changes){

    char* hex_id = (char*)malloc(7);

    int id = 0;

    for(int i = 0; i < strlen(commit->message); i++){
        id = (id + commit->message[i]) % 1000;
    }


    for(int j = 0; j < num_changes; j++){

        if(changes[j].addition){
            id = id + 376591;
        } else if (changes[j].deletion){
            id = id + 85973;
        } else if (changes[j].modification){
            id = id + 9573681;
        }

        for(int k = 0; k < strlen(changes[j].file_name); k++){
            id = (id * (changes[j].file_name[k] % 37)) % 15485863 + 1;
        }


    }

    sprintf(hex_id, "%06x", id);

    return hex_id;


}

// Retrieve pointer to a commit given the commit id
void *get_commit(void *helper, char *commit_id) {

    struct System* system = (struct System*)helper;
    
    if(commit_id == NULL || system->initial_commit == NULL){
        return NULL;
    }

    // Perform post order recursion on the commit tree

    struct Queue* queue = createQueue(MAX_QUEUE_SIZE);

    struct Commit* result = NULL;

    enqueue(queue, system->initial_commit);

    while(!isEmpty(queue)){

        struct Commit* cursor = dequeue(queue);

        // Looking for a matching commit id
        if(strcmp(cursor->id, commit_id) == 0){
            // Found a commit with matching id
            result = cursor;
            break;
        } else {
            // The current cursor does not match
            for(int i = 0; i < cursor->num_childs; i++){
                enqueue(queue, &(cursor->child_commits[i]));
            }


        }

    }

    
    if(result == NULL){
        // Given ID does not exist
        
        freeQueue(queue);

        return NULL;
    }


    freeQueue(queue);


    return result;
}

// Retrieve the immediate parents
char **get_prev_commits(void *helper, void *commit, int *n_prev) {

    struct Commit* com = (struct Commit*)commit;

    if(n_prev == NULL){
        return NULL;
    }

    if(com == NULL){
        *n_prev = 0;
        return NULL;
    }

    if(com->num_parents == 0){
        *n_prev = 0;
        return NULL;
    }

    // Allocate space into prev_commits
    char** prev_commits = (char**)malloc(sizeof(char*)*com->num_parents);

    *n_prev = com->num_parents;

    if(com->num_parents == 1){
        prev_commits[0] = com->parent_commit->id;
    } else if (com->num_parents == 2){
        prev_commits[0] = com->parent_commit->id;
        prev_commits[1] = com->parent_commit2->id;
    }

    return prev_commits;
}

// Print out relevant info for the commit
void print_commit(void *helper, char *commit_id) {


    struct System* system = (struct System*)helper;

    if(commit_id == NULL){
        printf("Invalid commit id\n");
        return;
    }

    struct Commit* commit = get_commit(helper, commit_id);
    
    if(commit == NULL){
        printf("Invalid commit id\n");
        return;
    }

    // Print commit information
    printf("%s [%s]: %s\n", commit->id, system->branches[commit->branch_id], commit->message);

    for(int i = 0; i < commit->num_changes; i++){

        printf("    ");

        if(commit->changes[i].addition){
            printf("+ %s\n", commit->changes[i].file_name);
        } else if (commit->changes[i].deletion){
            printf("- %s\n", commit->changes[i].file_name);  
        } else if (commit->changes[i].modification){
            printf("/ %s [% 10d -> % 10d]\n", commit->changes[i].file_name, commit->changes[i].prev_hash, commit->changes[i].new_hash);
        }


    }

    printf("\n");

    // Print tracked files
    printf("    ");
    printf("Tracked files (%zu):\n", commit->num_files);
    for(int j = 0; j < commit->num_files; j++){
        printf("    ");
        printf("[% 10ld] %s\n", commit->files[j].hash, commit->files[j].file_name);
    }




}

// Create a new branch using branch_name
int svc_branch(void *helper, char *branch_name) {


    struct System* system = (struct System*)helper;

    if(branch_name == NULL){
        return -1;
    }

    int valid_name = check_validity(branch_name);


    if(!valid_name){
        return -1;
    }

    for(int i = 0; i < system->num_branches; i++){

        if(strcmp(system->branches[i], branch_name) == 0){
            return -2;
        }
    }

    int made_changes = check_uncommitted_changes(system);

    if(made_changes){

        return -3;
    }

    int branch = system->active_branch_id;

    // Can begin branching at this point
    system->num_branches++;
    
    // Branch index
    int b_idx = system->num_branches-1;

    // Allocate space for the new branch
    system->files = (struct File**)realloc(system->files, sizeof(struct File*)*system->num_branches);
    system->files[system->num_branches -1] = (struct File*)malloc(sizeof(struct File)*system->cap_files[branch]);
    system->num_files = (size_t*)realloc(system->num_files, sizeof(size_t)*system->num_branches);
    system->cap_files = (size_t*)realloc(system->cap_files, sizeof(size_t)*system->num_branches);

    // Copy all the content over into the new branch
    memcpy(system->files[b_idx], system->files[branch], sizeof(struct File*)*system->num_files[branch]);
    memcpy(&system->num_files[b_idx], &system->num_files[branch], sizeof(size_t));
    memcpy(&system->cap_files[b_idx], &system->cap_files[branch], sizeof(size_t));
    
    for(int j = 0; j < system->num_files[branch]; j++){

        system->files[b_idx][j].file_name = strdup(system->files[branch][j].file_name);
        system->files[b_idx][j].hash = system->files[branch][j].hash;
        system->files[b_idx][j].fc_index = system->files[branch][j].fc_index;
        system->files[b_idx][j].fc_length = system->files[branch][j].fc_length;


    }


    // Alloate space for new branch_ptrs
    system->branch_ptrs = (struct Commit**)realloc(system->branch_ptrs, sizeof(struct Commit*)*system->num_branches);
    // Update the pointer value to current head commit
    system->branch_ptrs[system->num_branches - 1] = system->head_commit;

    // Allocate space for new branch name
    system->branches = (char**)realloc(system->branches, sizeof(char*)*system->num_branches);
    system->branches[system->num_branches - 1] = strdup(branch_name);

    return 0;
}

// Function to check branch name contains only
// Valid characters
int check_validity(char* name){

    for(int i = 0; i < strlen(name); i++){

        if(name[i] >= 65 && name[i] <= 90){
            continue;
        } else if(name[i] >= 97 && name[i] <= 122){
            continue;
        } else if (name[i] >= 48 && name[i] <= 57 ){
            continue;
        } else if(name[i] == '-' || name[i] == '_' || name[i] == '/'){
            continue;
        } else {
            return 0;
        }

    }

    return 1;


}


// Return 1 if there are uncommited changes,
// Return 0 if there are no changes
int check_uncommitted_changes(struct System* system){


    int branch = system->active_branch_id;


    // If the head_commit is null, that should mean this is the first commit
    // Therefore all current files are new, and no deletion and modifications needs to be checked
    if(system->head_commit == NULL){

        return 0;

    }


    // Detect removals from svc
    for(int i = 0; i < system->head_commit->num_files; i++){

        bool file_found = false;

        for(int j = 0; j < system->num_files[branch]; j++){

            if(strcmp(system->files[branch][j].file_name, system->head_commit->files[i].file_name) == 0){
                // Found equivalent
                file_found = true;
                break;
            }


        }

        if(!file_found){
            // A deletion has occured
            // Append into name and change into changes array
            return 1;
        }


    }

    // Detect removal outside svc

    for(int m = 0; m < system->head_commit->num_files; m++){
        
        FILE* file_check = fopen(system->head_commit->files[m].file_name, "rb");

        if(file_check == NULL){

            // A force removal has occured
            return 1;


        } else {
            fclose(file_check);
        }


    }


    // Detect additions
    for(int i = 0; i < system->num_files[branch]; i++){

        bool file_found = false;

        for(int j = 0; j < system->head_commit->num_files; j++){

            if(strcmp(system->files[branch][i].file_name, system->head_commit->files[j].file_name) == 0){
                // Found equivalent
                file_found = true;
                break;
            }


        }

        if(!file_found){
            // An addition has occured
            // MARK: Check if file still exists
            FILE* file_check = fopen(system->files[branch][i].file_name,"rb");

            if(file_check != NULL){

                // File still exists
                fclose(file_check);

                return 1;
            } else {

                // File has been removed manually
                // Remove file from svc system
                svc_rm(system, system->files[branch][i].file_name);

            }


        }


    }

    // Detect modifications
    for(int i = 0; i < system->num_files[branch]; i++){

        // bool modified = false;

        for(int j = 0; j < system->head_commit->num_files; j++){

            if(strcmp(system->files[branch][i].file_name, system->head_commit->files[j].file_name) == 0){
                // Found file with the same name
                
                int system_hash = hash_file(system, system->files[branch][i].file_name);
                int head_hash = system->head_commit->files[j].hash;

                if(system_hash != head_hash){
                    // Found modified file

                    return 1;

                }
            }


        }


    }



    return 0;

}

// Check out given branch name
int svc_checkout(void *helper, char *branch_name) {


    struct System* system = (struct System*)helper;

    if(branch_name == NULL){
        return -1;
    }

    // Check through all existing branches for matching branch name
    size_t branch_id = -1;
    for(size_t i = 0; i < system->num_branches; i++){

        if(strcmp(branch_name, system->branches[i]) == 0){

            branch_id = i;
            break;
        }

    }

    if(branch_id == -1){
        // No branch with this name exists
        return -1;
    }

    int made_changes = check_uncommitted_changes(system);

    if(made_changes){
        return -2;
    }

    // This branch exists without uncommitted changes
    // Check out branch

    system->active_branch_id = branch_id;
    system->head_commit = system->branch_ptrs[branch_id];

    struct Commit* commit = system->head_commit;


    // Replace all files shared by both branches to make sure 
    // each file the contain the content of
    // The most recent commit on this branch
    for(int i = 0; i < commit->num_files; i++){

        FILE* file = fopen(commit->files[i].file_name, "w");


        char* file_content = system->file_contents[commit->files[i].fc_index];

        int num_elm = commit->files[i].fc_length;


        fwrite(file_content, 1, num_elm, file);

        fflush(file);

        fclose(file);

    }
    


    return 0;
}

// Print all branches created
char **list_branches(void *helper, int *n_branches) {

    if(n_branches == NULL){
        return NULL;
    }
    
    struct System* system = (struct System*)helper;
    
    // For each branch, print out their name
    for(int i = 0; i < system->num_branches; i++){
        
        printf("%s\n", system->branches[i]);

    }

    char** branches = (char**)malloc(sizeof(char*)*system->num_branches);

    for(int j = 0; j < system->num_branches; j++){

        branches[j] = system->branches[j];

    }

    *n_branches = system->num_branches;

    return branches;
}

// Begin tracking given file in the svc system
int svc_add(void *helper, char *file_name) {

    struct System* system = (struct System*)helper;
    
    int branch = system->active_branch_id;

    if(file_name == NULL){
        return -1;
    }

    for(int i = 0; i < system->num_files[branch]; i++){

        if(strcmp(system->files[branch][i].file_name, file_name) == 0){
            // Found a file with the same name
            return -2;
        }

    }

    FILE* file = fopen(file_name, "rb");

    if(file == NULL){
        return -3;
    }

    // Add to SVC system

    // Add to list of tracked files
    if(system->num_files[branch] == system->cap_files[branch]){
        // We need to expand the capacity of the files array
        // We realloc double the space because its generally efficient
        system->files[branch] = (struct File*)realloc(system->files[branch], sizeof(struct File)*system->cap_files[branch]*2);
        system->cap_files[branch] = system->cap_files[branch]*2;
    }

    // Now that the files array have enough space
    // Initialise the struct we are going to use
    struct File* new_file = &system->files[branch][system->num_files[branch]];
    new_file->hash = (size_t)hash_file(helper, file_name);
    new_file->file_name = strdup(file_name);

    system->num_files[branch]++;

    // Copy the file and it's contents, into our system version controls
    new_file->fc_index = store_content(system, file);

    new_file->fc_length = num_bytes(file);

    fclose(file);
    
    return new_file->hash;
}

// Store the content of this file into system->file_contents
// And return the index where the content is stored inside file_contents array
int store_content(struct System* system, FILE* file){

    // Reallocate system->file_contents array
    if(system->cap_content == system->num_content){
        system->file_contents = (char**)realloc(system->file_contents, sizeof(char*)*(system->num_content*2));
        system->cap_content = system->cap_content * 2;;
    }

    size_t length = num_bytes(file);

    system->file_contents[system->num_content] = (char*)malloc(sizeof(char)*(length+1));

    if(system->file_contents[system->num_content] != NULL){
        fread(system->file_contents[system->num_content], 1, length, file);
    }

    // Null terminate the file_content
    system->file_contents[system->num_content][length] = '\0';

    system->num_content++;

    return system->num_content-1;

}

// Stop tracking given file
int svc_rm(void *helper, char *file_name) {

    struct System* system = (struct System*)helper;

    int branch = system->active_branch_id;
    

    if(file_name == NULL){
        return -1;
    }

    int rm_hash = 0;


    for(int i = 0; i < system->num_files[branch]; i++){
        if(strcmp(system->files[branch][i].file_name, file_name) == 0){
            // We found the file we wanted to remove from the SVC system

           rm_hash = system->files[branch][i].hash;

           free(system->files[branch][i].file_name);

            // Close the gap created by moving all the files forward by 1
            organise_files(system, i);

            system->num_files[branch]--;

        }
    }

    if(rm_hash == 0){
        return -2;
    }


    return rm_hash;
}

// Remove the gaps in the file array created by svc_rm
void organise_files(struct System* system, int index){

    int branch = system->active_branch_id;

    for(int i = index; i < system->num_files[branch] - 1; i++){

        memcpy(&system->files[branch][i], &system->files[branch][i+1], sizeof(struct File));
        system->files[branch][i].fc_index = system->files[branch][i+1].fc_index;
        system->files[branch][i].fc_length = system->files[branch][i+1].fc_length;
        system->files[branch][i].hash = system->files[branch][i+1].hash;

    }

}

// Reset to give commit 
int svc_reset(void *helper, char *commit_id) {

    struct System* system = (struct System*)helper;
    
    if(commit_id == NULL){
        return -1;
    }

    struct Commit* commit = get_commit(helper, commit_id);

    if(commit == NULL){
        return -2;
    }

    size_t branch = system->active_branch_id;

    // Update head commit and branch ptrs to the reset commit
    system->head_commit = commit;
    system->branch_ptrs[branch] = commit;

    // For every file that is tracked by this commit
    // Revert all changes by writing this copy of the file into the drive

    for(int i = 0; i < commit->num_files; i++){

        FILE* file = fopen(commit->files[i].file_name, "w");

        char* file_content = system->file_contents[commit->files[i].fc_index];

        int num_elm = commit->files[i].fc_length;

        fwrite(file_content, 1, num_elm, file);

        fflush(file);

        fclose(file);

    }

    // Next update the system->files[branch] to be the same this commit's files
    // First free this branch entirely

    for(int i = 0; i < system->num_files[branch]; i++){

        free(system->files[branch][i].file_name);
    }

    system->files[branch] = (struct File*)realloc(system->files[branch], sizeof(struct File)*commit->num_files);
    memcpy(system->files[branch], commit->files, sizeof(struct File)*commit->num_files);
    // STRDUP all the file names
    for(int j = 0; j < commit->num_files; j++){

        system->files[branch][j].file_name = strdup(commit->files[j].file_name);

    }

    // Then we reallocate the number of files according to our commit
    system->num_files[branch] = commit->num_files;
    system->cap_files[branch] = commit->num_files;


    return 0;
}


// Merge given branch into the active branch
char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {

    struct System* system = (struct System*)helper;

    if(branch_name == NULL){
        printf("Invalid branch name\n");
        return NULL;
    }

    size_t small_branch = -1;
    size_t main_branch = system->active_branch_id;

    for(size_t i = 0; i < system->num_branches; i++){

        if(strcmp(branch_name, system->branches[i]) == 0){
            // Found the branch
            small_branch = i;
        }

    }

    if(small_branch == -1){
        printf("Branch not found\n");
        return NULL;
    }

    if(small_branch == main_branch){
        printf("Cannot merge a branch with itself\n");
        return NULL;
    }

    int made_changes = check_uncommitted_changes(system);
    if(made_changes){
        printf("Changes must be committed\n");
        return NULL;
    }

    // Begin merging procedure
    // Add all the files from small branch into main branch
    for(int i = 0; i < system->num_files[small_branch]; i++){

        int file_index = check_branch_for_file(system, main_branch, system->files[small_branch][i].file_name);

        if (file_index == -1) {

            // File was not found in main branch

            // Add file from small branch into main branch
            if(system->num_files[main_branch] == system->cap_files[main_branch]){
                // Reallocate space for new file to be added
                system->files[main_branch] = (struct File*)realloc(system->files[main_branch], sizeof(struct File)*system->cap_files[main_branch]*2);
                system->cap_files[main_branch] = system->cap_files[main_branch]*2;
            }


            // Begin adding files
            int file_index = system->num_files[main_branch];
            // system->files[main_branch][file_index] = 

            memcpy(&system->files[main_branch][file_index], &system->files[small_branch][i], sizeof(struct File));
            system->files[main_branch][file_index].file_name = strdup(system->files[small_branch][i].file_name);
            system->files[main_branch][file_index].fc_index = system->files[small_branch][i].fc_index;
            system->files[main_branch][file_index].fc_length = system->files[small_branch][i].fc_length;
            system->files[main_branch][file_index].hash = system->files[small_branch][i].hash;

            // Write these files into the main_branch
            char* file_name = system->files[main_branch][file_index].file_name;

            FILE* file = fopen(file_name, "w");

            char* file_content = system->file_contents[system->files[main_branch][file_index].fc_index];

            int num_elm = system->files[main_branch][file_index].fc_length;

            fwrite(file_content, 1, num_elm, file);

            fflush(file);

            fclose(file);

            system->num_files[main_branch]++;

        } else {

            // File was also in main branch
            system->files[main_branch][file_index].fc_index = system->files[small_branch][i].fc_index;
            system->files[main_branch][file_index].fc_length = system->files[small_branch][i].fc_length;
            system->files[main_branch][file_index].hash = system->files[small_branch][i].hash;

            // Write these files into the main_branch
            char* file_name = system->files[main_branch][file_index].file_name;

            FILE* file = fopen(file_name, "w");

            char* file_content = system->file_contents[system->files[main_branch][file_index].fc_index];

            int num_elm = system->files[main_branch][file_index].fc_length;

            fwrite(file_content, 1, num_elm, file);

            fflush(file);

            fclose(file);


        }


    }

    // Handle all resolutions

    resolve_file_clashes(system, resolutions, n_resolutions);

    // Merge completed
    // Call commit for merged branch 
    char* prefix = "Merged branch ";

    char* message = (char*)malloc(strlen(prefix) + strlen(branch_name) + 1);

    strcpy(message, prefix);

    strcat(message, branch_name);

    char* commit_id = svc_commit(system, message);

    struct Commit* commit = get_commit(system, commit_id);

    commit->parent_commit2 = system->branch_ptrs[small_branch];

    commit->num_parents = 2;

    free(message);

    printf("Merge successful\n");

    return commit_id;
}

// Handle all resolutions given
void resolve_file_clashes(struct System* system, struct resolution *resolutions, int n_resolutions){

    int branch = system->active_branch_id;
    // Check through the resolutions for file_name.
    
    // For each resolution given
    for(int i = 0; i < n_resolutions; i++){

        // If the file path is NULL
        if(resolutions[i].resolved_file == NULL){
            // Remove file from SVC
            svc_rm(system, resolutions[i].file_name);
            continue;
        }

        FILE* file_ptr = fopen(resolutions[i].resolved_file, "rb");

        // Store the file content in to the system
        // Also update the file_content pointer to the new content
        int file_index = -1;
        int num_files = system->num_files[branch]; 

        for(int j = 0; j < num_files; j++){
            if(strcmp(system->files[branch][j].file_name, resolutions[i].file_name) == 0){
                file_index = j;
            }
        }

        // If file was not found in the branch
        if(file_index == -1){
            // We need to make space for this file

            // Add to list of tracked files
            if(system->num_files[branch] == system->cap_files[branch]){
                // We need to expand the capacity of the files array
                // We realloc double the space because its generally efficient
                system->files[branch] = (struct File*)realloc(system->files[branch], sizeof(struct File)*system->cap_files[branch]*2);
                system->cap_files[branch] = system->cap_files[branch]*2;
            }

            // Now that the files array have enough space
            // Initialise the struct we are going to use
            struct File* new_file = &system->files[branch][system->num_files[branch]];
            // new_file->hash = (size_t)hash_file(system, resolutions[i].resolved_file);
            new_file->file_name = strdup(resolutions[i].file_name);
            file_index = system->num_files[branch];
            system->num_files[branch]++;

        }


        // Hash, index, and check length of the resolution file
        system->files[branch][file_index].hash = (size_t)hash_file(system, resolutions[i].resolved_file);
        system->files[branch][file_index].fc_index = store_content(system, file_ptr);
        system->files[branch][file_index].fc_length = num_bytes(file_ptr);


        fclose(file_ptr);

        // Print out the content into file_name
        FILE* file = fopen(resolutions[i].file_name, "w");

        char* file_content = system->file_contents[system->files[branch][file_index].fc_index];

        int num_elm = system->files[branch][file_index].fc_length;

        fwrite(file_content, 1, num_elm, file);

        fflush(file);

        fclose(file);

    }


}


// Return -1 if no corresponding file was found
// Return index of file in main branch if the file was found
int check_branch_for_file(struct System* system, size_t branch, char* file_name){

    int file_index = -1;

    for(int i = 0; i < system->num_files[branch]; i++){

        if(strcmp(system->files[branch][i].file_name, file_name) == 0){

            file_index = i;
            break;

        }


    }

    return file_index;

}


#ifndef SVC_STRUCTS
#define SVC_STRUCTS

#include <stdlib.h>
#include "svc.h"
#include <stdio.h>

struct Commit{

    char* message;
    char* id;
    struct File* files;
    size_t num_files;

    struct Commit* parent_commit; // Pointer to parent commit
    struct Commit* parent_commit2;
    size_t num_parents;
    struct Commit* child_commits; // An array of commits
    size_t num_childs;
    bool child_allocated;

    struct Changes* changes;
    size_t num_changes;

    char* branch_name;
    size_t branch_id;


};

struct System{


    // Commits
    struct Commit* initial_commit;
    struct Commit* head_commit; // Currently active commit

    
    // Files
    struct File** files;
    size_t* num_files;
    size_t* cap_files;

    // File content control
    char** file_contents;
    size_t num_content;
    size_t cap_content;


    // Branches
    char** branches;
    size_t num_branches;
    // Points to where each branch's is currently active
    struct Commit** branch_ptrs;
    size_t active_branch_id;


};

struct File {

    size_t hash;
    char* file_name;
    // Index of file content in the array file_content
    int fc_index; 
    // MARK: Might need to store the actual file content as well
    int fc_length;


};


struct Changes {

    char* file_name;
    bool addition;
    bool deletion;
    bool modification;
    int prev_hash;
    int new_hash;
    

};




#endif
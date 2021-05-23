#ifndef structs
#define structs

char* get_commit_id(struct commit* commit);

struct changes{

    char* file_name;
    int addition;
    int deletion;
    int modification;

};

struct commit{

    char* message;
    size_t id;
    // Also is suppose to store all the files being tracked?
    struct changes* changes;
    size_t num_changes;

};

struct helper{

    struct commit* commits;
    size_t num_commits;
    size_t cap_commits;


};

#endif
#include <assert.h>
#include "svc.h"
#include <stdio.h>
// #include "svc.c"
// #include "structs.h"


void test_hash(void* helper){

    int result = hash_file(helper, "hello.py");
    printf("Result = %d\n", result);

}


int main(int argc, char **argv) {
    void *helper = svc_init();
    
    // TODO: write your own tests here
    // Hint: you can use assert(EXPRESSION) if you want
    // e.g.  assert((2 + 3) == 5);
    test_hash(helper);
    
    // cleanup(helper);
    
    return 0;
}


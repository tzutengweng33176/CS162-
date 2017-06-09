#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

int main() {
    struct rlimit lim;
    getrlimit(RLIMIT_STACK, &lim);

    long int stacksize=lim.rlim_cur;
    getrlimit(RLIMIT_NPROC, &lim);

    long int processlimit=lim.rlim_cur;
    
    getrlimit(RLIMIT_NOFILE, &lim);
    long int maxfiledescriptor=lim.rlim_cur;


    printf("stack size: %ld\n", stacksize);
    printf("process limit: %ld\n", processlimit);
    printf("max file descriptors: %ld\n", maxfiledescriptor);


    return 0;
}

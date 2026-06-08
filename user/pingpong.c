#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);
    int identifier = fork();
    int pid = getpid();
    if (identifier == 0){
        // child
        char byte;
        read(p1[0], &byte, 1);
        printf("%d: received ping\n", pid);
        write(p2[1], &byte, 1);
        close(p2[1]);
    }
    else {
        //parent
        write(p1[1], "A", 1);
        close(p1[1]);
        char byte;
        read(p2[0], &byte, 1);
        printf("%d: received pong\n", pid);
    }
    exit(0);
}
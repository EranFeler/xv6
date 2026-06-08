#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{
    if(argc > 1){
        fprintf(2, "usage: primes\n");
        exit(1);
    }
    int previous[2];
    int next[2];
    pipe(next);
    for(int i = 2; i <= 35; i++){
        write(next[1], &i, 4);
    }
    close(next[1]);
    if(fork() != 0){
            wait(0);
        }
    int done = 0;
    int first;
    while (!done){
        int num;
        done = 1;
        first = 0;
        previous[0] = next[0];
        pipe(next);
        while (read(previous[0], &num, sizeof(num)) > 0) {
            done = 0;
            if(!first){
                first = num;
            }
            else if(num % first != 0){
                write(next[1], &num, 4);
            }
        }
        if (first == 0) {
            break;
        }
        printf("prime %d\n", first);
        close(previous[0]);
        close(next[1]);
        if (!done){
            if(fork() != 0){
                wait(0);
            }
        }
    }
    exit(0);
}
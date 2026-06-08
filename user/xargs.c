#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#define MAXLINE 512

int
main(int argc, char *argv[])
{
    char line[MAXLINE];
    char *cmd_argv[MAXARG];
    int position;
    char c;
    if(argc < 2){
        fprintf(2, "usage: xargs command [args]\n");
        exit(1);
    }
    for(int i = 1; i < argc; i++){
        cmd_argv[i - 1] = argv[i];
    }
    position = 0;
    while(read(0, &c, 1) > 0){
        if(c == '\n'){
            line[position] = '\0';
            if(position > 0){
                cmd_argv[argc - 1] = line;
                cmd_argv[argc] = 0;
                if(fork() == 0){
                    exec(cmd_argv[0], cmd_argv);
                }
            wait(0);
            }
        position = 0;
        } else {
            if(position < MAXLINE - 1){
                line[position++] = c;
            }
        }
    }
    if(position > 0){
        line[position] = '\0';
        cmd_argv[argc - 1] = line;
        cmd_argv[argc] = 0;
        if(fork() == 0){
            exec(cmd_argv[0], cmd_argv);
        }   
        wait(0);
    }
    exit(0);
}
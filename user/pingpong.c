#include "kernel/types.h"
#include "user.h"
#include <stddef.h>
#define MSGSIZE 5

char *ping = "ping";
char *pong = "pong";

int main(int argc, char* argv[])
{
    char inbuf[MSGSIZE];
    int p_read[2], s_read[2];
    /*
        p_read[0]:父进程读
        p_read[1]:子进程写
        s_read[0]:子进程读
        s_read[1]:父进程写

    */
    int pid ;
    if(pipe(p_read) < 0 || pipe(s_read) < 0)//create a pipe
        exit(1);
    if((pid = fork()) > 0){
        //parent
        close(s_read[0]);
        write(s_read[1], ping, MSGSIZE);
        close(s_read[1]);
        wait(NULL);

        close(p_read[1]);
        read(p_read[0], inbuf, MSGSIZE);
        printf("%d: received %s\n",getpid(),inbuf);
        close(p_read[0]);
    } 
    else {
    //subprocess
        close(s_read[1]);
        read(s_read[0], inbuf, MSGSIZE);
        printf("%d: received %s\n",getpid(), inbuf);
        close(s_read[0]);
        
        close(p_read[0]);
        write(p_read[1], pong, MSGSIZE);
        close(p_read[1]);
        exit(0);
    }
    exit(0);
}
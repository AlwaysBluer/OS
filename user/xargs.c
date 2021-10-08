#include "kernel/types.h"
#include "user.h"
#include <stddef.h>

#define BUF_SIZE 32
#define PARA_LEN 32
#define PARA_NUM_MAX 16

int main(int argc ,char *argv[]){
    /*program can read parament from standard input*/
    // printf("argc is %d\n",argc);
    int pd;
    // int tmp_cnt;
    int read_len;
    char buf[BUF_SIZE];
    char *ptr[PARA_NUM_MAX] = {NULL};
    static char parament[PARA_NUM_MAX][PARA_LEN]; //char point of the string parament
    int i = 0,j = 0 ,k = 0; //i means the index of parament, j is iterator of parament[i], k is iterator of buf
    if(argc > PARA_NUM_MAX){
        printf("parament too much\n");
        exit(0);
    }
    for(i = 0; i < argc; i++)
    {
        if((strlen(argv[i])) + 1 > PARA_LEN)
        {
            printf("parament too long\n");
            exit(0);
        }
        memset(parament[i], '\0', sizeof(parament[i]));
        strcpy(parament[i], argv[i]);
        ptr[i] = parament[i];
    }
    ptr[i] = parament[i];
    memset(parament[i], '\0', sizeof(parament[i]));
    while((read_len = read(0, buf, sizeof buf )) > 0){
        for(k = 0; k < read_len; k++)
        {
            if(buf[k] == '\n'){
                // if(strlen(parament[i]) == 0){
                //     // input '\n'
                //     memset(parament[i], 0, sizeof(parament[i]));
                //     if((pd = fork()) == 0)
                //         exec(argv[1], ptr + 1);
                //     else
                //         wait(NULL);
                // }
                // printf("i:%d,par[%d]:%s\n",i, argc, parament[argc]);
                parament[i][j] = '\0';
                i++;
                j = 0;
                memset(parament[i], '\0', sizeof(parament[i]));
                ptr[i] = parament[i];
                parament[i][j] = 0; //the last parament must be 0 #requirement
                if((pd = fork()) < 0){
                    printf("error in forking\n");
                    exit(0);
                }
                else if(pd == 0){
                    //subprocess
                    exec(argv[1], ptr + 1);//parament[0] is current_path
                }
                else{
                    //parent
                    wait(NULL);
                }
                i = argc;
                j = 0;
            }
            else if(buf[k] == ' '){
                parament[i][j] = '\0';//end of this parament
                i++;
                memset(parament[i], '\0', sizeof(parament[i]));
                ptr[i] = parament[i];
                j = 0;
            }
            else{
                if(j == PARA_LEN ){
                    printf("overlip!\n");
                    exit(0);
                }
                parament[i][j] = buf[k];
                // printf("parament[%d][%d]:[%c]\n",i, j, buf[k]);
                j++;
            }
        }
    }
    return 0;
}
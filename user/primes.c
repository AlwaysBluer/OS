#include "kernel/types.h"
#include "user.h"
#include <stddef.h>
#define MAXSIZE 100


void filter(int a[],  int* left_amount); //filter some number which is not prime;according to a[0]

int main()
{
    int left_amount = 34;
    int amount_fd[2], array_fd[2];
    int pid;
    int after_filter[MAXSIZE];
    int cnt;
    for(cnt = 0; cnt < left_amount; cnt++){
        //initialize
        after_filter[cnt] = cnt + 2;
    }
    pipe(amount_fd);
    pipe(array_fd);
    pid = fork();
    while(left_amount > 0 ){
        if(pid > 0){
            //parent
            filter(after_filter, &left_amount);
            close(amount_fd[0]);
            write(amount_fd[1], &left_amount, sizeof(left_amount));
            close(amount_fd[1]);

            close(array_fd[0]);
            write(array_fd[1], after_filter, left_amount*sizeof(int));
            close(array_fd[1]);

            wait(NULL);
            exit(0);
        }
        else if(pid == 0){
            //subprocess
            close(amount_fd[1]);
            read(amount_fd[0], &left_amount, sizeof(left_amount));
            close(amount_fd[0]);

            close(array_fd[1]);
            read(array_fd[0], after_filter, left_amount*sizeof(int));
            close(array_fd[0]);
            //之前一直有bug：原来是一直在fork()之后才创建pipe,导致父子进程无法通信。pipe的创建必须在fork之前才能保证通信正常。
            if(left_amount > 0){
                pipe(amount_fd);
                pipe(array_fd);
                pid = fork();
            }
            else if(left_amount == 0){
                exit(0);
            }
            else{
                printf("left_amount less than 0\n");
                exit(1);
            }
        }
        else{
            exit(1);
        }
    }
    exit(0);
}


void filter(int a[],  int* left_amount){
   const int lenth = *left_amount;
   const int first = a[0];
   int cnt, i;
   printf("prime %d\n",first);
   if(lenth <= 1){//一定考虑边界情况
        *left_amount = 0;
        return;
   }
   for(cnt = 1, i = 0; cnt < lenth; cnt++){
       if(a[cnt] % first != 0){
           a[i] = a[cnt];
           i++;
       }
   }
   *left_amount = i;
}
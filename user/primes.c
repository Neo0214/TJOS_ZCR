#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

void checkNext(int* fd)
{
    // 已经在子进程中
    close(fd[1]); // 关闭写端，开启读端
    int prime=0;
    read(fd[0],&prime,sizeof(int));
    if (prime==0)
    {
        // 该结束了
        close(fd[0]); // 关闭读端，结束
        exit(0);
    }
    else
    {
        // 继续迭代
        printf("prime %d\n",prime);
        // 检查剩余的数字
        int numbers[35] = {0};
        int cur=0;
        int number=0;
        while (read(fd[0],&number,sizeof(int))>0)
        {
            if (number%prime!=0)
            {
                numbers[cur++] = number;
            }
        }
        int newfd[2];
        pipe(newfd);
        int ret = fork();
        if (ret==0)
        {
            // 子进程
            checkNext(newfd);
        }
        else if (ret>0)
        {
            // 父进程
            close(newfd[0]); // 关闭读端,准备写
            write(newfd[1],numbers,sizeof(int)*cur);
            close(newfd[1]); // 关闭写端
            wait(0); // 等待子进程结束，才能结束父进程
            exit(0);
        }
        else
        {
            printf("fork error\n");
        }
    }
}



int main(int argc, char* argv[])
{
    int fd[2]; // 0 读 1 写
    int numbers[35] = {0};
    pipe(fd);
    int ret = fork();
    if (ret==0)
    {
        // 子进程
        checkNext(fd);
    }
    else if (ret>0)
    {
        // 父进程
        close(fd[0]); // 关闭读端,准备写
        for (int i = 2; i <= 35; i++)
        {
            numbers[i-2] = i;
        }
        write(fd[1],numbers,sizeof(int)*35);
        close(fd[1]); // 关闭写端
        wait(0); // 等待子进程结束，才能结束父进程
    }
    else
    {
        printf("fork error\n");
    }





    exit(0);
}
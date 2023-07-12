#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc,char* argv[])
{
    if (argc!=1)
    {
        fprintf(2,"legal format as below:\n  pingpong\n");
        exit(1);
    }
    int parentToChild[2];
    int childToParent[2]; // 往1写，从0读
    pipe(parentToChild);
    pipe(childToParent);
    char content[4];
    int pid=fork();
    if (pid==0)
    {
        int childPID=getpid();
        close(parentToChild[1]); // 关闭写端
        close(childToParent[0]); // 关闭读端

        read(parentToChild[0],content,sizeof(content)); // 读取父进程写入的内容
        printf("%d: received ping\n",childPID);
        write(childToParent[1],"pong",sizeof(content)); // 写入内容
        
        exit(0);
    }
    else if (pid>0)
    {
        int parentPID=getpid();
        close(childToParent[1]); // 关闭写端
        close(parentToChild[0]); // 关闭读端
        write(parentToChild[1],"ping",sizeof(content)); // 写入内容
        read(childToParent[0],content,sizeof(content)); // 读取子进程写入的内容
        printf("%d: received pong\n",parentPID);
        exit(0);
    }
    else
    {
        fprintf(2,"system wrong, please try again");
        exit(1);
    }

}
#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define MAXLENGTH 1024

int main(int argc, char *argv[]) {
    char* buf[MAXARG]; // 用于存储参数
    char contentIn[MAXLENGTH]; // 用于存储输入的命令
    int cur = 0; // 记录当前读到的位置
    int curArgPos=0; // 记录当前参数的位置
    int writePos=0;
    for (int i=1;i<argc;i++)
        buf[curArgPos++] = argv[i];
    // 此时curArgPos指向下一个参数的位置
    int length=0;
    while ((length=read(0, contentIn+writePos, MAXLENGTH))>0) 
    {
        if (writePos+length>MAXLENGTH)
        {
            printf("xargs: input too long, it should be shorter than 1024 bytes\n");
            exit(1);
        }  
        // 正常读取后
        int top=writePos+length;
        for (int i=writePos;i<top;i++)
        {
            // 逐字符
            writePos++;
            if (contentIn[i]==' ')
            {
                contentIn[i]='\0';
                buf[curArgPos++] = &contentIn[cur];
                cur = i+1;
            }
            else if (contentIn[i]=='\n')
            {
                contentIn[i]='\0';
                buf[curArgPos++] = &contentIn[cur];
                buf[curArgPos] = '\0';
                // 此时buf中存储了所有的参数
                // 执行命令
                int pid = fork();
                if (pid==0)
                {
                    // 子进程
                    exec(argv[1], buf);
                    exit(0);
                }
                else
                {
                    // 父进程
                    curArgPos=argc-1;
                    for (int i=0;i<length;i++)
                        contentIn[i] = '\0';
                    cur=0;
                    writePos=0;
                    wait(0);
                }
            }
        }

        

    }
    if (length<0)
    {
        printf("xargs: read error\n");
        exit(1);
    }



  exit(0);
}


#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path) {
    static char buf[DIRSIZ+1];
    char *p;
    for(p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;  
    if(strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, sizeof(p));
    p[strlen(buf)] = '\0';
    return buf;
}

void compare(char *path, char *name){
    if(strcmp(fmtname(path), name) == 0)
        printf("%s\n", path);
    return;
}

void find(char* path, char* name)
{
    // 尝试 ls as ls.c
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0)
    {
        fprintf(2, "ls: cannot open %s\n", path);
        exit(1);
    }

    if(fstat(fd, &st) < 0)
    {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        exit(1);
    }

    switch(st.type)
    {
        case T_FILE:
            compare(path, name);
            break;

        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
            {
                printf("ls: path too long\n");
                break;
            }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(buf, &st) < 0)
            {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            find(buf, name);
        }
        break;
    }
    close(fd);

}

int main(int argc,char* argv[])
{
    if (argc != 3)
    {
        printf("legal format as below\n  : find <path> <name>\n");
        exit(1);
    }
    
    // 一般情况
    char* path = argv[1];
    char* name = argv[2];
    find(path, name);
    







    exit(0);
}
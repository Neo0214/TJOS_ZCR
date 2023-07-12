#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) 
{
  if (argc != 2) 
  {
    fprintf(2, "legal format as below:\n  sleep [ticks num]\n");
    exit(1);
  }
  
  int ticks = atoi(argv[1]);
  int ret = sleep(ticks);
  exit(ret);
}
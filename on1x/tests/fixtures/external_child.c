/* Standalone, shell-free child used to prove SIGTERM-ignoring cancellation. */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc,char **argv){
 struct timespec step={0,2000000};int i,fd;
 if(argc!=3)return 90;
 if(strcmp(argv[1],"ignore")==0){
  if(signal(SIGTERM,SIG_IGN)==SIG_ERR)return 91;
  fd=open(argv[2],O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd<0)return 92;
  char id[32];int count=snprintf(id,sizeof(id),"%ld\n",(long)getpid());
  if(count<0||count>=(int)sizeof(id)||write(fd,id,(size_t)count)!=count)return 95;
  (void)close(fd);
  for(;;)pause();
 }
 if(strcmp(argv[1],"wait")==0){
  struct stat info;
  for(i=0;i<2500;++i){
   if(stat(argv[2],&info)==0)return 0;
   (void)nanosleep(&step,NULL);
  }
  return 93;
 }
 return 94;
}

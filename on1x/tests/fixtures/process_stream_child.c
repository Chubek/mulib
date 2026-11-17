/* Strict-C11 child for captured stdin/stdout/stderr and SIGPIPE tests. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int write_all(int fd,const uint8_t *data,size_t size){
 size_t done=0;
 while(done<size){
  ssize_t count=write(fd,data+done,size-done);
  if(count<0&&errno==EINTR)continue;
  if(count<=0)return -1;
  done+=(size_t)count;
 }
 return 0;
}

int main(int argc,char **argv){
 uint8_t data[97];
 if(argc==6&&strcmp(argv[1],"stream")==0){
  static const uint8_t error_text[]={'E','R','R'};
  ssize_t count;
  if(strcmp(argv[2],"two words")||strcmp(argv[3],"four")||strcmp(argv[4],"escaped word")||strcmp(argv[5],"#literal"))return 96;
  for(;;){
   count=read(STDIN_FILENO,data,sizeof(data));
   if(count<0&&errno==EINTR)continue;
   if(count<0)return 97;
   if(!count)break;
   if(write_all(STDOUT_FILENO,data,(size_t)count))return 98;
  }
  if(write_all(STDERR_FILENO,error_text,sizeof(error_text)))return 99;
  return 0;
 }
 if(argc==2&&strcmp(argv[1],"noinput")==0){
  static const uint8_t marker='X';
  struct timespec delay={0,50000000};
  if(close(STDIN_FILENO))return 100;
  while(nanosleep(&delay,&delay)&&errno==EINTR){}
  return write_all(STDOUT_FILENO,&marker,1)?101:0;
 }
 return 95;
}

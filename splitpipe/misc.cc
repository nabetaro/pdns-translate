#include "misc.hh"
#include <stdexcept>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <cerrno>

using namespace std;

void unixDie(const string& during)
{
  throw runtime_error("during "+string(during)+": "+strerror(errno));
}

void waitForUser()
{	
  FILE *fp=fopen("/dev/tty", "r");
  if(!fp) 
    unixDie("opening of /dev/tty for user input");
  fflush(fp);
  char line[80];
  fgets(line, sizeof(line) - 1, fp);
  fclose(fp);
}


string makeHexDump(const string& str)
{
  char tmp[5];
  string ret;
  for(string::size_type n=0;n<str.size();++n) {
    if(n && !(n%8))
      ret+=" ";
    sprintf(tmp,"%02x", (unsigned char)str[n]);
    ret+=tmp;
  }
  return ret;
}


string generateUUID()
{
  int fd=open("/dev/urandom",O_RDONLY);
  if(fd < 0)
    unixDie("opening the random device");
  char buffer[16];
  if(read(fd, buffer, 16)!=16)
    throw runtime_error("partial read from random device");
  
  return string(buffer, buffer+16);
}


void setNonBlocking(int fd)
{
  int flags=fcntl(fd,F_GETFL,0);
  if(flags<0 || fcntl(fd, F_SETFL,flags|O_NONBLOCK) < 0 && errno!=ENOTTY)
    unixDie("setting filedescriptor to nonblocking failed");
}

void setBlocking(int fd)
{
  int flags=fcntl(fd,F_GETFL,0);
  if(flags<0 || fcntl(fd, F_SETFL,flags & (~O_NONBLOCK) ) <0 && errno!=ENOTTY)
    unixDie("setting filedescriptor to blocking failed");
}


double getTime()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec/1000000.0;
}

int readn(int fd, void* ptr, size_t size, const char* description)
{
  size_t done=0;
  int ret;
  while(size!=done) {
    ret=read(fd,(char*)ptr+done,size-done);
    if(ret==0)
      return 0;
    if(ret<0)
      unixDie(description);
    done+=ret;
  }
  return size;
}

int writen(int fd, const void* ptr, size_t size, const char* description)
{
  setBlocking(fd);

  size_t done=0;
  int ret;
  while(size!=done) {
    ret=write(fd,(const char*)ptr+done,size-done);
    if(ret==0) {
      setNonBlocking(fd);
      return 0;
    }
    if(ret<0)
      unixDie(description);
    done+=ret;
  }
  setNonBlocking(fd);
  return size;
}

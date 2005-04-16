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


void setNonBlocking(int fd)
{
  int flags=fcntl(fd,F_GETFL,0);
  if(flags<0 || fcntl(fd, F_SETFL,flags|O_NONBLOCK) <0)
    unixDie("Setting filedescriptor to nonblocking failed");
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
  size_t done=0;
  int ret;
  while(size!=done) {
    ret=write(fd,(const char*)ptr+done,size-done);
    if(ret==0)
      return 0;
    if(ret<0)
      unixDie(description);
    done+=ret;
  }
  return size;
}

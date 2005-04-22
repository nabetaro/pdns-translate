#ifndef MISC_HH
#define MISC_HH
#include <string>
#include <stdint.h>

int readn(int fd, void* ptr, size_t size, const char* description);
int writen(int fd, const void* ptr, size_t size, const char* description);
double getTime();
void setNonBlocking(int fd);
void setBlocking(int fd);
void unixDie(const std::string& during);
std::string generateUUID();
std::string makeHexDump(const std::string& str);
void waitForUser();
struct stretchHeader
{
  uint16_t size;
  uint8_t type;

  enum Types { SessionName=0, SessionUUID=1, VolumeNumber=2, VolumeEOF=3, 
	       Data=4, MD5Checksum=5, SHA1Checksum=6, SessionEOF=7, VolumeDate=8 };
} __attribute__((packed));

int SplitpipeMain(int argc, char** argv);
int JoinpipeMain(int argc, char** argv);

#endif

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

  enum Types { SessionName=0, SessionUUID, VolumeNumber, VolumeEOF, Data, MD5Checksum, SHA1Checksum, SessionEOF };
} __attribute__((packed));

#endif

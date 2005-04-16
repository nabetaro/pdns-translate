#ifndef MISC_HH
#define MISC_HH
#include <string>

int readn(int fd, void* ptr, size_t size, const char* description);
int writen(int fd, const void* ptr, size_t size, const char* description);
double getTime();
void setNonBlocking(int fd);
void unixDie(const std::string& during);

#endif

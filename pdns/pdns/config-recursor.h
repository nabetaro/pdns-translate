#define SYSCONFDIR "/etc/powerdns/" 
#define LOCALSTATEDIR "/var/run/" 
#define VERSION "3.2-testing"
#define RECURSOR
#ifndef WIN32

#if __GNUC__ == 4 &&  __GNUC_MINOR__ < 2
#define GCC_SKIP_LOCKING
#endif


#endif

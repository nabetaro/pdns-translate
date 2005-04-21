#ifndef SPLITPIPE_DISPLAY_HH
#define SPLITPIPE_DISPLAY_HH
#include <ncurses.h>
#include <stdint.h>

class SplitpipeDisplay
{
public:
  SplitpipeDisplay();
  void setBarPercentage(int percentage);
  ~SplitpipeDisplay();
  void programPut(char c);
  void programCommit();
  void log(const char* fmt, ...);
  void setTotalBytes(uint64_t input, uint64_t output, uint32_t bufferSize, int volPerc);
  void setLogStandout(bool bold);
  void refresh();
private:
  WINDOW* d_pwin, *d_sepawin, *d_logwin;
  int getHeight();
  int getWidth();
  bool d_firstlog;
};

#endif

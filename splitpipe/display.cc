#include "display.hh"
#include <ncurses.h>
#include <string>
#include <stdexcept>

using namespace std;

SplitpipeDisplay::SplitpipeDisplay()
{
  initscr();      /* initialize the curses library */
  
  int logSize=10;
  int statusSize=5;
  
  
  d_pwin=newwin(getHeight()-logSize-statusSize, 0, 0, 0);
  d_sepawin=newwin(statusSize, 0, getHeight()-statusSize-logSize, 0);
  d_logwin=newwin(logSize, 0, getHeight()-logSize, 0);
  
  scrollok(d_logwin, true);
  scrollok(d_pwin, true);
  if(!d_pwin || !d_logwin) {
    throw runtime_error("Unable to initialize ncurses");
  }
  
  //  keypad(swin, TRUE);  /* enable keyboard mapping */
  nonl();         /* tell curses not to do NL->CR/NL on output */
  //cbreak();       /* take input chars one at a time, no wait for \n */
  noecho();
  
  wmove(d_sepawin,0,0);
  
  for(int x=0;x<getWidth();++x)
    waddch(d_sepawin,ACS_HLINE);
  
  wmove(d_sepawin,2,0);
  for(int x=0;x<getWidth();++x)
    waddch(d_sepawin,ACS_HLINE);
  wmove(d_sepawin,4,0);
  for(int x=0;x<getWidth();++x)
    waddch(d_sepawin,ACS_HLINE);
  
  //  mvwprintw(d_sepawin,3,0,"Volume done ETA: --:--");
  
  wrefresh(d_sepawin);
  wrefresh(d_logwin);
  wrefresh(d_pwin);

  d_firstlog=true;
}

void SplitpipeDisplay::setTotalBytes(uint64_t input, uint64_t output, uint32_t bufferSize)
{
  mvwprintw(d_sepawin,3,0,"Input: %5dMB     Output: %5dMB     Buffer: %5dKB", 
	    (int)(input/1000000), (int)(output/1000000), bufferSize/1024);
  wrefresh(d_sepawin);
  wrefresh(d_logwin);
}

void SplitpipeDisplay::setBarPercentage(int percentage)
{
  int width=getWidth()-14;
  int fillTo=width*percentage/100.0;
  mvwprintw(d_sepawin,1,0,"Buffer: %3d%%", percentage);

  wmove(d_sepawin,1,14);
  for(int x=0 ; x < width;++x)
    if(x<fillTo)
      waddch(d_sepawin,'X');
    else
      waddch(d_sepawin,' ');
  wrefresh(d_sepawin);
  wrefresh(d_logwin);
}

SplitpipeDisplay::~SplitpipeDisplay()
{
  endwin();
}
  
void SplitpipeDisplay::programPut(char c)
{
  waddch(d_pwin, c);

}

void SplitpipeDisplay::programCommit()
{
  wrefresh(d_pwin);
  wrefresh(d_logwin);
}

void SplitpipeDisplay::log(const char* fmt, ...)
{
  char buffer[40];
  struct tm tm;
  time_t t;
  time(&t);
  tm=*localtime(&t);

  strftime(buffer,sizeof(buffer),"%H:%M:%S ", &tm);

  string total;
  if(!d_firstlog) {
    total="\n";
  }
  d_firstlog=false;
  total+=buffer;
  total+=fmt;

  va_list ap;
    
  /* Try to print in the allocated space. */
  va_start(ap, fmt);
  vw_printw(d_logwin, total.c_str(), ap);
  va_end(ap);
  wrefresh(d_logwin);
}

int SplitpipeDisplay::getHeight()
{
  int y,x;
  getmaxyx(stdscr, y,x);
  return y;
}
  

int SplitpipeDisplay::getWidth()
{
  int y,x;
  getmaxyx(stdscr, y,x);
  return x;
}
/*
int main(int argc, char** argv)
{
  SplitpipeDisplay spd;
  spd.log("Gaat dit allemaal '%s'\n", "goed");
  spd.programPut('h');
  spd.setBarPercentage(88);
  sleep(2);
}
*/

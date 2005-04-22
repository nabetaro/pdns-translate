/*
    Copyright (C) 2005  Netherlabs Computer Consulting BV

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as published by
    the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "display.hh"
#include <ncurses.h>
#include <string>
#include <stdexcept>

using namespace std;

SplitpipeDisplay::SplitpipeDisplay()
{
  char *term=getenv("TERM");
  if(term && !strcmp(term, "xterm")) 
    putenv("TERM=vt102");  // make sure ncurses does not clear screen on exit

  /* (See above) 
22:09 <zwane> hehehe
22:09 <zwane> *yuck*
22:10 <zwane> good god
22:10 <ahu> there are rumours there is a beter solution
22:10 <zwane> you should be flogged for that offence
22:10 <ahu> at least I admit my weaknesses
22:10 <zwane> ahu: if i did, i'd be flogged publically daily
22:10 <sarnold> ahu: eww :)
22:10 <zwane> so i keep my hacks _under_wraps_ ;)
  */

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

void SplitpipeDisplay::setTotalBytes(uint64_t input, uint64_t output, uint32_t bufferSize, int volPerc)
{
  mvwprintw(d_sepawin,3,0,"Input: %5dMB     Output: %5dMB     Buffered: %5dKB     Volume done: %3d%%", 
	    (int)(input/1000000), (int)(output/1000000), bufferSize/1024, volPerc);
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

void SplitpipeDisplay::refresh()
{
  redrawwin(d_pwin);
  redrawwin(d_sepawin);
  redrawwin(d_logwin);
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

void SplitpipeDisplay::setLogStandout(bool bold)
{
  if(bold)
    wattron(d_logwin, A_BOLD);
  else
    wattroff(d_logwin, A_BOLD);
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

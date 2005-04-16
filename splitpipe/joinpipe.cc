/*
    joinpipe takes chunks output by splitpipe and joins them to one pipe
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

#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdint.h>
#include <signal.h>
#include <iostream>
#include <netinet/in.h>
#include "misc.hh"

using namespace std;

struct params
{
  params()
  {
    verbose=debug=verify=false;
  }
  
  bool verbose;
  bool debug;
  bool verify;

}parameters;

void usage()
{
  cerr<<"joinpipe syntax:\n";
  cerr<<" --debug, -d\t\tGive debugging output\n";
  cerr<<" --help, -h\t\tGive this helpful message\n";
  cerr<<" --verbose, -v\t\tGive verbose output\n";
  cerr<<" --verify, -t\t\tOnly verify an archive\n\n";
  
  exit(1);
}

void ParseCommandline(int argc, char** argv)
{
  int c;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"debug", 0, 0, 'd'},
      {"help", 0, 0, 'h'},
      {"verbose", 0, 0, 'v'},
      {"verify", 0, 0, 't'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long (argc, argv, "dhv",
		     long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
    case 'd':
      parameters.debug=1;
      break;
    case 'h':
      usage();
      break;
    case 't':
      parameters.verify=1;
    case 'v':
      parameters.verbose=true;
      break;
    }
  }
  /*
  if (optind < argc) {
    while (optind < argc) {
      parameters.outputCommand.push_back(argv[optind++]);
    }
  }
  */
}



int main(int argc, char** argv)
try
{
  ParseCommandline(argc, argv);


  uint16_t stretchLeft;

  char* buffer=new char[65536];
  for(;;) {
    if(!readn(0, &stretchLeft, 2, "read of length of stretch"))
      break;
    
    stretchLeft=ntohs(stretchLeft);

    if(!stretchLeft)
      break;

    readn(0, buffer, stretchLeft, "read of a stretch of input");
    writen(1, buffer, stretchLeft, "write of a stretch of input");
  }

}
catch(exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

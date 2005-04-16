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
#include <stdexcept>
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
  string inputDevice;
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
  if (optind < argc) {
    while (optind < argc) {
      parameters.inputDevice=argv[optind++];
    }
  }

}

int main(int argc, char** argv)
try
{
  ParseCommandline(argc, argv);

  char* buffer=new char[65536];
  struct stretchHeader stretch;
  string uuid;
  unsigned int numChunks=0;

  int infd=open(parameters.inputDevice.c_str(), O_RDONLY);
  if(infd < 0)
    unixDie("opening of "+parameters.inputDevice+" for input");

  for(;;) {
    if(!readn(infd, &stretch, sizeof(struct stretchHeader), "read of stretch header"))
      throw runtime_error("EOF before end of session");

    stretch.size=ntohs(stretch.size);

    if(stretch.size && !readn(infd, buffer, stretch.size, "read of a stretch of input")) {
      cerr<<stretch.size<<endl;
      throw runtime_error("unexpected end of file during read of a stretch");
    }

    if(stretch.type==stretchHeader::SessionUUID) {    
      if(uuid.empty()) {
	uuid=string(buffer,buffer+stretch.size);
	cerr<<"UUID of this session is '"<<makeHexDump(uuid)<<"'"<<endl;
      } else {
	if(uuid != string(buffer,buffer+stretch.size)) {
	  cerr<<"This chunk does not belong to the correct session, ";
	  cerr<<"uuid should be '"<<makeHexDump(uuid)<<"', is '"<<makeHexDump(string(buffer,buffer+stretch.size))<<"'"<<endl;
	  exit(EXIT_FAILURE); // deal with this
	}
      }
    }
    else if(stretch.type==stretchHeader::ChunkNumber) {    
      uint16_t newChunkNumber;
      memcpy(&newChunkNumber, buffer, 2);
      newChunkNumber = ntohs(newChunkNumber);
      if(newChunkNumber != numChunks) {
	cerr<<"This is chunk number "<<newChunkNumber<<", were expecting "<<numChunks<<", please retry"<<endl;
	exit(EXIT_FAILURE);
      }
      else if(parameters.verbose)
	cerr<<"Found chunk "<<newChunkNumber<<", as expected"<<endl;
      numChunks++;
    }
    else if(stretch.type==stretchHeader::Data) {    
      if(!writen(1, buffer, stretch.size, "write of a stretch of input"))
	throw runtime_error("unexpected end of file on standard output");
    }
    else if(stretch.type==stretchHeader::ChunkEOF) {    

      close(infd);
      cerr<<"joinpipe: end of chunk, change media and press enter"<<endl;

      waitForUser();

      infd=open(parameters.inputDevice.c_str(), O_RDONLY);
      if(infd < 0)
	unixDie("opening of "+parameters.inputDevice+" for input");
    }
    else if(stretch.type==stretchHeader::SessionEOF) {    
      cerr<<"joinpipe: end of session"<<endl;
      break;
    }
    else {
      cerr<<"joinpipe: unknown stretch type "<<(int)stretch.type<<endl;
    }
  }
}
catch(exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

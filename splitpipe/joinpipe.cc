/*
    joinpipe takes volumes output by splitpipe and joins them to one pipe
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
#include <vector>
#include "misc.hh"
#include "md5.hh"

using namespace std;

namespace {
  struct params
  {
    params()
    {
    verbose = debug = verify = noPrompt = false;
    }
    
    bool verbose;
    bool debug;
    bool verify;
    bool noPrompt;
    vector<string> inputDevice;
  }parameters;
}

void usage()
{
  cerr<<"joinpipe joins multiple volumes (volumes) into one pipe.\n";
  cerr<<"\nsyntax: joinpipe [options] device1 [device2] | ...\n\n";
  cerr<<" --debug, -d\t\tGive debugging output\n";
  cerr<<" --help, -h\t\tGive this helpful message\n";
  cerr<<" --no-prompt, -n\tRun without user intervention\n";
  cerr<<" --verbose, -v\t\tGive verbose output\n";
  cerr<<" --version\t\tReport version\n\n";
  
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
      {"no-prompt", 0, 0, 'n'},
      {"version", 0, 0, 'e'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long (argc, argv, "dhnv",
		     long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
    case 'd':
      parameters.debug=1;
      break;
    case 'e':
      cerr<<"joinpipe "VERSION" (C) 2005 Netherlabs Computer Consulting BV\nReport bugs to bert hubert <ahu@ds9a.nl>"<<endl;
      exit(EXIT_SUCCESS);
    case 'h':
      usage();
      break;

    case 'n':
      parameters.noPrompt=1;
      break;

    case 'v':
      parameters.verbose=true;
      break;
    case '?':
      usage();
      break;
    }
  }
  if (optind < argc) {
    while (optind < argc) {
      parameters.inputDevice.push_back(argv[optind++]);
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
  unsigned int numVolumes=0;

  if(parameters.inputDevice.empty())
    parameters.inputDevice.push_back("/dev/stdin");

  vector<string>::const_iterator inputIter=parameters.inputDevice.begin();

  int infd=open(inputIter->c_str(), O_RDONLY);
  if(infd < 0)
    unixDie("opening of "+*parameters.inputDevice.begin()+" for input");


  MD5Summer md5;


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
	  cerr<<"This volume does not belong to the correct session, ";
	  cerr<<"uuid should be '"<<makeHexDump(uuid)<<"', is '"<<makeHexDump(string(buffer,buffer+stretch.size))<<"'"<<endl;
	  exit(EXIT_FAILURE); // deal with this
	}
      }
    }
    else if(stretch.type==stretchHeader::VolumeNumber) {    
      uint16_t newVolumeNumber;
      memcpy(&newVolumeNumber, buffer, 2);
      newVolumeNumber = ntohs(newVolumeNumber);
      if(newVolumeNumber != numVolumes) {
	cerr<<"This is volume number "<<newVolumeNumber<<", were expecting "<<numVolumes<<", please retry"<<endl;
	exit(EXIT_FAILURE);
      }
      else if(parameters.verbose)
	cerr<<"Found volume "<<newVolumeNumber<<", as expected"<<endl;
      numVolumes++;
    }
    else if(stretch.type==stretchHeader::Data) {    
      if(!writen(1, buffer, stretch.size, "write of a stretch of input"))
	throw runtime_error("unexpected end of file on standard output");
      md5.feed(buffer, stretch.size);
    }
    else if(stretch.type==stretchHeader::VolumeEOF) {    
      close(infd);
      cerr<<"joinpipe: end of volume, change media and press enter"<<endl;

      if(!parameters.noPrompt)
	waitForUser();

      if(inputIter + 1 != parameters.inputDevice.end())
	inputIter++;

      infd=open(inputIter->c_str(), O_RDONLY);
      if(infd < 0)
	unixDie("opening of "+ *inputIter+" for input");
    }
    else if(stretch.type==stretchHeader::SessionEOF) {    
      cerr<<"joinpipe: end of session"<<endl;
      break;
    }
    else if(stretch.type==stretchHeader::MD5Checksum) {    
      string md5sum=md5.get();
      if(md5sum==string(buffer, buffer + stretch.size))
	cerr<<"joinpipe: running checksum correct"<<endl;
      else {
	cerr<<"joinpipe: checksum incorrect, actual '"<<makeHexDump(md5sum)<<"', should be '"<<makeHexDump(string(buffer,buffer+stretch.size))<<"'" <<endl;
	exit(EXIT_FAILURE);
      }
    }
    else {
      cerr<<"joinpipe: unknown stretch type "<<(int)stretch.type<<" of length "<< stretch.size <<endl;
      cerr<<makeHexDump(string(buffer, buffer+stretch.size))<<endl;
    }
  }
}
catch(exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

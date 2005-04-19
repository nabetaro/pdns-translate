/*
    splitpipe allows the output of programs to span volumes
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
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include "misc.hh"
#include "ringbuffer.hh"
#include "md5.hh"

namespace {
  struct paramStruct {
    paramStruct() 
    {
      bufferSize = 0;
      volumeSize = 0;
      verbose = debug = noPrompt = false;
    }
  
    size_t bufferSize;
    uint64_t volumeSize;
    bool verbose;
    bool debug;
    bool noPrompt;
    string outputCommand;
  } parameters;
}


static struct predef {
  const char* name;
  uint64_t size;
} predefinedSizes[]= { 
  {"floppy", 1440000 }, 
  {"CD", 650000384ULL }, 
  {"CD-80", 700000256ULL }, 
  {"CDR-80", 700000256ULL }, 
  {"DVD", 4700000256ULL }, 
  {"DVD-5", 4700000256ULL }, 
  {0, 0} 
};

uint64_t getSize(const char* desc) 
{
  for(struct predef* p=predefinedSizes; p->name; ++p) {
    if(!strcasecmp(p->name, desc))
      return p->size;
  }
  return atoi(desc)*1024;
}

bool g_havebreak;

void breakHandler(int t)
{
  cerr<<"\nsplitpipe: Received interrupt request, terminating output"<<endl;
  g_havebreak=true;
}

pid_t g_pid;
void waitForOutputCommandToDie()
{
  int status;
  if(waitpid(g_pid, &status, 0) < 0)
    unixDie("wait on child process");
  
  if(WIFEXITED(status))
    cerr<<"\nsplitpipe: output command exited with status "<<WEXITSTATUS(status)<<endl;
  else {
    cerr<<"\nsplitpipe: output command exited abnormally";
    if(WIFSIGNALED(status))
      cerr<<", by signal "<<WTERMSIG(status);
    cerr<<endl;
  }
}


void outputGaveEof(int outputfd)
{
  cerr<<"\nsplitpipe: output command gave EOF, waiting for it to exit"<<endl;
  close(outputfd);
  waitForOutputCommandToDie();
  cerr<<"\nsplitpipe: future versions of splitpipe may allow you to continue, but for now.. exit\n";
  exit(EXIT_FAILURE);
}

static void usage()
{
  cerr<<"splitpipe divides its input over several volumes.\n";
  cerr<<"\nsyntax: ... | joinpipe [options] \n\n";
  cerr<<" --buffer-size, -b\tSize of buffer before output, in megabytes"<<endl;
  cerr<<" --volume-size, -s\tSize of output volumes, in kilobytes. See below"<<endl;
  cerr<<" --help, -h\t\tGive this helpful message"<<endl;
  cerr<<" --no-prompt, -n\tRun without user intervention\n";
  cerr<<" --output, -o\t\tThe output script that will be spawned for each volume"<<endl;
  cerr<<" --verbose, -v\t\tGive verbose output\n";
  cerr<<" --version\t\tReport version\n\n";

  cerr<<"predefined volume sizes: \n";
  for(struct predef* p=predefinedSizes; p->name; ++p) 
    cerr<<"\t"<<p->name<<"\t"<<p->size<<" bytes\n";
  exit(1);
  
}

static void ParseCommandline(int argc, char** argv)
{
  int c;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"buffer-size", 1, 0, 'b'},
      {"volume-size", 1, 0, 's'},
      {"no-prompt", 0, 0, 'n'},
      {"output", 1, 0, 'o'},
      {"debug", 0, 0, 'd'},
      {"verbose", 0, 0, 'v'},
      {"version", 0, 0, 'e'},
      {"help", 0, 0, 'h'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long (argc, argv, "b:ns:deho:v",
		     long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
    case 'b':
      parameters.bufferSize=1024*atoi(optarg);
      break;
    case 's':
      parameters.volumeSize=getSize(optarg);
      break;
    case 'd':
      parameters.debug=1;
      break;
    case 'e':
      cerr<<"splitpipe "VERSION" (C) 2005 Netherlabs Computer Consulting BV\nReport bugs to bert hubert <ahu@ds9a.nl>"<<endl;
      exit(EXIT_SUCCESS);
    case 'h':
      usage();
      break;
    case 'o':
      parameters.outputCommand=optarg;
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

    }
  }
}



int spawnOutputThread()
{
  int d_fds[2];
  if(pipe(d_fds) < 0)
    unixDie("unable creating pipe");
  
  int pid=fork();
  if(pid < 0)
    unixDie("Error during fork");

  if(pid) { // parent 
    g_pid=pid;
    close(d_fds[0]);

  } else {
    close(d_fds[1]);
    dup2(d_fds[0], 0); // connect to stdin
    
    char* argvp[4];
    argvp[0]="/bin/sh";
    argvp[1]="-c";
    argvp[2]=(char*)parameters.outputCommand.c_str();
    argvp[3]=0;

    if(execvp(argvp[0], argvp))
      unixDie("launch of output script");

    cerr<<"We should Never Ever end up here"<<endl;
  }
  setNonBlocking(d_fds[1]);

  return d_fds[1];
}

void appendStretch(int type, const string& content, string& stretches)
{
  struct stretchHeader stretch;
  stretch.type=type;
  stretch.size=htons(content.size());

  char *p=(char*) &stretch;
  stretches+=string(p, p + sizeof(stretch));
  stretches+=content;
}

void outputChecksum(int outputfd, const MD5Summer& md5)
{
  string stretches;
  appendStretch(stretchHeader::MD5Checksum, md5.get(), stretches);

  int ret=writen(outputfd, stretches.c_str(), stretches.length(), "write of meta-data to output command");
  
  if(!ret)
    outputGaveEof(outputfd);
}

string g_uuid;

int outputPerVolumeStretches(int fd, uint16_t volumeNumber)
{
  string stretches;
  
  appendStretch(stretchHeader::SessionUUID, g_uuid, stretches);

  volumeNumber = htons(volumeNumber);
  const string volNumString((char*)&volumeNumber, ((char*)&volumeNumber) + 2);
  
  appendStretch(stretchHeader::VolumeNumber,volNumString, stretches);

  int ret=writen(fd, stretches.c_str(), stretches.length(), "write of per-volume meta-data to output command");

  if(!ret)
    outputGaveEof(fd);

  return stretches.length();
}


int SplitpipeMain(int argc, char** argv)
try
{
  parameters.bufferSize=1000000;
  parameters.volumeSize=getSize("DVD-5");
  ParseCommandline(argc, argv);

  signal(SIGPIPE, SIG_IGN);

  //  signal(SIGINT, breakHandler);

  cerr.setf(ios::fixed);
  cerr.precision(2);

  if(parameters.verbose) {
    cerr<<"Buffer size: " << parameters.bufferSize/1000000.0 <<" MB\n";
    cerr<<"Volume size: " << parameters.volumeSize/1000000.0 <<" MB\n";
    //    cerr<<"Output command: "<<parameters.outputCommand<<endl;
  }

  if(parameters.outputCommand.empty()) {
    cerr<<"No output command specified - unable to write data\n\n";
    cerr<<"Suggested command for cd: \n";
    cerr<<".. -o 'cdrecord dev=/dev/cdrom speed=24 -eject -dummy -tao'\n";
    cerr<<"\nSuggested command for dvd: \n";
    cerr<<".. -o 'growisofs -Z/dev/dvd=/dev/stdin -dry-run'\n";
    usage();
  }

  if(!parameters.bufferSize) {
    cerr<<"Buffer size set to zero, which is unsupported. Try 1000 for 1 megabyte"<<endl;
    exit(1);
  }

  if(!parameters.volumeSize) {
    cerr<<"Volume size set to zero, which is unsupported. Try --volume-size DVD for DVD-size volumes"<<endl;
    exit(1);
  }

  g_uuid=generateUUID();

  parameters.volumeSize -= 2048; // leave room for last stretch

  RingBuffer rb(parameters.bufferSize);
  setNonBlocking(0);

  bool inputEof=false;
  bool outputOnline=false;
  int outputfd=-1;
  uint64_t amountOutput=0;
  uint16_t leftInStretch=0;
  int numStretches=0;
  int volumeNumber=0;

  MD5Summer md5;

  char *buffer = new char[parameters.bufferSize];

  if(parameters.verbose) 
    cerr<<"Prebuffering before starting output script..";

  bool d_firstvolume=true; // first volume does not get the 'press enter' stuff

  while(1) {
    if(!outputOnline && (inputEof || (1.0 * rb.available() / parameters.bufferSize > 0.5))) {
      if(d_firstvolume) {
	cerr<<" done"<<endl;;
	d_firstvolume=false;
      }
      else {
	cerr<<"splitpipe: reload media, if necessary, and press enter to continue"<<endl;
	if(!parameters.noPrompt)
	  waitForUser();
      }

      cerr<<"splitpipe: bringing output script online - buffer " << 100.0*rb.available() / parameters.bufferSize <<"% full"<<endl;
      outputfd=spawnOutputThread();
      amountOutput = outputPerVolumeStretches(outputfd, volumeNumber++);      
      outputOnline=true;
    }

    fd_set inputs;
    fd_set outputs;

    FD_ZERO(&inputs);
    FD_ZERO(&outputs);

    if(!inputEof && rb.room())
      FD_SET(0, &inputs);

    if(rb.available() && outputOnline)
      FD_SET(outputfd, &outputs);

    int ret=select( outputOnline ? (outputfd + 1) : 1, 
		    &inputs, &outputs, 0, 0);
    if(ret < 0)
      unixDie("select returned an error");

    if(!ret)  // odd
      continue;
    
    size_t bytesRead=0;

    if(!inputEof && FD_ISSET(0, &inputs)) {
      ret=read(0, buffer, min((size_t)1000000, rb.room()));
      if(ret < 0)
	unixDie("Reading from standard input");
      if(!ret) {
	if(parameters.debug) 
	  cerr<<"EOF on input, room in buffer: "<<rb.room()<<endl;
	inputEof=true;
      } else {
	if(parameters.debug)
	  cerr<<"Read "<<ret<<" bytes, and stored them"<<endl;
	rb.store(buffer, ret);
	bytesRead=ret;
      }
    }

    if(outputOnline && rb.available() &&  FD_ISSET(outputfd, &outputs)) {
      uint64_t totalBytesOutput=0;
      do {
	const char *rbuffer;
	size_t lenAvailable;
	rb.get(&rbuffer, &lenAvailable);

	uint64_t leftInVolume=parameters.volumeSize - amountOutput;

	if(leftInVolume >= 3 && !leftInStretch) {   // only start a new stretch if there is room for at least 1 byte
	  leftInStretch=min((size_t)0xffff, lenAvailable);
	  leftInStretch=min((uint64_t)leftInStretch, leftInVolume - 3);
	  if(parameters.debug) 
	    cerr<<"splitpipe: starting a stretch of "<<leftInStretch<<" bytes"<<endl;

	  struct stretchHeader stretch;

	  stretch.size=htons(leftInStretch);
	  stretch.type=stretchHeader::Data;

	  ret=writen(outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");

	  if(!ret)
	    outputGaveEof(outputfd);
	  
	  amountOutput += sizeof(stretch);
	  numStretches++;
	  break; // go past select again, not sure if this is needed
	}



	size_t len=min((uint64_t)lenAvailable, leftInVolume);
	
	len=min(len, (size_t)leftInStretch);

	if(!len) {
	  cerr<<"\nsplitpipe: output a full volume, waiting for output command to exit..\n";
	  struct stretchHeader stretch;

	  outputChecksum(outputfd, md5);

	  stretch.size=0;
	  stretch.type=stretchHeader::VolumeEOF;

	  ret=writen(outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");

	  if(!ret)
	    outputGaveEof(outputfd);


	  close(outputfd);

	  waitForOutputCommandToDie();
	  
	  outputOnline=false;
	  outputfd=-1;
	  amountOutput=0;
	  break; 
	}

	ret=write(outputfd, rbuffer, len);
	if(ret < 0) {
	  if(errno==EAGAIN)
	    break;
	  else
	    unixDie("write to standard output");
	}

	if(!ret) {
	  outputGaveEof(outputfd);
	}
	if(parameters.debug) 
	  cerr<<"Wrote out "<<ret<<" out of "<<len<<" bytes"<<endl;
	md5.feed(rbuffer, ret);
	rb.advance(ret);

	amountOutput += ret;
	leftInStretch -= ret;
	totalBytesOutput += ret;

	if(parameters.debug) 
	  cerr<<"There are now "<<rb.available()<<" bytes left in the rb"<<endl;
      } while(totalBytesOutput < bytesRead && rb.available());

    }

    if(inputEof && !rb.available())
      break;
  }

  if(outputOnline) {
    cerr<<"\nsplitpipe: done with input, waiting for output script to exit..\n";

    outputChecksum(outputfd, md5);

    struct stretchHeader stretch;
    stretch.size=0;
    stretch.type=stretchHeader::SessionEOF;
    
    int ret=writen(outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");
    
    if(!ret)
      outputGaveEof(outputfd);

    close(outputfd);
    waitForOutputCommandToDie();
  }
  if(parameters.verbose)
    cerr<<"splitpipe: output "<<numStretches<<" stretches\n";
  return EXIT_SUCCESS;
}
catch(exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
  return EXIT_FAILURE;
}

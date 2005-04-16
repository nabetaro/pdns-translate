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
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdint.h>
#include <signal.h>

#include "ringbuffer.hh"

struct {
  size_t bufferSize;
  uint64_t chunkSize;
  bool verbose;
  bool debug;
  vector<string> outputCommand;
} parameters;


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

void unixDie(const string& during)
{
  throw runtime_error("during "+string(during)+": "+strerror(errno));
}


void setNonBlocking(int fd)
{
  int flags=fcntl(fd,F_GETFL,0);
  if(flags<0 || fcntl(fd, F_SETFL,flags|O_NONBLOCK) <0)
    unixDie("Setting filedescriptor to nonblocking failed");
}

double getTime()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec/1000000.0;
}


void usage()
{
  cerr<<"splitpipe syntax:\n\n";
  cerr<<" --buffer-size, -b\tSize of buffer before output, in megabytes"<<endl;
  cerr<<" --chunk-size, -c\tSize of output chunks, in kilobytes, or use 'DVD', 'CDR' or 'CDR-80'"<<endl;
  cerr<<" --help, -h\t\tGive this helpful message"<<endl;
  cerr<<" --verbose, -v\t\tGive verbose output\n\n";

  cerr<<"predefined chunk sizes: \n";
  for(struct predef* p=predefinedSizes; p->name; ++p) 
    cerr<<"\t"<<p->name<<"\t"<<p->size<<" bytes\n";
  exit(1);
  
}

void ParseCommandline(int argc, char** argv)
{
  int c;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"buffer-size", 1, 0, 'b'},
      {"chunk-size", 1, 0, 'c'},
      {"debug", 0, 0, 'd'},
      {"verbose", 0, 0, 'v'},
      {"help", 0, 0, 'h'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long (argc, argv, "b:c:dhv",
		     long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
    case 'b':
      parameters.bufferSize=1024*atoi(optarg);
      break;
    case 'c':
      parameters.chunkSize=getSize(optarg);
      break;
    case 'd':
      parameters.debug=1;
      break;
    case 'h':
      usage();
      break;
    case 'v':
      parameters.verbose=true;
      break;
    }
  }
  if (optind < argc) {
    while (optind < argc) {
      parameters.outputCommand.push_back(argv[optind++]);
    }
  }
}

pid_t g_pid;

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
    
    char* argvp[parameters.outputCommand.size() + 1];
    argvp[0]=const_cast<char*>(parameters.outputCommand[0].c_str()); // FOAD 1
    unsigned int n=0;
    for(; n < parameters.outputCommand.size() ; ++n)
      argvp[n]=const_cast<char*>(parameters.outputCommand[n].c_str());
    argvp[n]=0;

    if(execvp(argvp[0], argvp))
      unixDie("launch of output script");

    cerr<<"We should Never Ever end up here"<<endl;
  }
  return d_fds[1];
}

void waitForUser()
{	
  FILE *fp=fopen("/dev/tty", "r");
  if(!fp) 
    unixDie("opening of /dev/tty for user input");
  
  char line[80];
  fgets(line, sizeof(line) - 1, fp);
  fclose(fp);
}

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

int main(int argc, char** argv)
try
{
  parameters.debug=parameters.verbose=false;
  parameters.bufferSize=1000000;
  parameters.chunkSize=getSize("DVD-5");
  ParseCommandline(argc, argv);

  signal(SIGPIPE, SIG_IGN);

  //  signal(SIGINT, breakHandler);

  cerr.setf(ios::fixed);
  cerr.precision(2);

  if(parameters.verbose) {
    cerr<<"Buffer size: " << parameters.bufferSize/1000000.0 <<" MB\n";
    cerr<<"Chunk size: " << parameters.chunkSize/1000000.0 <<" MB\n";
    //    cerr<<"Output command: "<<parameters.outputCommand<<endl;
  }

  if(parameters.outputCommand.empty()) {
    cerr<<"No output command specified - unable to write data\n\n";
    cerr<<"Suggested command for cd: \n";
    cerr<<"cdrecord dev=/dev/cdrom speed=24 -eject -dummy -tao\n";
    cerr<<"\nSuggested command for dvd: \n";
    cerr<<"growisofs -Z/dev/dvd=/dev/stdin -dry-run\n";
    exit(1);
  }

  if(!parameters.bufferSize) {
    cerr<<"Buffer size set to zero, which is unsupported. Try 1000 for 1 megabyte"<<endl;
    exit(1);
  }

  if(!parameters.chunkSize) {
    cerr<<"Chunk size set to zero, which is unsupported. Try --chunk-size DVD for DVD-size chunks"<<endl;
    exit(1);
  }

  RingBuffer rb(parameters.bufferSize);
  setNonBlocking(0);
  setNonBlocking(1);

  bool inputEof=false;
  bool outputOnline=false;
  int outputfd;
  uint64_t amountOutput=0;

  char *buffer = new char[parameters.bufferSize];

  if(parameters.verbose) 
    cerr<<"Prebuffering before starting output script..";

  bool d_firstchunk=true;

  while(1) {

    if(!outputOnline && (inputEof || (1.0 * rb.available() / parameters.bufferSize > 0.5))) {
      if(d_firstchunk) {
	cerr<<" done"<<endl;;
	d_firstchunk=false;
      }
      else {
	cerr<<"splitpipe: reload media, if necessary, and press enter to continue"<<endl;
	waitForUser();

      }

      cerr<<"splitpipe: bringing output script online - buffer " << 100.0*rb.available() / parameters.bufferSize <<"% full"<<endl;
      outputfd=spawnOutputThread();
      outputOnline=true;
      amountOutput=0;
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

	uint64_t leftInChunk=parameters.chunkSize - amountOutput;

	size_t len=min((uint64_t)lenAvailable, leftInChunk);
	
	if(!len) {
	  cerr<<"\nsplitpipe: output a full chunk, waiting for output command to exit..\n";
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
	  cerr<<"\nsplitpipe: output command gave EOF, waiting for it to exit"<<endl;
	  close(outputfd);
	  waitForOutputCommandToDie();
	  cerr<<"\nsplitpipe: future versions of splitpipe may allow you to continue, but for now.. exit\n";
	  exit(EXIT_FAILURE);
	}
	if(parameters.debug) 
	  cerr<<"Wrote out "<<ret<<" out of "<<len<<" bytes"<<endl;
	rb.advance(ret);

	amountOutput+=ret;

	totalBytesOutput+=ret;

	if(parameters.debug) 
	  cerr<<"There are now "<<rb.available()<<" bytes left in the rb"<<endl;
      } while(totalBytesOutput < bytesRead && rb.available());

    }

    if(inputEof && !rb.available())
      break;
  }

  if(outputOnline) {
    cerr<<"\nsplitpipe: done with input, waiting for output script to exit..\n";
    close(outputfd);
    waitForOutputCommandToDie();
  }
}
catch(exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

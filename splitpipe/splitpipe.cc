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
#include <ncurses.h>
#include "misc.hh"
#include "ringbuffer.hh"
#include "md5.hh"
#include "display.hh"

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
    string label;
  } parameters;
}

bool g_wantsbreak;

void breakHandler(int)
{
  g_wantsbreak=true;
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

class SplitpipeClass
{
public:
  int go(int argc, char**argv);
  SplitpipeClass() : d_spd(0)
  {}
  ~SplitpipeClass()
  {
    delete d_spd;
  }
private:
  void outputChecksum( const MD5Summer& md5);
  int outputPerVolumeStretches(uint16_t volumeNumber);
  void appendStretch(int type, const string& content, string& stretches);
  void spawnOutputThread();
  void usage();
  bool checkDeathOutputCommand( bool doWait=false);
  void outputGaveEof();
  void ParseCommandline(int argc, char** argv);
  void updateDisplay(RingBuffer& rb);
  SplitpipeDisplay* d_spd;
  int d_outputfd, d_stdoutfd, d_stderrfd;
  pid_t d_pid;
  string d_uuid;
  int d_lastPercentage;
  time_t d_lastRefresh;
  uint64_t d_amountOutputVolume, d_grandTotalOut, d_grandTotalIn;

};

bool SplitpipeClass::checkDeathOutputCommand(bool doWait)
{
  int status, ret;
  if((ret=waitpid(d_pid, &status, doWait ? 0 : WNOHANG)) < 0)
    unixDie("wait on child process");
  
  if(!ret)
    return false;

  if(WIFEXITED(status))
    d_spd->log("output command exited with status %d",WEXITSTATUS(status));
  else {
    d_spd->log("output command exited abnormally");
    if(WIFSIGNALED(status))
      d_spd->log("was killed by signal %d",WTERMSIG(status));

  }
  return true;
}


void SplitpipeClass::outputGaveEof()
{
  endwin();
  cerr<<"\noutput command gave EOF, waiting for it to exit"<<endl;
  close(d_outputfd);
  checkDeathOutputCommand(true);
  cerr<<"\nfuture versions of splitpipe may allow you to continue, but for now.. exit\n";
  exit(EXIT_FAILURE);
}

void SplitpipeClass::usage()
{
  cerr<<"splitpipe divides its input over several volumes.\n";
  cerr<<"\nsyntax: ... | joinpipe [options] \n\n";
  cerr<<" --buffer-size, -b\tSize of buffer before output, in megabytes"<<endl;
  cerr<<" --volume-size, -s\tSize of output volumes, in kilobytes. See below"<<endl;
  cerr<<" --help, -h\t\tGive this helpful message"<<endl;
  cerr<<" --label, -l\t\tGive textual label for this session"<<endl;
  cerr<<" --no-prompt, -n\tRun without user intervention\n";
  cerr<<" --output, -o\t\tThe output script that will be spawned for each volume"<<endl;
  cerr<<" --verbose, -v\t\tGive verbose output\n";
  cerr<<" --version\t\tReport version\n\n";

  cerr<<"predefined volume sizes: \n";
  for(struct predef* p=predefinedSizes; p->name; ++p) 
    cerr<<"\t"<<p->name<<"\t"<<p->size<<" bytes\n";
  exit(1);
}

void SplitpipeClass::ParseCommandline(int argc, char** argv)
{
  int c;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"buffer-size", 1, 0, 'b'},
      {"volume-size", 1, 0, 's'},
      {"label", 0, 0, 'L'},
      {"no-prompt", 0, 0, 'n'},
      {"output", 1, 0, 'o'},
      {"debug", 0, 0, 'd'},
      {"verbose", 0, 0, 'v'},
      {"version", 0, 0, 'e'},
      {"help", 0, 0, 'h'},
      {0, 0, 0, 0}
    };
    
    c = getopt_long (argc, argv, "b:L:ns:deho:v",
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
    case 'L':
      parameters.label=optarg;
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
}

void SplitpipeClass::spawnOutputThread()
{
  int inpipefds[2], stdoutfds[2], stderrfds[2];
  if(pipe(inpipefds) < 0 || pipe(stdoutfds) < 0 || pipe(stderrfds) < 0)
    unixDie("unable creating pipe");
  
  int pid=fork();
  if(pid < 0)
    unixDie("Error during fork");

  if(pid) { // parent 
    d_pid=pid;
    close(inpipefds[0]);
    close(stdoutfds[1]);
    close(stderrfds[1]);

  } else {
    close(inpipefds[1]);
    close(stdoutfds[0]);
    close(stderrfds[0]);
    dup2(inpipefds[0], 0); // connect to stdin


    if(dup2(stdoutfds[1], 1) < 0) 
      unixDie("dup2 of stdout"); // connect to stdout

    if(dup2(stderrfds[1], 2) < 0)
      unixDie("dup2 of stderr"); // connect to stderr
    
    char* argvp[4];
    argvp[0]="/bin/sh";
    argvp[1]="-c";
    argvp[2]=(char*)parameters.outputCommand.c_str();
    argvp[3]=0;

    if(execvp(argvp[0], argvp))
      unixDie("launch of output script");

    cerr<<"We should Never Ever end up here"<<endl;
  }
  setNonBlocking(inpipefds[1]);

  d_outputfd=inpipefds[1];
  d_stdoutfd=stdoutfds[0];
  d_stderrfd=stderrfds[0];
}

void SplitpipeClass::appendStretch(int type, const string& content, string& stretches)
{
  struct stretchHeader stretch;
  stretch.type=type;
  stretch.size=htons(content.size());

  char *p=(char*) &stretch;
  stretches+=string(p, p + sizeof(stretch));
  stretches+=content;
}

void SplitpipeClass::outputChecksum( const MD5Summer& md5)
{
  string stretches;
  appendStretch(stretchHeader::MD5Checksum, md5.get(), stretches);

  int ret=writen(d_outputfd, stretches.c_str(), stretches.length(), "write of meta-data to output command");
  
  if(!ret)
    outputGaveEof();
}



int SplitpipeClass::outputPerVolumeStretches(  uint16_t volumeNumber)
{
  string stretches;
  
  appendStretch(stretchHeader::SessionUUID, d_uuid, stretches);

  uint32_t now=static_cast<uint32_t>(time(0)); // this saves us from having to worry about 64 bit time_t until February 2106 or so!
  now=htonl(now);
  string nowString((char*)&now, ((char*)&now) + 4);
  appendStretch(stretchHeader::VolumeDate, nowString, stretches);

  if(!parameters.label.empty())
    appendStretch(stretchHeader::SessionName, parameters.label, stretches);

  volumeNumber = htons(volumeNumber);
  const string volNumString((char*)&volumeNumber, ((char*)&volumeNumber) + 2);
  
  appendStretch(stretchHeader::VolumeNumber, volNumString, stretches);



  int ret=writen(d_outputfd, stretches.c_str(), stretches.length(), "write of per-volume meta-data to output command");

  if(!ret)
    outputGaveEof();

  return stretches.length();
}


void SplitpipeClass::updateDisplay(RingBuffer& rb)
{
  int newPercentage=round(100.0*rb.available()/parameters.bufferSize);
  
  if(d_lastPercentage != newPercentage) {
    d_spd->setBarPercentage(newPercentage);
    d_lastPercentage = newPercentage;
  }
  if(time(0) != d_lastRefresh) {
    d_spd->setTotalBytes(d_grandTotalIn, d_grandTotalOut, rb.available(), 
			 (int)(100.0 * d_amountOutputVolume / parameters.volumeSize));
    d_lastRefresh=time(0);
  }
}

void waitForUserCurses()
{	
  int fd=open("/dev/tty", O_RDONLY);
  if(fd < 0) 
    unixDie("opening of /dev/tty for user input");

  char c;
  read(fd, &c, 1);
  close(fd);
}


int SplitpipeMain(int argc, char** argv)
try
{
  SplitpipeClass spc;
  return spc.go(argc, argv);
}
catch(exception &e)
{
  cerr<<"\n\rFatal: "<<e.what()<<endl;
  return EXIT_FAILURE;
}

int SplitpipeClass::go(int argc, char**argv) 
{
  parameters.bufferSize=1000000;
  parameters.volumeSize=getSize("DVD-5");
  ParseCommandline(argc, argv);

  signal(SIGPIPE, SIG_IGN);

  signal(SIGINT, breakHandler);

  cerr.setf(ios::fixed);
  cerr.precision(2);


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

  d_uuid=generateUUID();

  parameters.volumeSize -= 2048; // leave room for last stretch

  RingBuffer rb(parameters.bufferSize);
  setNonBlocking(0);
  d_stdoutfd = d_stderrfd = -1;

  d_spd = new SplitpipeDisplay;

  if(parameters.verbose) {
    d_spd->log("Buffer size: %.1f MB", parameters.bufferSize/1000000.0);
    d_spd->log("Volume size: %.1f MB", parameters.volumeSize/1000000.0);
  }


  bool inputEof=false;
  enum {Dead, Working, Dying} outputStatus=Dead;
  d_outputfd=-1;
  d_amountOutputVolume=d_grandTotalOut=d_grandTotalIn=0;
  d_lastRefresh = 0; d_lastPercentage=0;
  uint16_t leftInStretch=0;
  int numStretches=0;
  int volumeNumber=0;

  MD5Summer md5;

  char *buffer = new char[100000];  // XXX FIXME! This is waaaay too much

  if(parameters.verbose) {
    d_spd->log("%s","Prebuffering before starting output program");
  }

  bool d_firstvolume=true; // first volume does not get the 'press enter' stuff
  bool waitingForUser=false;
  int d_ttyfd=open("/dev/tty",O_RDONLY);
  if(d_ttyfd < 0)
    unixDie("opening of keyboard for input");

  while(1) { 
    if(!waitingForUser && outputStatus==Dead && !g_wantsbreak && (inputEof || (1.0 * rb.available() / parameters.bufferSize > 0.5))) {
      if(!parameters.noPrompt && !d_firstvolume) {
	d_spd->setLogStandout(true);
	d_spd->log("%s","reload media, if necessary, and press enter to continue");
	d_spd->setLogStandout(false);
	
	waitingForUser=true;
      }
      else {
	spawnOutputThread();
	d_spd->log("sending data to output script");
	d_amountOutputVolume = outputPerVolumeStretches(volumeNumber++);      
	outputStatus=Working;
	waitingForUser=false;
	d_firstvolume=false;
      }
    }

    if(outputStatus!=Dead && d_stdoutfd < 0 && d_stderrfd < 0 && checkDeathOutputCommand()) {
      if(!inputEof && (1.0 * rb.available() / parameters.bufferSize <= 0.5))
	d_spd->log("waiting for buffer to fill before starting new output volume");
											      
      outputStatus = Dead;
    }

    fd_set inputs;
    fd_set outputs;

    FD_ZERO(&inputs);
    FD_ZERO(&outputs);

    if(!inputEof && rb.room())
      FD_SET(0, &inputs);

    if(rb.available() && outputStatus==Working)
      FD_SET(d_outputfd, &outputs);

    if(d_stdoutfd > 0)
      FD_SET(d_stdoutfd, &inputs);

    if(d_stderrfd > 0)
      FD_SET(d_stderrfd, &inputs);

    FD_SET(d_ttyfd, &inputs);

    updateDisplay(rb);

    if(waitingForUser && g_wantsbreak) {
      d_spd->log("user interrupt received");
      close(d_outputfd);
      d_outputfd=-1;
      outputStatus=Dying;
      break;
    }


    struct timeval tv={0,10000};
    int ret=select( max(0,max(d_ttyfd,max(d_stdoutfd, d_stderrfd)))+1,  // XXX FIXME somewhat dodgy
		    &inputs, &outputs, 0, &tv);
    if(ret < 0) {
      if(errno == EINTR) 
	continue;
      unixDie("select returned an error");
    }

    if(!ret)  
      continue;
    
    size_t bytesRead=0;

    if(d_stdoutfd > 0 && FD_ISSET(d_stdoutfd, &inputs)) {
      ret=read(d_stdoutfd, buffer, 1024);
      if(ret > 0) {
	for(int counter=0;counter<ret;++counter)
	  d_spd->programPut(buffer[counter]);
	d_spd->programCommit();
      }
      if(!ret) {
	close(d_stdoutfd);
	d_stdoutfd=-1;
      }
    }

    if(FD_ISSET(d_ttyfd, &inputs)) {
      char c=0;
      if(read(d_ttyfd, &c, 1)==1) {
	if(waitingForUser && (c=='\r' || c=='\n')) {
	  d_spd->log("bringing output script online. Buffer %.02f%% full", (100.0 * rb.available() / parameters.bufferSize));
	  
	  spawnOutputThread();
	  d_spd->log("sending data to output script");
	  d_amountOutputVolume = outputPerVolumeStretches(volumeNumber++);      
	  outputStatus=Working;
	  waitingForUser=false;
	}
	else if(c==12) {
	  d_spd->refresh();
	}
      }
    }


    if(d_stderrfd > 0 && FD_ISSET(d_stderrfd, &inputs)) {
      ret=read(d_stderrfd, buffer, 1024);
      if(ret > 0) {
	for(int counter=0;counter<ret;++counter)
	  d_spd->programPut(buffer[counter]);
      }
      if(!ret) {
	close(d_stderrfd);
	d_stderrfd=-1;
      }
    }

    if(!inputEof && FD_ISSET(0, &inputs)) {
      ret=read(0, buffer, min((size_t)100000, rb.room()));
      if(ret < 0)
	unixDie("Reading from standard input");
      else if(!ret) {
	if(parameters.debug) 
	  cerr<<"EOF on input, room in buffer: "<<rb.room()<<endl;
	d_spd->log("all data in memory, draining buffer");
	inputEof=true;
      } else {
	if(parameters.debug)
	  cerr<<"Read "<<ret<<" bytes, and stored them"<<endl;
	rb.store(buffer, ret);
	bytesRead=ret;
	d_grandTotalIn+=bytesRead;
      }
    }


    if(outputStatus==Working && rb.available() &&  FD_ISSET(d_outputfd, &outputs)) {
      uint64_t total=0;
      do {
	const char *rbuffer;
	size_t lenAvailable;
	rb.get(&rbuffer, &lenAvailable);

	uint64_t leftInVolume=parameters.volumeSize - d_amountOutputVolume;

	if(leftInVolume >= 3 && !leftInStretch) {   // only start a new stretch if there is room for at least 1 byte
	  leftInStretch=min((size_t)0xffff, lenAvailable);
	  leftInStretch=min((uint64_t)leftInStretch, leftInVolume - 3);
	  if(parameters.debug) 
	    cerr<<"starting a stretch of "<<leftInStretch<<" bytes"<<endl;

	  struct stretchHeader stretch;

	  stretch.size=htons(leftInStretch);
	  stretch.type=stretchHeader::Data;

	  ret=writen(d_outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");

	  if(!ret)
	    outputGaveEof();
	  
	  d_amountOutputVolume += sizeof(stretch);
	  d_grandTotalOut += sizeof(stretch);
	  numStretches++;
	  break; // go past select again, not sure if this is needed
	}

	size_t len=min((uint64_t)lenAvailable, leftInVolume);
	len=min(len, (size_t)leftInStretch);

	if(!len) {
	  d_spd->log("%s","Output a full volume, waiting for output command to exit..");
	
	  struct stretchHeader stretch;

	  outputChecksum(md5);

	  stretch.size=0;
	  stretch.type=stretchHeader::VolumeEOF;

	  ret=writen(d_outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");

	  if(!ret)
	    outputGaveEof();

	  d_grandTotalOut+=ret;
	  close(d_outputfd);

	  outputStatus=Dying;
	  d_outputfd=-1;
	  d_amountOutputVolume=0;
	  break; 
	}

	ret=write(d_outputfd, rbuffer, len);
	if(ret < 0) {
	  if(errno==EAGAIN)
	    break;
	  else if(g_wantsbreak && errno==EPIPE) {
	    close(d_outputfd);
	    d_outputfd=-1;
	    outputStatus=Dying;
	    break;
	  }
	  else
	    unixDie("write to standard output");
	}

	if(!ret) {
	  outputGaveEof();
	}
	if(parameters.debug) 
	  cerr<<"Wrote out "<<ret<<" out of "<<len<<" bytes"<<endl;
	md5.feed(rbuffer, ret);
	rb.advance(ret);

	d_amountOutputVolume += ret;
	d_grandTotalOut += ret;
	leftInStretch -= ret;
	total += ret;

	if(parameters.debug) 
	  cerr<<"There are now "<<rb.available()<<" bytes left in the rb"<<endl;
      } while(total < bytesRead && rb.available());
    }

    if(inputEof && !rb.available())
      break;

    if(g_wantsbreak && d_stdoutfd==-1) {
      d_spd->log("Received break - exiting");
      break;
    }
  }

  // XXX FIXME: deal with outputStatus == Dying 
  if(outputStatus==Working) {
    d_spd->log("done with input, waiting for output script to exit..");

    outputChecksum(md5);

    struct stretchHeader stretch;
    stretch.size=0;
    stretch.type=stretchHeader::SessionEOF;
    
    int ret=writen(d_outputfd, &stretch, sizeof(stretch),"write of meta-data to output command");
    
    if(!ret)
      outputGaveEof();

    close(d_outputfd);
    checkDeathOutputCommand(true);
  }
  cerr<<endl;
  return EXIT_SUCCESS;
}

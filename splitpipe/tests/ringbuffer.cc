#include "ringbuffer.hh"

int main()
try
{
  RingBuffer rb(1000000);
  cerr<<"Available: "<<rb.available()<<endl;
  cerr<<"Room: "<<rb.room()<<endl;

  char buffer[1024];
  cerr<<"Now storing 1024 bytes"<<endl;
  rb.store(buffer, 1024);

  cerr<<"Available: "<<rb.available()<<endl;
  cerr<<"Room: "<<rb.room()<<endl;

  cerr<<"Now getting the data pointer.."<<endl;
  const char *buffer2;
  size_t len;

  rb.get(&buffer2, &len);

  cerr<<"It reports "<<len<<" bytes ready for us consecutively, advancing"<<endl;
  rb.advance(len);

  cerr<<"Available: "<<rb.available()<<endl;
  cerr<<"Room: "<<rb.room()<<endl;


  cerr<<"Now storing the full buffer capacity"<<endl;
  char buffer3[1000000];
  rb.store(buffer3, 1000000);

  cerr<<"Available: "<<rb.available()<<endl;
  cerr<<"Room: "<<rb.room()<<endl;

  cerr<<"Asking to retrieve data, won't get it all since we are wrapped"<<endl;
  rb.get(&buffer2, &len);
  cerr<<"Gave us a pointer to "<<len<<" bytes, advancing to see if you get the rest"<<endl;
  rb.advance(len);

  cerr<<"Asking to retrieve data, we should get the rest"<<endl;
  rb.get(&buffer2, &len);
  cerr<<"Gave us a pointer to "<<len<<" bytes"<<endl;
  rb.advance(len);

  cerr<<"Available: "<<rb.available()<<endl;
  cerr<<"Room: "<<rb.room()<<endl;

  try {
    cerr<<"Should give exception, was asked to store more than was room for: "<<endl;
    rb.store(buffer3, 1000001);
  }
  catch(exception &e)
  {
    cerr<<"error: "<<e.what()<<endl;
  }
  
  try {
    cerr<<"Should now generate another exception, buffer is full already: "<<endl;
    rb.store(buffer3,1000000);
    cerr<<"Loaded buffer up, exception is on next store: "<<endl;
    rb.store(buffer3,1);
  }
  catch(exception &e)
  {
    cerr<<"error: "<<e.what()<<endl;
  }

  rb.advance(1000000);
  cerr<<"Available should be 0 now: "<<rb.available()<<endl;

  rb.store(buffer3, 100000);
  cerr<<"Stuffed in 100000 bytes in one chunk, now adding 900000 times 1 byte"<<endl;
  for(unsigned n=0;n<900000;++n)
    rb.store(buffer3,1);
  
  cerr<<"There are now "<<rb.available()<<" bytes available"<<endl;
  cerr<<"Advancing 1000000 times 1 byte"<<endl;
  for(unsigned int n=0; n< 1000000; ++n)
    rb.advance(1);

  cerr<<"There are now "<<rb.available()<<" bytes available"<<endl;
  
  if(rb.available()==0) {
    cerr<<"*** Everything went well!"<<endl;
    exit(EXIT_SUCCESS);
  }
  exit(EXIT_FAILURE);
}
catch(exception &e)
{
  cerr<<"error: "<<e.what()<<endl;
  exit(EXIT_FAILURE);
}

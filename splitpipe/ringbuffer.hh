#ifndef RINGBUFFER_HH
#define RINGBUFFER_HH

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std; // sue me

class RingBuffer
{
public:
  RingBuffer(size_t size) : d_vector(size+1), d_publicSize(size)
  {
    d_rpos=d_wpos=0;
  }

  /*  |        r             w     |

      |        w             r     |
  */

  size_t room()
  {
    if(d_rpos == d_wpos)
      return d_publicSize;
    else if(d_rpos  < d_wpos)
      return d_rpos + d_publicSize - d_wpos;
    else //    if(d_wpos < d_rpos)
      return d_rpos - d_wpos - 1;
  }

  size_t available()
  {
    if(d_rpos == d_wpos)
      return 0;
    else if(d_rpos < d_wpos)
      return d_wpos - d_rpos;
    else  // d_wpos < d_rpos
      return d_wpos + d_vector.size() - d_rpos;
  }

  size_t contiguousRoom()
  {
    return d_vector.size() - d_wpos;
  }

  size_t contiguousAvailable()
  {
    if(d_wpos > d_rpos)
      return d_wpos - d_rpos;
    else
      return d_vector.size() - d_rpos;
  }


  void store(const char* block, size_t size)
  {
    if(!size)
      return; 

    if(size > room())
      throw runtime_error("was asked to store more data than there is room for!");

    size_t chunk=min(size, contiguousRoom());

    copy(block, block + chunk, d_vector.begin() + d_wpos);

    size-=chunk;
    d_wpos+=chunk;

    if(!size)
      return;

    d_wpos=0;
    copy(block+chunk, block+chunk+size, d_vector.begin());
    d_wpos+=size;
  }

  void get(const char **buf, size_t* bytes)
  {
    if(d_rpos == d_vector.size())
      d_rpos = 0; 

    *buf=&d_vector[d_rpos];
    *bytes=contiguousAvailable();
  }

  void advance(size_t bytes)
  {
    d_rpos+=bytes;
    if(d_rpos > d_vector.size())
      d_rpos -= d_vector.size();
  }

private:
  vector<char> d_vector;
  size_t d_publicSize;
  size_t d_rpos;
  size_t d_wpos;
};

#endif

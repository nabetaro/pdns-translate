#include <string>
#include <iostream>
#include "md5.hh"
#include "misc.hh"

using namespace std;

char correct1[]="53dc9b8e98d1716e 6f4c810b2208d994";
char correct2[]="6c7ba9c5a141421e 1c03cb9807c97c74";

int main()
{
  MD5Summer md5;
  md5.feed("this is a string in one part");
  
  string hex=makeHexDump(md5.get());
  cerr<<"hash of 'this is a string in one part' is '"<<hex<<"'"<<endl;
  cerr<<"Should be: '"<<correct1<<"'"<<endl;
  if(strcmp(hex.c_str(), correct1)) {
    cerr<<" *** FAILURE ***";
    exit(EXIT_FAILURE);
  }

  MD5Summer md5a;
  md5a.feed("this is a ");
  md5a.feed("string in one part");

  hex=makeHexDump(md5a.get());
  cerr<<"hash of 'this is a string in one part', fed in two parts, is '"<<hex<<"'"<<endl;

  cerr<<"Should be: '"<<correct1<<"'"<<endl;
  if(strcmp(hex.c_str(), correct1)) {
    cerr<<" *** FAILURE ***";
    exit(EXIT_FAILURE);
  }
  

  MD5Summer md5b;
  md5b.feed("something else");
  hex=makeHexDump(md5b.get());
  cerr<<"hash of 'something else' is '"<<hex<<"'"<<endl;

  cerr<<"Should be: '"<<correct2<<"'"<<endl;
  if(strcmp(hex.c_str(), correct2)) {
    cerr<<" *** FAILURE ***";
    exit(EXIT_FAILURE);
  }

  cerr<<" *** SUCCESS ***"<<endl;
  exit(EXIT_SUCCESS);

}

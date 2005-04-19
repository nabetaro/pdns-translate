#include <libgen.h>
#include "misc.hh"

int main(int argc, char** argv)
{
  char *base=basename(strdup(argv[0]));
  if(!strcmp(base,"splitpipe"))
    return SplitpipeMain(argc, argv);
  else
    return JoinpipeMain(argc, argv);
}

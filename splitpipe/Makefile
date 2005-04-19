VERSION=0.2-svn
export VERSION

OUTPUTDIR=splitpipe-$(VERSION)
CXXFLAGS=-Wall -g -O2 -DVERSION=\"$(VERSION)\"
PREFIX?=/usr/local/
BINDIR?=$(DESTDIR)/$(PREFIX)/bin/


all: splitpipe joinpipe

# output of g++ -MM *.cc
joinpipe.o: joinpipe.cc misc.hh md5.hh
md5.o: md5.cc md5.hh
misc.o: misc.cc misc.hh
splitpipe.o: splitpipe.cc misc.hh ringbuffer.hh md5.hh


clean:
	rm -rf *~ *.o splitpipe 
	cd tests; make clean
	cd doc; make clean

common_OBJECTS=misc.o md5.o
splitpipe_OBJECTS=splitpipe.o joinpipe.o main.o $(common_OBJECTS)

splitpipe: $(splitpipe_OBJECTS)
	g++ $(splitpipe_OBJECTS) -o $@

joinpipe: splitpipe
	ln -f splitpipe joinpipe

check: all
	cd tests; make

dist:
	rm -rf $(OUTPUTDIR)
	mkdir -p $(OUTPUTDIR)/tests
	cp Makefile PLAN *.cc *.hh COPYING TODO README HACKING cdrecord-script $(OUTPUTDIR)
	cp tests/{*.cc,Makefile} $(OUTPUTDIR)/tests
	cd doc; make dist
	tar cvzf $(OUTPUTDIR).tar.gz $(OUTPUTDIR)/
	rm -rf $(OUTPUTDIR)

install:
	install -s splitpipe $(BINDIR)
	ln -f $(BINDIR)/splitpipe $(BINDIR)/joinpipe

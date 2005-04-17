VERSION=0.2
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

common_OBJECTS=misc.o md5.o
splitpipe_OBJECTS=splitpipe.o $(common_OBJECTS)
joinpipe_OBJECTS=joinpipe.o $(common_OBJECTS)

splitpipe: $(splitpipe_OBJECTS)
	g++ $(splitpipe_OBJECTS) -o $@

joinpipe: $(joinpipe_OBJECTS)
	g++ $(joinpipe_OBJECTS) -o $@


check: all
	cd tests; make

dist:
	rm -rf $(OUTPUTDIR)
	mkdir -p $(OUTPUTDIR)/tests
	cp Makefile PLAN *.cc *.hh COPYING TODO README HACKING cdrecord-script $(OUTPUTDIR)
	cp tests/{*.cc,Makefile} $(OUTPUTDIR)/tests
	tar cvzf $(OUTPUTDIR).tar.gz $(OUTPUTDIR)/
	rm -rf $(OUTPUTDIR)

install:
	install -s splitpipe $(BINDIR)
	install -s joinpipe $(BINDIR)

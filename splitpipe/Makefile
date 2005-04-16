CXXFLAGS=-Wall -g -O2
VERSION=0.1
OUTPUTDIR=splitpipe-$(VERSION)

all: splitpipe joinpipe

clean:
	rm -rf *~ *.o splitpipe 

common_OBJECTS=misc.o md5.o
splitpipe_OBJECTS=splitpipe.o $(common_OBJECTS)
joinpipe_OBJECTS=joinpipe.o $(common_OBJECTS)

splitpipe: $(splitpipe_OBJECTS)
	g++ $(splitpipe_OBJECTS) -o $@

joinpipe: $(joinpipe_OBJECTS)
	g++ $(joinpipe_OBJECTS) -o $@


check: 
	cd tests; make

dist:
	rm -rf $(OUTPUTDIR)
	mkdir -p $(OUTPUTDIR)/tests
	cp Makefile PLAN *.cc *.hh COPYING TODO README HACKING cdrecord-script $(OUTPUTDIR)
	cp tests/{*.cc,Makefile} $(OUTPUTDIR)/tests
	tar cvzf $(OUTPUTDIR).tar.gz $(OUTPUTDIR)/
	rm -rf $(OUTPUTDIR)

CXXFLAGS=-Wall -g -O2
VERSION=0.1
OUTPUTDIR=splitpipe-$(VERSION)

all: splitpipe

clean:
	rm -rf *~ *.o splitpipe 

splitpipe_OBJECTS=splitpipe.o

splitpipe: $(splitpipe_OBJECTS)
	g++ $(splitpipe_OBJECTS) -o $@

check: 
	cd tests; make

dist:
	rm -rf $(OUTPUTDIR)
	mkdir -p $(OUTPUTDIR)/tests
	cp Makefile PLAN *.cc *.hh COPYING TODO README HACKING cdrecord-script $(OUTPUTDIR)
	cp tests/{*.cc,Makefile} $(OUTPUTDIR)/tests
	tar cvzf $(OUTPUTDIR).tar.gz $(OUTPUTDIR)/
	rm -rf $(OUTPUTDIR)
	
	
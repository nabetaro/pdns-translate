CXXFLAGS=-Wall -g -O2

all: splitpipe

splitpipe_OBJECTS=main.o

splitpipe: $(splitpipe_OBJECTS)
	g++ $(splitpipe_OBJECTS) -o $@


check: 
	cd tests; make

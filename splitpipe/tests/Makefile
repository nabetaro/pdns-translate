CXXFLAGS:=$(CXXFLAGS) -I../

all: programs run

run:
	./ringbuffer

programs: ringbuffer

ringbuffer: ringbuffer.o
	g++ ringbuffer.o -o ringbuffer
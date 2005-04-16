CXXFLAGS:=$(CXXFLAGS) -I../

all: programs run

CHECK_PROGRAMS=ringbuffer md5test

clean:
	rm -f *.o *~ $(CHECK_PROGRAMS)

run:
	./ringbuffer
	./md5test

programs: $(CHECK_PROGRAMS)

ringbuffer: ringbuffer.o
	g++ ringbuffer.o -o ringbuffer

md5test: md5test.o 
	g++ md5test.o -o md5test ../md5.o ../misc.o
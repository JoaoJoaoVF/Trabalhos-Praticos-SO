LOGFLAGS=-DUVMLOG -DMMULOG
CFLAGS=-g -Wall -Isrc -std=gnu99

all:
	
clean:
	rm -f *.o *.a
	rm -f img
	rm -f gcc.log
	rm -f test*.out
	rm -f test*.err
	rm -f test*
	pgrep --list-full mmu || true

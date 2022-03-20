#
# file:        Makefile - Lab4: CS5600 file system
#

CFLAGS = -ggdb3 -Wall -O0 -g
LDLIBS = -lcheck -lz -lm -lsubunit -lrt -lpthread -lfuse

all: lab4fuse test.img test2.img test1 test2

test1: test1.o fs5600.o misc.o
	$(CC) $^ $(LDLIBS) -o $@

test2: test2.o fs5600.o misc.o
	$(CC) $^ $(LDLIBS) -o $@

lab4fuse: misc.o fs5600.o lab4fuse.o
	$(CC) $^ $(LDLIBS) -o $@

testa: all
	./test1

testb: all
	./test2

# force test.img, test2.img to be rebuilt each time
.PHONY: test.img test2.img

test.img: 
	python2 gen-disk.py -q disk1.in test.img

test2.img: 
	python2 gen-disk.py -q disk2.in test2.img

clean: 
	rm -f *.o lab4fuse test.img test2.img test1 test2 diskfmt.pyc

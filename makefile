P=pb
CC=cc
TESTS=tests

OBJECTS=
CFLAGS=-I/usr/local/include -Iyajl/build/yajl-2.1.1/include -g -Wall -O0 -std=gnu11
LDLIBS=-L/usr/local/lib -lcurl -Lyajl/build/yajl-2.1.1/lib -lyajl_s
LDFLAGS=$(LDLIBS)

$(P): $(OBJECTS)

# $(TESTS).o: $(TESTS).c
# 	$(CC) $(CFLAGS) $(LDFLAGS) -o tests.o tests.c

$(TESTS):
	$(CC) `pkg-config --cflags glib-2.0` $(TESTS).c -o $(TESTS) `pkg-config --libs glib-2.0`

test: $(TESTS)
	./$(TESTS)

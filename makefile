P=pb
OBJECTS=
CFLAGS=-I/usr/local/include/glib-2.0 -I/usr/local/include -Iyajl/build/yajl-2.1.1/include -g -Wall -O0 -std=gnu99
LDLIBS=-lglib-2.0 -L/usr/local/lib -lcurl -Lyajl/build/yajl-2.1.1/lib -lyajl_s

$(P): $(OBJECTS)

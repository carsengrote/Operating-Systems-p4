all: libmfs.so server tester

libmfs.so: libmfs.c udp.c
	gcc -fPIC -g -c -Wall libmfs.c
	gcc -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o -lc udp.c

server: server.c udp.c
	gcc -o server server.c -Wall udp.c

tester: udp.c
	gcc -o tester tester.c -Wall -L. -lmfs udp.c

clean:
	rm -f libmfs.so
	rm -f server
	rm -f tester

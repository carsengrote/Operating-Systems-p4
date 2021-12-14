all: libmfs.so server

libmfs.so: libmfs.c udp.c
	gcc -fPIC -g -c -Wall libmfs.c
	gcc -fPIC -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o -lc udp.c

server: server.c udp.c
	gcc -o server server.c -Wall -Werror -g udp.c

client: client.o libmfs.so
	gcc -Wall -Werror -g -o client client.o -lmfs -L.

clean:
	rm -f libmfs.so libmfs.o
	rm -f server server.o
	rm -f client.o client
	

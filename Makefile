


server : server.o queue.o
	gcc -o server server.o queue.o


server.o : server.c
	gcc -c server.c -o server.o

queue.o : queue.c
	gcc -c queue.c -o queue.o


clean make:
	rm -f *.o



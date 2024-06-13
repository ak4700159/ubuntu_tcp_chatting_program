


server : server.o queue.o
	gcc -o app.out server.o queue.o


server.o : server.c queue.h
	gcc -c server.c

queue.o : queue.c queue.h
	gcc -c queue.c


clean make:
	rm -f *.o



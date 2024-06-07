#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdbool.h>
#define MAX_BUFFER_SIZE 1000
#define MAX_QUEUE_SIZE 20

typedef struct Queue{
	int rear;
	int front;
	char* data[MAX_QUEUE_SIZE];
}Queue;

void queue_init(Queue* q);
void push(Queue* q, char* msg);
char* pop(Queue* q);

bool isFully(Queue* q);
bool isEmpty(Queue* q);

#endif

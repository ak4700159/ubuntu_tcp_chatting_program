#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void queue_init(Queue* q){
	q->rear = 0;
	q->front = 0;
	for(int i = 0; i < MAX_QUEUE_SIZE; i++)
		q->data[i] = (char*)malloc(sizeof(char) * MAX_BUFFER_SIZE);
}

void push(Queue* q, char* msg){
	if(!isFully(q)){
		strcpy(q->data[q->rear], msg);
		q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
	}
}

char* pop(Queue* q){
	char* data;
	if(!isEmpty(q)){
		data = q->data[q->front];
		q->front = (q->front + 1) % MAX_QUEUE_SIZE;
	}
	return data;
}

bool isFully(Queue* q){
	return ( ((q->rear + 1) % MAX_BUFFER_SIZE)  == q->front );
}
bool isEmpty(Queue* q){
	return (q->rear == q->front);
}

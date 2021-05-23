#ifndef SVC_QUEUE
#define SVC_QUEUE

#include <stdlib.h>
#include "svc.h"
#include <stdio.h>
#include "structures.h"




struct Queue {
  int front;
  int rear;
  size_t size;
  size_t capacity;
  struct Commit** array;
};

struct Queue* createQueue(int capacity){
  struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
  queue->capacity = capacity;
  queue->front = 0;
  queue->size = 0;
  queue->rear = capacity - 1;
  queue->array = (struct Commit**)malloc(queue->capacity * sizeof(struct Commit*));

  return queue;
}

void freeQueue(struct Queue* queue){

  free(queue->array);
  free(queue);


}

int isFull(struct Queue* queue){

  return (queue->size == queue->capacity);

}


int isEmpty(struct Queue* queue) {

  return (queue->size == 0);

}

void enqueue(struct Queue* queue, struct Commit* commit){

  if(isFull(queue)){
    return;
  }

  queue->rear = (queue->rear + 1) % queue->capacity;
  queue->array[queue->rear] = commit;
  queue->size = queue->size + 1;

}

struct Commit* dequeue(struct Queue* queue){

  if(isEmpty(queue)){
    return NULL;
  }

  struct Commit* commit = queue->array[queue->front];
  queue->front = (queue->front + 1) % queue->capacity;
  queue->size = queue->size - 1;
  return commit;


}


#endif
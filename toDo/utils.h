#ifndef UTILS_
#define UTILS_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/queue.h>
#include <stdatomic.h>
#include <stdint.h>

#define SUCCESS 0
#define FAIL -1
// #define CACHESPACE 10240
// #define HASHSPACE 1024
// #define THREADS 1024
// #define BILLION  1000000000L
// atomic_int buffer_count;

#define CACHESPACE 8
#define HASHSPACE 90
#define THREADS 6
#define BILLION  1000000000L
atomic_int buffer_count;


pthread_spinlock_t LRU_lock;
pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;


typedef struct buffer_node {
    int value;
	struct buffer_node *prev, *next;
} buffer_node;

//defining root and ref root globally to use in other functions
buffer_node* root = NULL;
buffer_node* ref_node = NULL;

typedef struct QNode {
	struct QNode *prev, *next;
	unsigned pageNumber; 
} QNode;

// A Queue (A FIFO collection of Queue Nodes)
typedef struct Queue {
	unsigned count; // Number of filled frames
	unsigned numberOfFrames; // total number of frames
	QNode *front, *rear;
} Queue;

// A hash (Collection of pointers to Queue Nodes)
typedef struct Hash {
	int capacity; // how many pages can be there
	QNode** array; // an array of queue nodes
} Hash;

#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <stdint.h>
#include <math.h>

#define SUCCESS 0
#define FAIL -1
#define CACHESPACE 1024
#define HASHSPACE 10240
#define THREADS 1024

// #define CACHESPACE 4
// #define HASHSPACE 20
// #define THREADS 10

#define BILLION  1000000000L
atomic_int allocation_time_avg;
atomic_int total_allocations;
atomic_int access_time_avg;
atomic_int total_access;
atomic_int access_done_time_avg;

atomic_int buffer_count;
atomic_int node_tl;
atomic_int left_node_tl;
atomic_int right_node_tl;

pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;

typedef struct QNode {
	struct QNode *prev, *next;
	unsigned key;
	atomic_int ref_count; 
	pthread_spinlock_t node_lock;
	// add data ptr
} QNode; 

// A Queue (A FIFO collection of Queue Nodes)
typedef struct Queue {
	unsigned count; // Number of filled frames
	QNode *front, *rear;
} Queue;

// A hash (Collection of pointers to Queue Nodes)
typedef struct Hash {
	int capacity; // how many pages can be there
	QNode** array; // an array of queue nodes
} Hash;

// Function to add buffers to the memory pool
void add_buffer(QNode *ref_node);

// Function to create a memory pool
int buffer_pool();

// Function to put buffers back to the memory pool
void put_buffer(QNode* free_buffer);

// Function to get buffers from the memory pool 
QNode* get_buffer();

// Function to create the LRU cache
Queue* createQueue();

// Function to check if the cache is empty
int isQueueEmpty(Queue* queue);

// Function to create a hash table
Hash* createHash(int capacity);

// Utitlity function to remove the last entry in cache to allocate a new entry
// when there are no remaining buffers to use in the memory pool
QNode* evict(Queue* queue, unsigned key_passed);

//Utility function to allocate a buffer to an entry if not present in the hash table
QNode* allocate_node(Queue* queue, Hash* hash, unsigned key_passed, long tid);

// Utility function to access the last entry of the cache
int access_rear_node(Queue* queue, QNode* req_page, long tid);

// Utility function to access the first entry of the cache
int access_front_node(Queue* queue, QNode* req_page, long tid);

// Utility function to access an entry of the cache which is not first or last
int access_middle_node(Queue* queue, QNode* req_page, long tid);

// Utility function to access an entry of the cache
int access_node(Queue* queue, QNode* req_page, unsigned key, long tid);

// Utility function to free the first entry of the cache
void free_front_node(Queue* queue, QNode* req_page, unsigned key);

// Utility function to free the last entry of the cache
void free_rear_node(Queue* queue, QNode* req_page, unsigned key);

// Utility function to free an entry of the cache which is not first or last
void free_middle_node(Queue* queue, QNode* req_page, unsigned key);

// Utility function to free an entry of the cache
void free_node(Queue* queue, QNode* req_page, unsigned key);

// Utility function to indicate an entry's access is completed
void access_done(Queue* queue, Hash* hash, QNode* req_page, long tid);

// Function to print the available buffers in the memory pool
void print_buffer_pool();
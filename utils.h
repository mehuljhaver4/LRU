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

#define SUCCESS 0
#define FAIL -1
#define CACHESPACE 512
#define HASHSPACE 1024
#define THREADS 1024
#define BILLION  1000000000L
atomic_int buffer_count;
int task_count = 512; //task_count and CACHESPACE should be equal for this example.

pthread_spinlock_t LRU_lock;
pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;

// Queue* q;
// Hash* hash;
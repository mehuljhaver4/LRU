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
#define CACHESPACE 10240
#define HASHSPACE 1024
#define THREADS 1024
#define BILLION  1000000000L
atomic_int buffer_count;

// #define CACHESPACE 4
// #define HASHSPACE 10
// #define THREADS 6
// #define BILLION  1000000000L
// atomic_int buffer_count;


pthread_spinlock_t LRU_lock;
pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;

void *buffer_pool_ptr = NULL; 

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

typedef struct buffer_node {
    int value;
	struct buffer_node *prev, *next;
} buffer_node;

//defining root and ref root globally to use in other functions
buffer_node* root = NULL;
buffer_node* ref_node = NULL;

// Append buffers to the memory pool
buffer_node* append_buffer(buffer_node **ref_node, buffer_node **root, int size)
{
    buffer_node* newNode = *ref_node;
    buffer_node* lastNode = root;
    newNode->next = NULL;

    while(lastNode->next != NULL) {
        lastNode = lastNode->next;
    }

    lastNode->next = newNode;
    newNode->prev = lastNode;
    return newNode;
}


// create a memory pool to append buffers for later use
int buffer_pool() {
    buffer_pool_ptr = calloc(CACHESPACE ,sizeof(buffer_node));
    printf("\n Memory pool created\n");
    root = buffer_pool_ptr;
    
	if(!buffer_pool_ptr) {
        printf("\nBuffer memory allocation failed\n");
        return FAIL;
    }
    // using a ref node to update the pointer after incrementing
    ref_node = root; 

    for(int i = 0; i< CACHESPACE; i++) {
        append_buffer(&ref_node, &root, sizeof(buffer_node));
		buffer_count++;
        ref_node++;
    }
    return SUCCESS;
}



// A utility function to delete a frame from queue
QNode* deQueue(Queue* queue)
{
	// Change rear and remove the previous rear
	QNode* temp = queue->rear;
	queue->rear = queue->rear->prev;

	if (queue->rear)
		queue->rear->next = NULL;
	// decrement the number of full frames by 1
	queue->count--;
    return temp;
}

// buffer_node* free_node(Queue* queue) {
// 	printf("wjhgfuwef");
// 	buffer_node* lastNode = root;
// 	printf("aaaaaa");
// 	QNode* freed = deQueue(queue);
// 	printf("bbbbbb");

// 	freed->next = NULL;
// 	while(lastNode->next != NULL) {
// 		lastNode = lastNode->next;
// 	}

// 	lastNode->next = freed;
// 	freed->prev = lastNode;
// 	return freed;
// }

// A utility function to get a buffer from buffer pool
QNode* get_buffer(buffer_node* root, Queue* queue, Hash* hash)
{
    buffer_node* lastNode = root;
    QNode* temp = NULL;

	//if only one node present.
	if (lastNode->next == NULL) {
        temp = (QNode *) lastNode;
        temp->prev = NULL;
		buffer_count--;
        return temp;
    }

    // when multiple nodes are preset get the last buffer
    // make changes to the second last buffer
    while (lastNode->next != NULL) {
        if (lastNode->next->next == NULL) {
            
			temp = (QNode *)lastNode->next; //temp gets the last node
            temp->prev = NULL; // prev link becomes null
            lastNode->next = NULL;

			buffer_count--;
            return temp;
        }
        lastNode = lastNode->next;
    }  
}



QNode* newQNode(Queue* queue, Hash* hash, unsigned pageNumber)
{   
    QNode* newBuff = NULL;
    if (buffer_count != 0 ) { 
        //get_buffer from buffer pool
		pthread_spin_lock(&buffer_lock);
        newBuff = get_buffer(root, queue, hash);
		pthread_spin_unlock(&buffer_lock);

        newBuff->next = root->next;
        newBuff->prev = NULL;
        newBuff->pageNumber = pageNumber;
        // printf("-> Address of new node: %p // content: %d \n",newBuff,newBuff->pageNumber);
    }

	else{ 
			
		pthread_spin_lock(&LRU_lock);
		hash->array[queue->rear->pageNumber] = NULL;
		newBuff = deQueue(queue);
		newBuff->pageNumber = pageNumber;
		pthread_spin_unlock(&LRU_lock);
		// printf("-> Address of new node due to no new buffers in pool is: %p // content: %d \n",newBuff,newBuff->pageNumber);
	}
	return newBuff;
}

Queue* createQueue(int numberOfFrames)
{
	Queue* queue = (Queue*)malloc(task_count*sizeof(Queue));

	// The queue is empty
	queue->count = 0;
	queue->front = queue->rear = NULL;

	// Number of frames that can be stored in memory
	queue->numberOfFrames = numberOfFrames;

	return queue;
}

// A utility function to create an empty Hash of given capacity
Hash* createHash(int capacity)
{
	// Allocate memory for hash
	Hash* hash = (Hash*)malloc(sizeof(Hash));
	hash->capacity = capacity;

	// Create an array of pointers for referring queue nodes
	hash->array = (QNode**)malloc(hash->capacity * sizeof(QNode*));

	// Initialize all hash entries as empty
	for (int i = 0; i < hash->capacity; ++i)
		hash->array[i] = NULL;

	return hash;
}


// A utility function to check if queue is empty
int isQueueEmpty(Queue* queue)
{
	return queue->rear == NULL;
}


QNode* allocate_node(Queue* queue, Hash* hash, unsigned pageNumber) {
		
		// record the time taken before applying the lock
		struct timespec start_1, stop_1, stop_2, stop_3;
        uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
		
		// Create a new node with given page number,
		// And add the new node to the front of queue
		QNode* temp = newQNode(queue, hash, pageNumber);

		if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
			perror( "clock gettime" );
			return EXIT_FAILURE;
		}

		pthread_spin_lock(&LRU_lock);

        if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
            perror( "clock gettime" );
            return EXIT_FAILURE;
        }

		temp->next = queue->front;
		temp->pageNumber = pageNumber;

		// If queue is empty, change both front and rear pointers
		if (isQueueEmpty(queue))
			queue->rear = queue->front = temp;
		else // Else change the front
		{
			queue->front->prev = temp;
			queue->front = temp;
		}
		queue->count++;

		if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
				return EXIT_FAILURE;
		}
		pthread_spin_unlock(&LRU_lock);
		
		if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
				return EXIT_FAILURE;
		}

		accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
        accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
        accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
        accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

		printf("> Time taken b/w lock to *ALLOCATE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",queue->front->pageNumber, accum_1, accum_2, accum_3, accum_4);
		return temp;
}

void access_node(Queue* queue, Hash* hash, QNode* reqPage) { //change parameters
		
		// record the time taken before applying the lock
		struct timespec start_1, stop_1, stop_2, stop_3;
        uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

		if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
			perror( "clock gettime" );
			return EXIT_FAILURE;
		}

        pthread_spin_lock(&LRU_lock);

        if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
            perror( "clock gettime" );
            return EXIT_FAILURE;
        }
		
		// if the requested pagenumber is already infront of queue do nothing
	    if (reqPage == queue->front) {}
			// printf(" *** Page number: %d already infront of the cache \n",reqPage->pageNumber);
		else {
			// Unlink rquested page from its current location in queue
			reqPage->prev->next = reqPage->next;
			if (reqPage->next)
				reqPage->next->prev = reqPage->prev;

			// If the requested page is rear, then change rear
			// as this node will be moved to front
			if (reqPage == queue->rear) {
				queue->rear = reqPage->prev;
				queue->rear->next = NULL;
			}
			// Put the requested page before current front
			reqPage->next = queue->front;
			reqPage->prev = NULL;

			// Change prev of current front
			reqPage->next->prev = reqPage;

			// Change front to the requested page
			queue->front = reqPage;
			// printf("\n *** PageNumber already present in cache: %p // content: %d \n",reqPage,queue->front->pageNumber );
		}

		if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
				return EXIT_FAILURE;
		}
        pthread_spin_unlock(&LRU_lock);

        if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
                perror( "clock gettime" );
                return EXIT_FAILURE;
        }

		accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
        accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
        accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
        accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

		printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",queue->front->pageNumber, accum_1, accum_2, accum_3, accum_4);
}
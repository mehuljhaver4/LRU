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
#include <errno.h>

#define SUCCESS 0
#define FAIL -1
// #define CACHESPACE 10240
// #define HASHSPACE 1024
// #define THREADS 1024
// #define BILLION  1000000000L
// atomic_int buffer_count;

#define CACHESPACE 20
#define HASHSPACE 80
#define THREADS 6
#define BILLION  1000000000L
atomic_int buffer_count;

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

// Memory pool buffer nodes
typedef struct buffer_node {
    int value;
	struct buffer_node *prev, *next;
} buffer_node;

//defining root and ref root globally to use in other functions
buffer_node* root = NULL;
buffer_node* ref_node = NULL;


// Append buffers to the memory pool
buffer_node* append_buffer(buffer_node **ref_node, buffer_node **root)
{
    buffer_node* newNode = *ref_node;
    buffer_node* lastNode = (buffer_node *)root;
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
        append_buffer(&ref_node, &root);
		buffer_count++;
        ref_node++;
    }
    return SUCCESS;
}

// A utility function to add the buffer back to the buffer pool
// after deleting a pageNumber from cache.
buffer_node* put_buffer(buffer_node **root, buffer_node* free_buffer) {
	pthread_spin_lock(&buffer_lock);

	buffer_node* new_buffer = free_buffer; //might be a porblem here
	buffer_node* lastNode = *root;

	while (lastNode->next != NULL) {
		lastNode = lastNode->next;
	}
	lastNode->next = new_buffer;
	new_buffer->prev = lastNode;
	new_buffer->next = NULL;
	new_buffer->value = 0; // need to decide what to do with value.
	buffer_count++;

	pthread_spin_unlock(&buffer_lock);
	return new_buffer;
}

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
			printf("Temp = %p\n", temp);
            return temp;
        }
        lastNode = lastNode->next;
    }
}

// A utility function to access the last frame of queue
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

// A utility function to get a new node
QNode* newQNode(Queue* queue, Hash* hash, unsigned pageNumber)
{   
    QNode* newBuff = NULL;
    if (buffer_count != 0 ) { 
        //get_buffer from buffer pool
		pthread_spin_lock(&buffer_lock);

        newBuff = get_buffer(root, queue, hash);
		
		pthread_spin_unlock(&buffer_lock);

        newBuff->next = (QNode *)root->next;
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

// A utility function to create a queue
Queue* createQueue(int numberOfFrames)
{
	Queue* queue = (Queue*)malloc(CACHESPACE*sizeof(Queue));

	// The queue is empty
	queue->count = 0;
	queue->front = queue->rear = NULL;

	// Number of frames that can be stored in memory
	queue->numberOfFrames = numberOfFrames;

	return queue;
}

// A utility function to check if queue is empty
int isQueueEmpty(Queue* queue)
{
	return queue->rear == NULL;
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


// A utility function to allocate pageNumber to the cache
QNode* allocate_node(Queue* queue, Hash* hash, unsigned pageNumber) {
		
	// record the time taken before applying the lock
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
	
	// Create a new node with given page number,
	// And add the new node to the front of queue
	QNode* temp = newQNode(queue, hash, pageNumber);

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	pthread_spin_lock(&LRU_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
			// return EXIT_FAILURE;
	}
	pthread_spin_unlock(&LRU_lock);
	
	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
			perror( "clock gettime" );
			// return EXIT_FAILURE;
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

	printf("> Time taken b/w lock to *ALLOCATE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",queue->front->pageNumber, accum_1, accum_2, accum_3, accum_4);
	return temp;
}

// A utility function to access a pageNumber present in cache
void access_node(Queue* queue, Hash* hash, QNode* reqPage) { //change parameters
		
	// record the time taken before applying the lock
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	pthread_spin_lock(&LRU_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
			// return EXIT_FAILURE;
	}
	pthread_spin_unlock(&LRU_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
			perror( "clock gettime" );
			// return EXIT_FAILURE;
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",queue->front->pageNumber, accum_1, accum_2, accum_3, accum_4);
}

// A utility function to delete a pageNumber from cache
QNode* free_node(Queue* queue, QNode* reqPage) {

	// record the time taken before applying the lock
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	pthread_spin_lock(&LRU_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	//when cache is empty or reqPage is NULL
	if (queue->front == NULL || reqPage == NULL) {
		printf("*** OPERATION NOT POSSIBLE, EXITING! ***\n"); //throw an error
		exit(EXIT_FAILURE); // exit for now. decide later what to do.
	}
	else {
		// if reqPage is the root
		if (queue->front == reqPage)
			queue->front = reqPage->next;
		
		// Change next only if node to be deleted is NOT the last node
		if (reqPage->next != NULL)
			reqPage->next->prev = reqPage->prev;
	
		// Change prev only if node to be deleted is NOT the first node
		if (reqPage->prev != NULL)
			reqPage->prev->next = reqPage->next;
		
		printf("\nPage requested to be deleted is at %p and is %d \n",reqPage,reqPage->pageNumber);
		if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
		}
		pthread_spin_unlock(&LRU_lock);
		
		if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
		}
		accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
        accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
        accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
        accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

		printf("> Time taken b/w lock to *FREE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",reqPage->pageNumber, accum_1, accum_2, accum_3, accum_4);
		return reqPage;
	}
}
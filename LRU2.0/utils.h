#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <stdint.h>

#define SUCCESS 0
#define FAIL -1
// #define CACHESPACE 10240
// #define HASHSPACE 1024
// #define THREADS 1024
// #define BILLION  1000000000L
// atomic_int buffer_count;

#define CACHESPACE 10
#define HASHSPACE 20
#define THREADS 6
#define BILLION  1000000000L
atomic_int buffer_count;

atomic_int buff_tl;
atomic_int hash_tl;
atomic_int node_tl;
atomic_int left_node_tl;
atomic_int right_node_tl;

pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;

void *buffer_pool_ptr = NULL; 

typedef struct QNode {
	struct QNode *prev, *next;
	unsigned key;
	atomic_int ref_count; 
	pthread_spinlock_t node_lock;  //initialise  in getbuffer
	// add data ptr
} QNode; 

// A Queue (A FIFO collection of Queue Nodes)
typedef struct Queue {
	unsigned count; // Number of filled frames
	QNode *front;
	QNode *rear;
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

	buffer_node* new_buffer = free_buffer;
	buffer_node* lastNode = *root;

	while (lastNode->next != NULL)
		lastNode = lastNode->next;
	
	lastNode->next = new_buffer;
	new_buffer->prev = lastNode;
	new_buffer->next = NULL;
	new_buffer->value = 0; // need to decide what to do with value. 
	buffer_count++;

	pthread_spin_unlock(&buffer_lock);
	return new_buffer;
}

// A utility function to get a buffer from buffer pool
QNode* get_buffer(buffer_node* root) {	
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
			// initiliase node lock here
			if (pthread_spin_init(&temp->node_lock, 0) != 0) {
				printf("\n spin node lock has failed\n");
				return NULL;
			}				
            return temp;
        }
        lastNode = lastNode->next;
    }
}


// A utility function to create a queue
Queue* createQueue() {
	Queue* queue = (Queue*)malloc(CACHESPACE*sizeof(Queue));

	// The queue is empty
	queue->count = 0;
	queue->front = queue->rear = NULL;
	return queue;
}

// A utility function to check if queue is empty
int isQueueEmpty(Queue* queue) {
	return queue->rear == NULL;
}

// A utility function to create an empty Hash of given capacity
Hash* createHash(int capacity) {
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

QNode* evict(Queue* queue, unsigned key_passed) {
	QNode* lastNode = queue->rear;
	QNode* leftNode = lastNode->prev;

	int retry = 1;  // use boolean variable
	//Take try lock on the last node to be evicted.

	while (retry != 0) {
		QNode* lastNode = queue->rear;
		QNode* leftNode = lastNode->prev;
		node_tl = pthread_spin_trylock(&lastNode->node_lock);
		left_node_tl = pthread_spin_trylock(&leftNode->node_lock);

		if ((node_tl == 0) && (left_node_tl == 0) && (lastNode->ref_count == 0)) {
			//check if the last node is still the same last node of the list
			// same for the left node.
			if((lastNode == queue->rear) && (leftNode == queue->rear->prev)) {
				retry = 0;
				printf("Got both the locks in evict \n");
			}
			else {
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&lastNode->node_lock);
			}
		}
		else {
			if (left_node_tl == 0) {
				pthread_spin_unlock(&leftNode->node_lock);
			}
			else if(node_tl == 0) {
				pthread_spin_unlock(&lastNode->node_lock);
			}
		}
	}

	lastNode->ref_count++;
	lastNode->key = key_passed;
	lastNode->next = NULL;
	lastNode->prev = NULL;
	
	//Changing the last node of the LRU list
	queue->rear = queue->rear->prev;
	if (queue->rear) {
		queue->rear->next = NULL;
	}
	queue->count--;

	pthread_spin_unlock(&leftNode->node_lock);
	pthread_spin_unlock(&lastNode->node_lock);
	return lastNode;
}

// A utility function to allocate pageNumber to the cache
QNode* allocate_node(Queue* queue, Hash* hash, unsigned key_passed, long tid) {
	// Create a new node with given ke_passed,
	QNode* newBuff = NULL;
	//get buffer from buffer pool
	pthread_spin_lock(&buffer_lock);

	if (buffer_count != 0 ) { 
        newBuff = get_buffer(root);	
		newBuff->next = NULL;
		newBuff->prev = NULL;
		newBuff->key = key_passed;
		printf("new buffer address: %p key: %d, ThreadID: %ld\n",newBuff, newBuff->key, tid);
		pthread_spin_unlock(&buffer_lock);
		return newBuff;
	}
	pthread_spin_unlock(&buffer_lock);
	newBuff = evict(queue, key_passed);
	return newBuff;
}

// A utility function to access a pageNumber present in cache
int access_node(Queue* queue, Hash* hash, QNode* reqPage, unsigned key, long tid) { //change parameters

	int retry = 1;
	
	// record the time taken before applying the lock
	struct timespec start_1, stop_1, stop_2, stop_3, stop_4;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}

	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&reqPage->node_lock);
		if (node_tl == 0)
			retry = 0;
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}

	// check for key match - failure should return appropriate error code and unlock	
	if (key == reqPage->key){  
		
		// if ref_count is 0 then node is present in the LRU list
		if (reqPage->ref_count == 0) {
			reqPage->ref_count++;

			// if there is only one node in the LRU list.
			if ((queue->front == reqPage) && (queue->rear == reqPage)) {
				queue->front = NULL;
				queue->rear = NULL;
				reqPage->next = NULL;
				reqPage->prev = NULL;
				queue->count--;

				pthread_spin_unlock(&reqPage->node_lock);
				return SUCCESS;
			}			
			// check the position of the node
			// if last then two locks. node and left
			else if (reqPage == queue->rear) {
				
				QNode* leftNode = reqPage->prev;
				printf("last node condition: left node: %p, ThreadID: %ld\n", leftNode, tid);

				retry = 1;
				while (retry != 0) {
					QNode* leftNode = reqPage->prev;
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);
					if (left_node_tl == 0) {
						if (reqPage == queue->rear) {
							retry = 0;
							printf("Got the leftnode lock in access \n");
						}
						else {
							pthread_spin_unlock(&leftNode->node_lock);
						}
					}
				}

				if( clock_gettime( CLOCK_REALTIME, &stop_4) == -1 ) {
					perror( "clock gettime" );
				}
				
				//removing from LRU list
				queue->rear = queue->rear->prev;
				reqPage->next = NULL;
				reqPage->prev = NULL;

				if (queue->rear) {
					queue->rear->next = NULL;
				}
				// decrement the number of full frames by 1
				queue->count--;

				if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
					perror( "clock gettime" );
				}
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);

				if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
					perror( "clock gettime" );
				}

				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_4.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_4.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",reqPage->key, accum_1, accum_2, accum_3, accum_4);
				return SUCCESS;
			}

			// if root then two locks. node and right
			else if (reqPage == queue->front) {
				printf("head node condition, ThreadID: %ld\n",tid);
				QNode* rightNode = reqPage->next;
				retry = 1;

				// taking the right node lock
				while (retry != 0) {
					QNode* rightNode = reqPage->next;
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);
					if (right_node_tl == 0) {
						if (reqPage == queue->front) {
							retry = 0;
							printf("Got the rightNode lock in access \n");
						}
						else {
							pthread_spin_unlock(&rightNode->node_lock);
						}
					}
				}

				if( clock_gettime( CLOCK_REALTIME, &stop_4) == -1 ) {
					perror( "clock gettime" );
				}	

				queue->front = reqPage->next;
				reqPage->prev = NULL;
				reqPage->next = NULL;
				queue->count --;

				if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
					perror( "clock gettime" );
				}

				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);

				if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
					perror( "clock gettime" );
				}
				
				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_4.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_4.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",reqPage->key, accum_1, accum_2, accum_3, accum_4);
				return SUCCESS;
			}

			// Not root and rear, three locks.  Node left right
			else if ((reqPage->next != NULL) && (reqPage->prev != NULL)) {

				QNode* leftNode = reqPage->prev;
				QNode* rightNode = reqPage->next;

				printf("Middle node condition: left node: %p, ThreadID: %ld\n", leftNode, tid);
				printf("reqPage: %p key: %d\n",reqPage, reqPage->key);
				printf("reqPage->next: %p key: %d\n",reqPage->next, reqPage->next->key);

				retry = 1;
				while (retry != 0) {
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if ((left_node_tl == 0) && (right_node_tl == 0)) {
						retry = 0;
						printf("Got both the locks in access \n");
					}
					else {
						if(left_node_tl == 0) {
							pthread_spin_unlock(&leftNode->node_lock);
						}
						else if (right_node_tl == 0) {
							pthread_spin_unlock(&rightNode->node_lock);
						}
					}
				}

				if( clock_gettime( CLOCK_REALTIME, &stop_4) == -1 ) {
					perror( "clock gettime" );
				}

				leftNode->next = rightNode;
				rightNode->prev = leftNode;
				reqPage->next = NULL;
				reqPage->prev = NULL;
				queue->count--;
				
				if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
					perror( "clock gettime" );
				}

				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);

				if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
					perror( "clock gettime" );
				}

				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_4.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_4.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",reqPage->key, accum_1, accum_2, accum_3, accum_4);
				return SUCCESS;
			}
			else {
				pthread_spin_unlock(&reqPage->node_lock);
				return SUCCESS;
			}
		}
		else if (reqPage->ref_count > 0) { // node is being used by other thread
			reqPage->ref_count++;
			pthread_spin_unlock(&reqPage->node_lock);
			return SUCCESS;
		}
	}
	else {
		printf("\nERROR... KEYS NOT MATCHING CALLING ALLOCATE \n");
		pthread_spin_unlock(&reqPage->node_lock);
		return FAIL;
	}
}

// A utility function to delete a pageNumber from cache
void free_node(Queue* queue, QNode* reqPage, unsigned key, long tid) {   // remove from hash first??

	int retry = 1;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&reqPage->node_lock);
		if (node_tl == 0) {
			retry = 0;
		}
	}

	if (key == reqPage->key){ 
		if (reqPage->ref_count > 0) {  // cannot free as node is being used by other thread
			pthread_spin_unlock(&reqPage->node_lock);
			printf("NODE IS IN USE!!! \n");  // return error statement
		}
		else if (reqPage->ref_count == 0) {
			//clear node key
			reqPage->key = (intptr_t) NULL;

			// if reqPage is the root, take right node lock
			if (queue->front == reqPage) { 
				QNode* rightNode = reqPage->next;
				retry = 1;
				while (retry != 0) {
					QNode* rightNode = reqPage->next;
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);
					if (right_node_tl == 0) {
						if (reqPage == queue->front) {
							retry = 0;
						}
						else {
							pthread_spin_unlock(&rightNode->node_lock);
						}
					}
				}
				
				// change the front of the LRU list and put it back to free pool
				queue->front = reqPage->next;
				put_buffer(&root, (buffer_node *)reqPage);

				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);
				return;
			}

			// if reqPage is last node
			else if (queue->rear == reqPage) {
				QNode* leftNode = reqPage->prev;
				int retry = 1;
				//Take try lock on the last node to be evicted.
				while (retry != 0) {
					QNode* leftNode = reqPage->prev;
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);

					if (left_node_tl == 0) {
						//check if the last node is still the same last node of the list
						// same for the left node.
						if((reqPage == queue->rear) && (leftNode == queue->rear->prev)) {
							retry = 0;
						}
						else {
							pthread_spin_unlock(&leftNode->node_lock);
						}
					}
				}
					//Changing the last node of the LRU list
					queue->rear = queue->rear->prev;
					if (queue->rear) {
						queue->rear->next = NULL;
					}
					queue->count--; 

					put_buffer(&root, (buffer_node *)reqPage);

					pthread_spin_unlock(&leftNode->node_lock);
					pthread_spin_unlock(&reqPage->node_lock);
					return;			
			}

			else {
				QNode* leftNode = reqPage->prev;
				QNode* rightNode = reqPage->next;
				retry = 1;

				while (retry != 0) {
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if ((left_node_tl == 0) && (right_node_tl == 0)) {
						retry = 0;
					}
					
					else {
						if(left_node_tl == 0) {
							pthread_spin_unlock(&leftNode->node_lock);
						}
						else if (right_node_tl == 0) {
							pthread_spin_unlock(&rightNode->node_lock);
						}
					}
				}
				reqPage->next->prev = reqPage->prev;
				reqPage->prev->next = reqPage->next;

				put_buffer(&root, (buffer_node *)reqPage);

				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);
				return;
			}
		}
	}
	else {
		printf("ERROR.. PAGE NUMBER NOT PRESENT IN CACHE \n");
		pthread_spin_unlock(&reqPage->node_lock);
		return;
	}	
}

void access_done(Queue* queue, Hash* hash, QNode* reqPage, long tid) {

	int retry = 1;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&reqPage->node_lock);
		if (node_tl == 0) { 
			retry = 0;
		}
	}
	printf("Before assert inside ACCESS_DONE key: %d, ref_count: %d, ThreadID: %ld\n",reqPage->key, reqPage->ref_count, tid);
	assert(reqPage->ref_count > 0);
	reqPage->ref_count--;
	printf("After assert inside ACCESS_DONE key: %d, ref_count: %d, ThreadID: %ld\n",reqPage->key, reqPage->ref_count, tid);
	
	if (reqPage->ref_count == 0) {
		// If queue is empty, change both front and rear pointers
		if (isQueueEmpty(queue)) {
			queue->rear = queue->front = reqPage;
			reqPage->next = reqPage->prev = NULL;
			queue->count++;
			pthread_spin_unlock(&reqPage->node_lock);
			printf("First node added to LRU list. Key: %d\n",reqPage->key);
			return;
		}
		else {
			// change the front, take a lock on the present first node
			QNode* rightNode = queue->front;
			retry = 1;
			while (retry != 0) {
				rightNode = queue->front;
				right_node_tl = pthread_spin_trylock(&rightNode->node_lock);
				if ((right_node_tl == 0) && (node_tl == 0)) {
					if (rightNode == queue->front) {
						retry = 0;
						printf("Got the right node lock in access_done by thread ID: %ld\n", tid);
						printf("Current front node is: %p -> %d ,Previous: %p, ThreadID: %ld\n",rightNode, rightNode->key,rightNode->prev, tid);
						printf("queue->front : %p\n", queue->front);
					}
					else {
						pthread_spin_unlock(&rightNode->node_lock);
					}
				}
			}
			
			// Add node to the front of LRU list.
			// Put the requested page before current front
			reqPage->prev = NULL;
			reqPage->next = queue->front;
			queue->front->prev = reqPage;
			queue->front = reqPage;
			queue->count++;

			printf("reqPage: %p, reqPage->key: %d reqPage->prev: %p, reqPage->next: %p \n",reqPage,reqPage->key,reqPage->prev, reqPage->next);
			printf("New front node is: %p -> %d, Previous: %p, Next: %p, ThreadID: %ld\n",queue->front, queue->front->key, queue->front->prev,queue->front->next,tid);
			printf("Old front node is: %p -> %d, Previous: %p, Next: %p, ThreadID: %ld\n", queue->front->next, queue->front->next->key, queue->front->next->prev, queue->front->next->next, tid);
			pthread_spin_unlock(&rightNode->node_lock);
			pthread_spin_unlock(&reqPage->node_lock);
			return;			
		}		
	}
	pthread_spin_unlock(&reqPage->node_lock);
	return;
}
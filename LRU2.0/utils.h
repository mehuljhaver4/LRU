#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <stdatomic.h>
#include <stdint.h>

#define SUCCESS 0
#define FAIL -1
// #define CACHESPACE 10240
// #define HASHSPACE 1024
// #define THREADS 1024
// #define BILLION  1000000000L
// atomic_int buffer_count;

#define CACHESPACE 40
#define HASHSPACE 20
#define THREADS 6
#define BILLION  1000000000L
atomic_int buffer_count;

atomic_int buff_tl;
atomic_int hash_tl;
atomic_int node_tl;
atomic_int left_node_tl;
atomic_int right_node_tl;

// pthread_spinlock_t LRU_lock;
pthread_spinlock_t buffer_lock;
pthread_spinlock_t hashtbl_lock;

void *buffer_pool_ptr = NULL; 

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

	pthread_spin_init(&lastNode->node_lock , 0);
	pthread_spin_init(&leftNode->node_lock , 0);

	int retry = 1;
	//Take try lock on the last node to be evicted.
	while (retry != 0) {
		QNode* lastNode = queue->rear;
		QNode* leftNode = lastNode->prev;
		node_tl = pthread_spin_trylock(&lastNode->node_lock);
		left_node_tl = pthread_spin_trylock(&leftNode->node_lock);

		if ((node_tl == 0) && (left_node_tl == 0) &&(lastNode->ref_count == 0)) {
			//check if the last node is still the same last node of the list
			// same for the left node.
			if((lastNode == queue->rear) && (leftNode == queue->rear->prev)){
				retry = 0;
				break;
			}
			else {
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&lastNode->node_lock);
			}
		}
		else {
			pthread_spin_unlock(&leftNode->node_lock);
			pthread_spin_unlock(&lastNode->node_lock);
			}
		}
	lastNode->ref_count++;
	lastNode->key = key_passed;
	
	//Changing the last node of the LRU list
	queue->rear = queue->rear->prev;
	if (queue->rear)
		queue->rear->next = NULL;
	queue->count--; // decrement the number of full frames by 1

	pthread_spin_unlock(&leftNode->node_lock);
	pthread_spin_unlock(&lastNode->node_lock);
	return lastNode;
}

// A utility function to allocate pageNumber to the cache
QNode* allocate_node(Queue* queue, Hash* hash, unsigned key_passed) {
	// Create a new node with given ke_passed,
	QNode* newBuff = NULL;
	//get_buffer from buffer pool
	pthread_spin_lock(&buffer_lock);

	if (buffer_count != 0 ) { 
        newBuff = get_buffer(root);	
		// newBuff->next = (QNode *)root->next;
		newBuff->next = NULL;
		newBuff->prev = NULL;
		newBuff->key = key_passed;
		newBuff->ref_count++;
		pthread_spin_unlock(&buffer_lock);
		return newBuff;
	}
	pthread_spin_unlock(&buffer_lock);
	newBuff = evict(queue, key_passed);
	// hash->array[key_passed] = NULL;  // should be outside mostly
	return newBuff;
}

// A utility function to access a pageNumber present in cache
void access_node(Queue* queue, Hash* hash, QNode* reqPage, unsigned key) { //change parameters

	pthread_spin_init(&reqPage->node_lock , 0);
	int retry;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&reqPage->node_lock);

		if (node_tl == 0) {
			retry = 0;
			break;
		}
		else
			pthread_spin_unlock(&reqPage->node_lock);
	}

	// check for key match - failure should return appropriate error code and unlock
	
	if (key == reqPage->key){  // condition will change when using the actual hash
		printf("Keys Matched \n");
		printf("ref_count: %d \n",reqPage->ref_count);
		// if ref_count is 0 then node is present in the LRU list
		if (reqPage->ref_count == 0) {
			reqPage->ref_count++;
			// check the position of the node
			// if last then two locks. node and left
			if (reqPage == queue->rear) {
				printf("last node condition \n");
				QNode* leftNode = reqPage->prev;
				pthread_spin_init(&leftNode->node_lock , 0);
				
				retry = 1;
				while (retry != 0) {
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);

					if ((node_tl == 0) && (left_node_tl == 0)) {
						retry = 0;
						break;
					}
					else
						pthread_spin_unlock(&leftNode->node_lock);
				}
				// remove from hash
				hash->array[queue->rear->key] = NULL;
				
				//removing from LRU list
				queue->rear = queue->rear->prev;

				if (queue->rear)
					queue->rear->next = NULL;
				// decrement the number of full frames by 1
				queue->count--;
				printf("last node \n");
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);
				return;
			}

			// if root then two locks. node and right
			else if (reqPage == queue->front) {
				printf("head node condition\n");
				QNode* rightNode = reqPage->next;
				pthread_spin_init(&rightNode->node_lock , 0);
				retry = 1;

				// taking the right node lock
				while (retry != 0) {
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if (right_node_tl == 0) {
						retry = 0;
						break;
					}
					else
						pthread_spin_unlock(&rightNode->node_lock);
				}
				//remove from hash
				// hash->array[queue->front->key] = NULL;
				queue->front = reqPage->next;
				reqPage->prev = NULL;
				printf("head node \n");
				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);
				return;
			}
			// Not root and rear, three locks.  Node left right
			else {
				printf("middle node condition \n");
				printf("Left node is: %d and Right node is: %d \n",reqPage->prev->key, reqPage->next->key);
				QNode* leftNode = reqPage->prev;
				QNode* rightNode = reqPage->next;
				
				pthread_spin_init(&leftNode->node_lock , 0);
				pthread_spin_init(&rightNode->node_lock , 0);

				retry = 1;

				while (retry != 0) {
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if ((left_node_tl == 0) && (right_node_tl == 0)) {
						retry = 0;
						printf("Got right and left locks\n");
						break;
					}
					else {
						pthread_spin_unlock(&rightNode->node_lock);
						pthread_spin_unlock(&leftNode->node_lock);
					}
				}

				//remove from hash
				// hash->array[reqPage->key] = NULL;
				leftNode->next = rightNode;
				rightNode->prev = leftNode;
				reqPage->prev->next = reqPage->next;
				reqPage->next->prev = reqPage->prev;

				printf("middle node \n");
				pthread_spin_unlock(&rightNode->node_lock);
				pthread_spin_unlock(&leftNode->node_lock);
				pthread_spin_unlock(&reqPage->node_lock);
				return;
			}

		}

		else if (reqPage->ref_count > 0) { // node is being used by other thread
			reqPage->ref_count++;
			pthread_spin_unlock(&reqPage->node_lock);
			return;
		}
	}
	else {
		printf("ERROR... \n"); // use error
		pthread_spin_unlock(&reqPage->node_lock);
		return;
	}
}

// A utility function to delete a pageNumber from cache
void free_node(Queue* queue, QNode* reqPage, unsigned key) {   // remove from hash first??

	pthread_spin_init(&reqPage->node_lock , 0);
	int retry;
	//take the main node lock for all scenarios
	while (retry != 0) {
	node_tl = pthread_spin_trylock(&reqPage->node_lock);

		if (node_tl == 0) {
			retry = 0;
			break;
		}
		else
			pthread_spin_unlock(&reqPage->node_lock);
	}

	if (key == reqPage->key){ 
		if (reqPage->ref_count > 0) {  // cannot free as node is being used by other thread
			pthread_spin_unlock(&reqPage->node_lock);
			printf("NODE IS IN USE!!! \n");  // return error statement
		}
		else if (reqPage->ref_count == 0) {
			//clear node key
			reqPage->key = (intptr_t) NULL;

			// if reqPage is the root
			if (queue->front == reqPage) { 
				// take right node lock
				QNode* rightNode = reqPage->next;
				pthread_spin_init(&rightNode->node_lock , 0);
				retry = 1;

				// taking the right node lock
				while (retry != 0) {
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if (right_node_tl == 0) {
						retry = 0;
						break;
					}
					else
						pthread_spin_unlock(&rightNode->node_lock);
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
				pthread_spin_init(&leftNode->node_lock , 0);
				int retry = 1;
				//Take try lock on the last node to be evicted.
				while (retry != 0) {
					QNode* leftNode = reqPage->prev;
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);

					if (left_node_tl == 0) {
						//check if the last node is still the same last node of the list
						// same for the left node.
						if((reqPage == queue->rear) && (leftNode == queue->rear->prev)){
							retry = 0;
							break;
						}
						else 
							pthread_spin_unlock(&leftNode->node_lock);
					}
					else
						pthread_spin_unlock(&leftNode->node_lock);
					}
					//Changing the last node of the LRU list
					queue->rear = queue->rear->prev;
					if (queue->rear)
						queue->rear->next = NULL;
					queue->count--; // decrement the number of full frames by 1	

					put_buffer(&root, (buffer_node *)reqPage);

					pthread_spin_unlock(&leftNode->node_lock);
					pthread_spin_unlock(&reqPage->node_lock);
					return;			
			}

			else {
				QNode* leftNode = reqPage->prev;
				QNode* rightNode = reqPage->next;
				pthread_spin_init(&leftNode->node_lock , 0);
				pthread_spin_init(&rightNode->node_lock , 0);
				retry = 1;

				while (retry != 0) {
					left_node_tl = pthread_spin_trylock(&leftNode->node_lock);
					right_node_tl = pthread_spin_trylock(&rightNode->node_lock);

					if ((left_node_tl == 0) && (right_node_tl == 0)) {
						retry = 0;
						break;
					}
					else {
						pthread_spin_unlock(&rightNode->node_lock);
						pthread_spin_unlock(&leftNode->node_lock);
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
}

void access_done(Queue* queue, Hash* hash, QNode* reqPage) {
	printf("Inside access done\n");
	pthread_spin_init(&reqPage->node_lock , 0);
	int retry = 1;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&reqPage->node_lock);
		if (node_tl == 0) {
			retry = 0;
			break;
		}
		else
			pthread_spin_unlock(&reqPage->node_lock);
	}
	printf("Got the node lock in access done\n");
	reqPage->ref_count--;
	if (reqPage->ref_count == 0) {
		// If queue is empty, change both front and rear pointers
		if (isQueueEmpty(queue)) {
			queue->rear = queue->front = reqPage;
			pthread_spin_unlock(&reqPage->node_lock);
			return;
		}
		else {
			// change the front
			//take a lock on the present first node
			QNode* rightNode = queue->front;
			printf("Current front node is: %p -> %d\n",rightNode, rightNode->key);
			pthread_spin_init(&rightNode->node_lock , 0);
		
			retry = 1;
			// taking the right node lock
			while (retry != 0) {
				right_node_tl = pthread_spin_trylock(&rightNode->node_lock);
				if (right_node_tl == 0) {
					retry = 0;
					break;
				}
				else
					pthread_spin_unlock(&rightNode->node_lock);
			}
			printf("Got the right node lock in access done\n");		
			// Add node to the front of LRU list.
			// Put the requested page before current front
				reqPage->prev = NULL;
				reqPage->next = queue->front;
				queue->front->prev = reqPage;
				queue->front = reqPage;
				queue->count++;
			
			pthread_spin_unlock(&rightNode->node_lock);
			pthread_spin_unlock(&reqPage->node_lock);
				printf("Success \n");
				return;			
		}		
	}
	pthread_spin_unlock(&reqPage->node_lock);
	return;
}
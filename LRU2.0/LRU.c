#include "LRU.h"

void *buffer_pool_ptr = NULL; 
QNode* root = NULL;
QNode* ref_node = NULL;

void print_buffer_pool() {
    printf("\n***** Block start: %p ****\n", buffer_pool_ptr);
	printf("root: %p\n", root);
    QNode *temp = root;
    int buf_counter = buffer_count;
    while(buf_counter){
        printf("\nBuffer %d: %p\n",buf_counter,temp);
        buf_counter--;

        temp = temp->next;
         }
    printf("NULL\n");
}

// Add buffers to the memory pool
void add_buffer(QNode *ref_node)
{	
    QNode* new_node = ref_node;

    if (root == NULL) {
        root = new_node;
        new_node->next = new_node->prev = NULL;
        return;
    }
    new_node->next = root;
    new_node->prev = NULL;
    root = new_node;
    return;
}

// create a memory pool to add buffers for later use
int buffer_pool() {
    buffer_pool_ptr = calloc(CACHESPACE ,sizeof(QNode));
    printf("\n Memory pool created\n");
    
	if(!buffer_pool_ptr) {
        printf("\nBuffer memory allocation failed\n");
        return FAIL;
    }
    // using a ref node to update the pointer after incrementing
    ref_node = buffer_pool_ptr; 

    for(int i = 0; i< CACHESPACE; i++) {
		// initiliase node lock here
		if (pthread_spin_init(&ref_node->node_lock, 0) != 0) {
			printf("\n spin node lock has failed\n");
			return -1;
		}
        add_buffer(ref_node);
		buffer_count++;
        ref_node++;
    }
    return SUCCESS;
}

// A utility function to add the buffer back to the buffer pool
void put_buffer(QNode* free_buffer) {
	
	pthread_spin_lock(&buffer_lock);
	QNode* new_buffer = free_buffer;
    if (root == NULL) {
        root = new_buffer;
        new_buffer->next = new_buffer->prev = NULL;
		buffer_count++;
		pthread_spin_unlock(&buffer_lock);
        return;
    }
    new_buffer->next = root;
    new_buffer->prev = NULL;
    root = new_buffer;
	buffer_count++;
	pthread_spin_unlock(&buffer_lock);
	return;
}

// A utility function to get a buffer from buffer pool
QNode* get_buffer() {	
	QNode* new_buffer = root;
	if (buffer_count == 1) {
		root = NULL;
		buffer_count--;
		return new_buffer;
	}
	root = root->next;
	buffer_count--;
	return new_buffer;
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
	for (int i = 0; i < hash->capacity; ++i) {
		hash->array[i] = NULL;
	}
	return hash;
}

/*
* Evict is called by allocate_node() when there are no buffers left in the buffer pool.
* It evicts the last node present in the LRU list to allocate a new node.
* We take two locks i.e, last node and second last node lock before evicting the last node. 
*/
QNode* evict(Queue* queue, unsigned key_passed) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* last_node = NULL;
	QNode* left_node = NULL;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}

	//take the last node lock first
	int retry;
	evict_rear_lock:
		retry = 1;
		while (retry != 0) {
			last_node = queue->rear;
			node_tl = pthread_spin_trylock(&last_node->node_lock);
			if (node_tl == 0) {
				if (queue->rear == last_node) {
					retry = 0;
				}
				else {
					pthread_spin_unlock(&last_node->node_lock);
				}
			}
		}
	
		retry = 1;
		while (retry != 0) {
			left_node = last_node->prev;
			left_node_tl = pthread_spin_trylock(&left_node->node_lock);
			if (left_node_tl == 0) {
				if (queue->rear->prev == left_node) {
					retry = 0;
				}
				else {
					pthread_spin_unlock(&left_node->node_lock);
					pthread_spin_unlock(&last_node->node_lock);
					goto evict_rear_lock;
				}
			}
			else {
				pthread_spin_unlock(&last_node->node_lock);
				goto evict_rear_lock;
			}
		}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}

	assert((last_node != NULL) && (left_node != NULL));
	
	//Changing the last node of the LRU list
	queue->rear = last_node->prev;
	last_node->next = NULL;
	last_node->prev = NULL;

	if (queue->rear) {
		queue->rear->next = NULL;
	}
	queue->count--;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
	}
	pthread_spin_unlock(&left_node->node_lock);
	pthread_spin_unlock(&last_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
	}
	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

	allocation_time_avg += accum_4;
	printf("> Time taken b/w lock to *EVICT* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",last_node->key, accum_1, accum_2, accum_3, accum_4);
	return last_node;
}

/* 
* A utility function to allocate pageNumber/key to the cache.
* First it checks the buffer pool for buffers, if available then,
* it allocates a buffer node with the pageNumber/key. 
* If there are no buffers available then it calls evict()
*/
QNode* allocate_node(Queue* queue, unsigned key_passed, long tid) {

	QNode* new_buff = NULL;
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
	
	total_allocations++;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}
	pthread_spin_lock(&buffer_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}
	ref_node--;
	if (buffer_count != 0 ) { 
        new_buff = get_buffer();	
		new_buff->next = NULL;
		new_buff->prev = NULL;
		new_buff->key = key_passed;
		new_buff->ref_count = 0;

		if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
			perror( "clock gettime" );
		}

		pthread_spin_unlock(&buffer_lock);

		if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
		}

		accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
		accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
		accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
		accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

		allocation_time_avg += accum_4;
		printf("> Time taken b/w lock to *ALLOCATE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",new_buff->key, accum_1, accum_2, accum_3, accum_4);
		return new_buff;
	}
	pthread_spin_unlock(&buffer_lock);
	new_buff = evict(queue, key_passed);
	new_buff->key = key_passed;
	new_buff->ref_count = 0;
	return new_buff;
}

/*
* Utility function to access the rear node of the LRU list
* We take a lock on the last second node before accessing the last node of the cache.
* If lock is not aquired then we unlock the main node lock too and retry till both locks are aquired.
* After accesing the last node we make the second last node as the new rear node of the cache.
*/
int access_rear_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* left_node = NULL;
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}
	int retry = 1;
	while (retry != 0) {
		left_node = req_page->prev;
		left_node_tl = pthread_spin_trylock(&left_node->node_lock);
		if (left_node_tl == 0) {
			if (req_page == queue->rear) {
				retry = 0;
			}
			else {
				pthread_spin_unlock(&left_node->node_lock);
				return FAIL;
			}
		}
		else {
			return FAIL;
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}
	//removing from LRU list
	queue->rear = queue->rear->prev;
	req_page->next = NULL;
	req_page->prev = NULL;

	if (queue->rear) {
		queue->rear->next = NULL;
	}
	queue->count--;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
	}

	pthread_spin_unlock(&left_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_4;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

/*
* Utility function to access the front node of the LRU list
* We take a lock on the second node before accessing the first node of the cache.
* If lock is not aquired then we unlock the main node lock too and retry till both locks are aquired.
* After accesing the first node we make the second node as the new front node of the cache.
*/
int access_front_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* right_node = NULL;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}
	int retry = 1;
	while (retry != 0) {
		right_node = req_page->next;
		right_node_tl = pthread_spin_trylock(&right_node->node_lock);
		if (right_node_tl == 0) {
			if (req_page == queue->front) {
				retry = 0;
			}
			else {
				pthread_spin_unlock(&right_node->node_lock);
				return FAIL;
			}
		}
		else {
			return FAIL;
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}
	queue->front = req_page->next;
	req_page->prev = NULL;
	req_page->next = NULL;
	queue->count --;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
	}

	pthread_spin_unlock(&right_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_4;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

/*
* Utility function to access a node which is not the front or rear node of the cache
* We take two locks i.e, lock on the left and right node of the main node. 
* If any of the lock is not aquired then we unlock the aquired locks and retry till all 
* three locks are aquired.
*/
int access_middle_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* left_node = req_page->prev;
	QNode* right_node = req_page->next;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}

	int retry = 1;
	while (retry != 0) {
		left_node_tl = pthread_spin_trylock(&left_node->node_lock);
		right_node_tl = pthread_spin_trylock(&right_node->node_lock);

		if ((left_node_tl == 0) && (right_node_tl == 0)) {
			retry = 0;
		}
		else {
			if(left_node_tl == 0) {
				pthread_spin_unlock(&left_node->node_lock);
				return FAIL;
			}
			else if (right_node_tl == 0) {
				pthread_spin_unlock(&right_node->node_lock);
				return FAIL;
			}
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}

	left_node->next = right_node;
	right_node->prev = left_node;
	req_page->next = NULL;
	req_page->prev = NULL;
	queue->count--;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
	}

	pthread_spin_unlock(&right_node->node_lock);
	pthread_spin_unlock(&left_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
	}
	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_4;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

/*
* A utility function to access a pageNumber/key present in cache
* If the hash key and node key matches and if the ref_count = 0, access_node() locates
* the position of the node in the cache and accesses it.
* If the ref_count >0 then, it just increments the ref_count which means some other 
* thread is using that node.
* If the keys do not match then, it allocates the pageNumber/key before accessing it.
*/
int access_node(Queue* queue, QNode* req_page, unsigned key, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
	
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}

	//take the main node lock for all scenarios
	int retry;
	access_main_lock:
		retry = 1;
		while (retry != 0) {
			node_tl = pthread_spin_trylock(&req_page->node_lock);
			if (node_tl == 0)
				retry = 0;
		}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}

	// check for key match - failure should return appropriate error code and unlock	
	if (key == req_page->key){  	
		
		// if ref_count is 0 then node is present in the LRU list
		if (req_page->ref_count == 0) {
			total_access++;
			req_page->ref_count++;

			// if there is only one node in the LRU list.
			if ((queue->front == req_page) && (queue->rear == req_page)) {
				queue->front = queue->rear = NULL;
				req_page->next = req_page->prev = NULL;
				queue->count--;
				
				if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
					perror( "clock gettime" );
				}
				pthread_spin_unlock(&req_page->node_lock);

				if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
					perror( "clock gettime" );
				}
			
				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				access_time_avg += accum_4;
				printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
				return SUCCESS;
			}			
			// check the position of the node
			// rear node
			else if (req_page == queue->rear) {
				if (access_rear_node(queue, req_page, tid) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto access_main_lock;
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}

			// front node
			else if (req_page == queue->front) {
				if (access_front_node(queue, req_page, tid) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto access_main_lock;	
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}

			// middle node
			else if ((req_page->next != NULL) && (req_page->prev != NULL)) {
				if (access_middle_node(queue, req_page, tid) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto access_main_lock;	
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}

			else {
				pthread_spin_unlock(&req_page->node_lock);
				return SUCCESS;
			}
		}
		 // node is being used by other thread
		else if (req_page->ref_count > 0) {
			req_page->ref_count++;
			pthread_spin_unlock(&req_page->node_lock);
			return SUCCESS;
		}
	}
	else {
		printf("\nERROR... KEYS NOT MATCHING CALLING ALLOCATE \n");
		pthread_spin_unlock(&req_page->node_lock);
		return FAIL;
	}
}

/*
* Utility function to free the first entry of the cache
* We take a lock on the second node before freeing the first node of the cache.
* If lock is not aquired then we unlock the main node lock too and retry till both locks are aquired.
* After freeing the first node we make the second node as the new front node of the cache and put the
* used buffer back to the buffer pool.
*/
int free_front_node(Queue* queue, QNode* req_page, unsigned key) {
	QNode* right_node = NULL;
	int retry = 1;
	while (retry != 0) {
		right_node = req_page->next;
		right_node_tl = pthread_spin_trylock(&right_node->node_lock);
		if (right_node_tl == 0) {
			if (req_page == queue->front) {
				retry = 0;
			}
			else {
				pthread_spin_unlock(&right_node->node_lock);
				return FAIL;
			}
		}
		else {
			return FAIL;
		}
	}
	
	// change the front of the LRU list and put it back to free pool
	queue->front = req_page->next;
	queue->front->prev = NULL;
	put_buffer(req_page);

	pthread_spin_unlock(&right_node->node_lock);
	return SUCCESS;
}

/*
* Utility function to free the last entry of the cache
* We take a lock on the last second node before freeing the last node of the cache.
* If lock is not aquired then we unlock the main node lock too and retry till both locks are aquired.
* After freeing the last node we make the second last node as the new rear node of the cache and put the
* used buffer back to the buffer pool.
*/
int free_rear_node(Queue* queue, QNode* req_page, unsigned key) {
	QNode* left_node = NULL;
	int retry = 1;
	//Take try lock on the last node to be evicted.
	while (retry != 0) {
		left_node = req_page->prev;
		if (left_node) {
			left_node_tl = pthread_spin_trylock(&left_node->node_lock);

			if (left_node_tl == 0) {
				//check if the last node is still the same last node of the list
				// same for the left node.
				if((req_page == queue->rear) && (left_node == queue->rear->prev)) {
					retry = 0;
				}
				else {
					pthread_spin_unlock(&left_node->node_lock);
					return FAIL;
				}
			}
			else {
				return FAIL;
			}
		}
	}
	//Changing the last node of the LRU list
	req_page->prev->next = req_page->next;
	queue->rear = left_node;
	if (queue->rear) {
		queue->rear->next = NULL;
	}
	queue->count--; 
	put_buffer(req_page);

	pthread_spin_unlock(&left_node->node_lock);
	return SUCCESS;	
}

/*
* Utility function to free an entry of the cache which is not first or last
* We take two locks i.e, lock on the left and right node of the main node. 
* If any of the lock is not aquired then we unlock the aquired locks and retry till all 
* three locks are aquired.
* After freeing the node, put the used buffer back to the buffer pool.
*/
int free_middle_node(Queue* queue, QNode* req_page, unsigned key) {
	QNode* left_node = NULL;
	QNode* right_node = NULL;
	
	int retry = 1;
	while (retry != 0) {
		left_node = req_page->prev;
		right_node = req_page->next;
		if ((left_node) && (right_node)) {
			left_node_tl = pthread_spin_trylock(&left_node->node_lock);
			right_node_tl = pthread_spin_trylock(&right_node->node_lock);

			if ((left_node_tl == 0) && (right_node_tl == 0)) {
				retry = 0;
			}
			
			else {
				if(left_node_tl == 0) {
					pthread_spin_unlock(&left_node->node_lock);
					return FAIL;
				}
				if (right_node_tl == 0) {
					pthread_spin_unlock(&right_node->node_lock);
					return FAIL;
				}
			}
		}
	}
	right_node->prev = left_node;
	left_node->next = right_node;
	put_buffer(req_page);

	pthread_spin_unlock(&right_node->node_lock);
	pthread_spin_unlock(&left_node->node_lock);
	return SUCCESS;
}

/*
* A utility function to delete a pageNumber/key from cache
* If the hash key and node key matches and if the ref_count = 0, free_node() locates
* the position of the node in the cache and removes it and places the buffer back to the buffer pool
* that can be used later on for allocating a new node.
* If the ref_count >0, it means that the node is in use by some other thread and cannot be freed.
* If the keys do not match then it means that the pageNumber/key is not present in the cache.
*/
int free_node(Queue* queue, QNode* req_page, unsigned key) {

	//take the main node lock for all scenarios
	int retry;
	free_main_lock:
		retry = 1;		
		while (retry != 0) {
			node_tl = pthread_spin_trylock(&req_page->node_lock);
			if (node_tl == 0) {
				retry = 0;
			}
		}

	if (key == req_page->key){ 
		if (req_page->ref_count > 0) {  // cannot free as node is being used by other thread
			pthread_spin_unlock(&req_page->node_lock);
			printf("NODE IS IN USE!!! \n");
		}
		else if (req_page->ref_count == 0) {

			if (req_page->key == 0) {
				 put_buffer(req_page);
				 pthread_spin_unlock(&req_page->node_lock);
				 return SUCCESS;
			}
			// front node
			else if (queue->front == req_page) { 
				if (free_front_node(queue,req_page,key) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto free_main_lock;
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}
			// rear node
			else if (queue->rear == req_page) {
				if (free_rear_node(queue,req_page,key) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto free_main_lock;
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}
			// middle node
			else if ((req_page->next != NULL) && (req_page->prev != NULL)) {
				if (free_middle_node(queue,req_page,key) == -1) {
					pthread_spin_unlock(&req_page->node_lock);
					goto free_main_lock;
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					return SUCCESS;
				}
			}
		}
	}
	else {
		printf("ERROR.. PAGE NUMBER NOT PRESENT IN CACHE \n");
		pthread_spin_unlock(&req_page->node_lock);
		return SUCCESS;
	}	
}

/*
* Utility function to indicate an entry's access is completed.
* When the ref_count is 0, it means the node is not being used by any other 
* thread and is now ready to be placed in front of the cache. 
* We take a lock on the current front node while inserting the accessed node as
* the new front node of the cache.
*/
void access_done(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3, stop_4, stop_5;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0, accum_5 = 0;
	
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
	}
	int retry;
	access_done_main_lock:
		retry = 1;
		//take the main node lock for all scenarios
		while (retry != 0) {
			node_tl = pthread_spin_trylock(&req_page->node_lock);
			if (node_tl == 0) { 
				retry = 0;
			}
		}
	
	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
	}

	// assert(req_page->ref_count > 0);
	req_page->ref_count--;
	
	if (req_page->ref_count == 0) {
		total_access_done++;

		// If queue is empty, change both front and rear pointers
		if (isQueueEmpty(queue)) {
		// When 2 threads try to insert two different nodes in an empty cache at the same time
			if ((queue->rear == NULL) && (queue->front == NULL)) {
				if (__sync_val_compare_and_swap(&queue->rear, NULL, req_page) != NULL) {
					printf("Some other thread has entred this condition\n");
				}
				if(__sync_val_compare_and_swap(&queue->front, NULL, req_page) != NULL) {
					printf("Some other thread has entred this condition\n");					
				}
			}
			req_page->next = req_page->prev = NULL;
			queue->count++;
			if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
			}

			pthread_spin_unlock(&req_page->node_lock);
			
			if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
			}
				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				access_done_time_avg += accum_4;
				printf("> Time taken b/w lock for *ACCESS_DONE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);			
			return;
		}
		else {
			if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
			}
			// change the front, take a lock on the present first node
			QNode* right_node = NULL;
			retry = 1;
			while (retry != 0) {
				right_node = queue->front;
				right_node_tl = pthread_spin_trylock(&right_node->node_lock);
				if ((right_node_tl == 0)) {
					if (right_node == queue->front) {
						retry = 0;
					}
					else {
						pthread_spin_unlock(&right_node->node_lock);
						pthread_spin_unlock(&req_page->node_lock);
						goto access_done_main_lock;
					}
				}
				else {
					pthread_spin_unlock(&req_page->node_lock);
					goto access_done_main_lock;
				}
			}
			if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
			}
			
			// Add node to the front of LRU list.
			// Put the requested page before current front
			req_page->prev = NULL;
			req_page->next = right_node;
			right_node->prev = req_page;
			queue->front = req_page;
			queue->count++;

			if( clock_gettime( CLOCK_REALTIME, &stop_4) == -1 ) {
				perror( "clock gettime" );
			}

			pthread_spin_unlock(&right_node->node_lock);
			pthread_spin_unlock(&req_page->node_lock);

			if( clock_gettime( CLOCK_REALTIME, &stop_5) == -1 ) {
				perror( "clock gettime" );
			}

			accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
			accum_2 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
			accum_3 = (uint64_t)(( stop_4.tv_sec - stop_3.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_4.tv_nsec - stop_3.tv_nsec);
			accum_4 = (uint64_t)(( stop_5.tv_sec - stop_4.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_5.tv_nsec - stop_4.tv_nsec);
			accum_5 = (uint64_t)(( stop_5.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_5.tv_nsec - start_1.tv_nsec);

			access_done_time_avg += accum_5;
			printf("> Time taken b/w lock to *ACCESS_DONE* page number: %d is main_lock : %ld  right_lock : %ld main_logic : %ld unlock: %ld access call: %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4, accum_5);
			return;			
		}		
	}
	pthread_spin_unlock(&req_page->node_lock);
	return;
}
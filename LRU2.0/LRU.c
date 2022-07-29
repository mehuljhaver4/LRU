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
	for (int i = 0; i < hash->capacity; ++i)
		hash->array[i] = NULL;

	return hash;
}

QNode* evict(Queue* queue, unsigned key_passed) {
	// record the time taken before applying the lock
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* last_node = NULL;
	QNode* left_node = NULL;

	int retry = 1;
	//Take try lock on the last node to be evicted.

	while (retry != 0) {
		last_node = queue->rear;
		left_node = last_node->prev;

		if ((last_node) && (left_node)) {

			if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
			}
			node_tl = pthread_spin_trylock(&last_node->node_lock);
			left_node_tl = pthread_spin_trylock(&left_node->node_lock);

			if ((node_tl == 0) && (left_node_tl == 0) && (last_node->ref_count == 0)) {
				//check if the last node is still the same last node of the list
				// same for the left node.
				if((last_node == queue->rear) && (left_node == queue->rear->prev)) {
					retry = 0;
					if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
						perror( "clock gettime" );
						// return EXIT_FAILURE;
					}
				}
				else {
					pthread_spin_unlock(&left_node->node_lock);
					pthread_spin_unlock(&last_node->node_lock);
				}
			}
			else {
				if (left_node_tl == 0) {
					pthread_spin_unlock(&left_node->node_lock);
				}
				else if(node_tl == 0) {
					pthread_spin_unlock(&last_node->node_lock);
				}
			}
		}
	}

	assert((last_node != NULL) && (left_node != NULL));
	
	//Changing the last node of the LRU list
	queue->rear = last_node->prev;
	last_node->key = key_passed;
	last_node->next = NULL;
	last_node->prev = NULL;

	if (queue->rear) {
		queue->rear->next = NULL;
	}
	queue->count--;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	pthread_spin_unlock(&left_node->node_lock);
	pthread_spin_unlock(&last_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

	allocation_time_avg += accum_2;
	printf("> Time taken b/w lock to *EVICT* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",last_node->key, accum_1, accum_2, accum_3, accum_4);
	return last_node;
}

// A utility function to allocate pageNumber to the cache
QNode* allocate_node(Queue* queue, Hash* hash, unsigned key_passed, long tid) {

	QNode* new_buff = NULL;
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
	
	total_allocations++;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	pthread_spin_lock(&buffer_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	ref_node--;
	if (buffer_count != 0 ) { 
        new_buff = get_buffer();	
		new_buff->next = NULL;
		new_buff->prev = NULL;
		new_buff->key = key_passed;

		if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
			perror( "clock gettime" );
			// return EXIT_FAILURE;
		}

		pthread_spin_unlock(&buffer_lock);

		if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
		}

		accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
		accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
		accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
		accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

		allocation_time_avg += accum_2;
		printf("> Time taken b/w lock to *ALLOCATE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",new_buff->key, accum_1, accum_2, accum_3, accum_4);
		return new_buff;
	}
	pthread_spin_unlock(&buffer_lock);
	new_buff = evict(queue, key_passed);
	return new_buff;
}

// Utility function to access the rear node of the LRU list
int access_rear_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* left_node = NULL;
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
			}
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
		// return EXIT_FAILURE;
	}

	pthread_spin_unlock(&left_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_2;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

// Utility function to access the head/front node of the LRU list
int access_front_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* right_node = NULL;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
			}
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	queue->front = req_page->next;
	req_page->prev = NULL;
	req_page->next = NULL;
	queue->count --;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	pthread_spin_unlock(&right_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_2;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

int access_middle_node(Queue* queue, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;

	QNode* left_node = req_page->prev;
	QNode* right_node = req_page->next;

	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
			}
			else if (right_node_tl == 0) {
				pthread_spin_unlock(&right_node->node_lock);
			}
		}
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	left_node->next = right_node;
	right_node->prev = left_node;
	req_page->next = NULL;
	req_page->prev = NULL;
	queue->count--;

	if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	pthread_spin_unlock(&right_node->node_lock);
	pthread_spin_unlock(&left_node->node_lock);

	if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
	accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
	accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
	accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);
	access_time_avg += accum_2;
	printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
	return SUCCESS;
}

// A utility function to access a pageNumber present in cache
int access_node(Queue* queue, QNode* req_page, unsigned key, long tid) { //change parameters
	struct timespec start_1, stop_1, stop_2, stop_3;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0;
	
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	//take the main node lock for all scenarios
	int retry = 1;
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&req_page->node_lock);
		if (node_tl == 0)
			retry = 0;
	}

	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
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
					// return EXIT_FAILURE;
				}
				pthread_spin_unlock(&req_page->node_lock);

				if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
					perror( "clock gettime" );
					// return EXIT_FAILURE;
				}
			
				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				access_time_avg += accum_2;
				printf("> Time taken b/w lock to *ACCESS* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);
				return SUCCESS;
			}			
			// check the position of the node
			// if last then two locks. node and left
			else if (req_page == queue->rear) {
				access_rear_node(queue, req_page, tid);
				pthread_spin_unlock(&req_page->node_lock);
				return SUCCESS;
			}

			// if root then two locks. node and right
			else if (req_page == queue->front) {
				access_front_node(queue, req_page, tid);
				pthread_spin_unlock(&req_page->node_lock);
				return SUCCESS;
			}

			// Not root and rear, three locks.  Node left right
			else if ((req_page->next != NULL) && (req_page->prev != NULL)) {
				access_middle_node(queue, req_page, tid);
				pthread_spin_unlock(&req_page->node_lock);
				return SUCCESS;
			}

			else {
				pthread_spin_unlock(&req_page->node_lock);
				return SUCCESS;
			}
		}
		else if (req_page->ref_count > 0) { // node is being used by other thread
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

void free_front_node(Queue* queue, QNode* req_page, unsigned key) {
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
			}
		}
	}
	
	// change the front of the LRU list and put it back to free pool
	queue->front = req_page->next;
	queue->front->prev = NULL;
	put_buffer(req_page);

	pthread_spin_unlock(&right_node->node_lock);
	pthread_spin_unlock(&req_page->node_lock);
	return;
}

void free_rear_node(Queue* queue, QNode* req_page, unsigned key) {
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
				}
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
	pthread_spin_unlock(&req_page->node_lock);
	return;	
}

void free_middle_node(Queue* queue, QNode* req_page, unsigned key) {
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
				}
				else if (right_node_tl == 0) {
					pthread_spin_unlock(&right_node->node_lock);
				}
			}
		}
	}
	right_node->prev = left_node;
	left_node->next = right_node;
	put_buffer(req_page);

	pthread_spin_unlock(&right_node->node_lock);
	pthread_spin_unlock(&left_node->node_lock);
	pthread_spin_unlock(&req_page->node_lock);
	return;
}

// A utility function to delete a pageNumber from cache
void free_node(Queue* queue, QNode* req_page, unsigned key) {

	int retry = 1;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&req_page->node_lock);
		if (node_tl == 0) {
			retry = 0;
		}
	}

	if (key == req_page->key){ 
		if (req_page->ref_count > 0) {  // cannot free as node is being used by other thread
			pthread_spin_unlock(&req_page->node_lock);
			printf("NODE IS IN USE!!! \n");  // return error statement
		}
		else if (req_page->ref_count == 0) {
			//clear node key
			req_page->key = (intptr_t) NULL;

			// if req_page is the root, take right node lock
			if (queue->front == req_page) { 
				free_front_node(queue,req_page,key);
			}

			// if req_page is last node
			else if (queue->rear == req_page) {
				free_rear_node(queue,req_page,key);
			}

			else if ((req_page->next != NULL) && (req_page->prev != NULL)) {
				free_middle_node(queue,req_page,key);
			}
		}
	}
	else {
		printf("ERROR.. PAGE NUMBER NOT PRESENT IN CACHE \n");
		pthread_spin_unlock(&req_page->node_lock);
		return;
	}	
}

void access_done(Queue* queue, Hash* hash, QNode* req_page, long tid) {
	struct timespec start_1, stop_1, stop_2, stop_3, stop_4, stop_5;
	uint64_t accum_1 = 0, accum_2 = 0, accum_3 = 0, accum_4 = 0, accum_5 = 0;
	
	if( clock_gettime( CLOCK_REALTIME, &start_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}
	int retry = 1;
	//take the main node lock for all scenarios
	while (retry != 0) {
		node_tl = pthread_spin_trylock(&req_page->node_lock);
		if (node_tl == 0) { 
			retry = 0;
		}
	}
	
	if( clock_gettime( CLOCK_REALTIME, &stop_1) == -1 ) {
		perror( "clock gettime" );
		// return EXIT_FAILURE;
	}

	assert(req_page->ref_count > 0);
	req_page->ref_count--;
	
	if (req_page->ref_count == 0) {
		// If queue is empty, change both front and rear pointers
		if (isQueueEmpty(queue)) {
			queue->rear = queue->front = req_page;
			req_page->next = req_page->prev = NULL;
			queue->count++;
			if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
			}

			pthread_spin_unlock(&req_page->node_lock);
			
			if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
			}
				accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
				accum_2 = (uint64_t)(( stop_2.tv_sec - stop_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_2.tv_nsec - stop_1.tv_nsec);
				accum_3 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
				accum_4 = (uint64_t)(( stop_3.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - start_1.tv_nsec);

				access_done_time_avg += accum_2;
				printf("> Time taken b/w lock for *ACCESS_DONE* page number: %d is lock : %ld main_logic : %ld unlock : %ld access call : %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4);			
			return;
		}
		else {
			if( clock_gettime( CLOCK_REALTIME, &stop_2) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
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
					}
				}
			}
			if( clock_gettime( CLOCK_REALTIME, &stop_3) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
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
				// return EXIT_FAILURE;
			}

			pthread_spin_unlock(&right_node->node_lock);
			pthread_spin_unlock(&req_page->node_lock);

			if( clock_gettime( CLOCK_REALTIME, &stop_5) == -1 ) {
				perror( "clock gettime" );
				// return EXIT_FAILURE;
			}

			accum_1 = (uint64_t)(( stop_1.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_1.tv_nsec - start_1.tv_nsec);
			accum_2 = (uint64_t)(( stop_3.tv_sec - stop_2.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_3.tv_nsec - stop_2.tv_nsec);
			accum_3 = (uint64_t)(( stop_4.tv_sec - stop_3.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_4.tv_nsec - stop_3.tv_nsec);
			accum_4 = (uint64_t)(( stop_5.tv_sec - stop_4.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_5.tv_nsec - stop_4.tv_nsec);
			accum_5 = (uint64_t)(( stop_5.tv_sec - start_1.tv_sec )*(uint64_t)BILLION) + (uint64_t)( stop_5.tv_nsec - start_1.tv_nsec);

			access_done_time_avg += accum_3;
			printf("> Time taken b/w lock to *ACCESS_DONE* page number: %d is main_lock : %ld  right_lock : %ld main_logic : %ld unlock: %ld access call: %ld\n",req_page->key, accum_1, accum_2, accum_3, accum_4, accum_5);

			return;			
		}		
	}
	pthread_spin_unlock(&req_page->node_lock);
	return;
}

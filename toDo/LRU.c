#include "LRU.h"
// #include "bufferPool.h"

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

	//when cache is empty or reqPage is NULL
	if (queue->front == NULL || reqPage == NULL)
		printf("ERROR\n"); //add error statement later
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
	}

	printf("\nPage requested to be deleted is at %p and is %d \n",reqPage,reqPage->pageNumber);
	return reqPage;
}
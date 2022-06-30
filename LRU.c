#include "utils.h"
#include "LRU.h"
#include "BufferPool.h"


typedef struct QNode {
	struct QNode *prev, *next;
	unsigned pageNumber; 
} QNode;


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
		hash->array[queue->rear->pageNumber] = NULL; // hashtbl lock here?
		newBuff = deQueue(queue);
		newBuff->pageNumber = pageNumber;
		pthread_spin_unlock(&LRU_lock);
		// printf("-> Address of new node due to no new buffers in pool is: %p // content: %d \n",newBuff,newBuff->pageNumber);
	}
	return newBuff;
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

QNode* allocate_node(Queue* queue, Hash* hash, unsigned pageNumber) {
		// record the time taken before applying the lock
		clock_t start_allocate, end_allocate;
		start_allocate = clock();
		
		// Enqueue(queue, hash, pageNumber);
		
		// Create a new node with given page number,
		// And add the new node to the front of queue
		QNode* temp = newQNode(queue, hash, pageNumber);

		pthread_spin_lock(&LRU_lock);
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

		pthread_spin_unlock(&LRU_lock);
		
		end_allocate = clock();  //try gettime of the day
		double time_taken_allocate = ((double)(end_allocate- start_allocate))/ CLOCKS_PER_SEC; // in seconds
		time_taken_allocate *= pow(10,9); //nanosec
    	printf("> Time taken b/w lock to *ALLOCATE* page number: %d is %f nanosec\n",queue->front->pageNumber, time_taken_allocate);
		
		return temp;
}

void access_node(Queue* queue, Hash* hash, QNode* reqPage) { //change parameters
		// record the time taken before applying the lock
		// clock_t start, end;
 		// gettimeofday(&start_time, NULL);		
		// struct timespec tstart={0,0}, tend={0,0};
		struct timespec start, stop;
		double accum;
    // clock_gettime(CLOCK_REALTIME, &tstart);
    // some_long_computation();
// 

		pthread_spin_lock(&LRU_lock);

		if( clock_gettime( CLOCK_MONOTONIC, &start) == -1 ) {
		perror( "clock gettime" );
		return EXIT_FAILURE;
		}

		// struct timeval start_time;
		// struct timeval end_time;

  		// printf("micro seconds : %ld",start_time.tv_usec);
		
		// if the requested pagenumber is already infront of queue
	    if (reqPage == queue->front){

		}
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

		if( clock_gettime( CLOCK_MONOTONIC, &stop) == -1 ) {
			perror( "clock gettime" );
		return EXIT_FAILURE;
		}
		pthread_spin_unlock(&LRU_lock);
		
		// if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) {
		// 	perror( "clock gettime" );
		// return EXIT_FAILURE;
		// }
		// accum = ( stop.tv_sec - start.tv_sec )+ (double)( stop.tv_nsec - start.tv_nsec )/ (double)BILLION;
		accum = ((double)( stop.tv_sec - start.tv_sec )*(double)BILLION) + (double)( stop.tv_nsec - start.tv_nsec ); //nano

    	printf("> Time taken b/w lock to *ACCESS* page number: %d is %lf\n",queue->front->pageNumber, accum);
}
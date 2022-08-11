#include "LRU.h"

Queue* q;
Hash* hash;

void DisplayCache(Queue* Q) {
    printf("\n\n****Final Cache****\n\n");
    
    if (isQueueEmpty(Q)) {
        printf("Cache is empty\n");
    }
    else {
        QNode* temp = Q->front;
        while(temp) {
            printf("Key: %d, Address: %p, Previous: %p, Next: %p\n", temp->key,temp, temp->prev, temp->next);
            temp = temp->next;
        }
    }
}

void *thread_fun(void *ThreadID){
    
    long tid = (long)ThreadID;
    unsigned int random;

    for (int i= 0; i < rand() % 4; i++) {
        printf("\n********** Using thread: %ld *************\n", tid);
        random = (rand() % (1000 - 5 + 1)) + 5;
        printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, random);

        pthread_spin_lock(&hashtbl_lock);
        QNode* req_page = hash->array[random];  

        if (req_page == NULL) {
            pthread_spin_unlock(&hashtbl_lock); 
            
            req_page = allocate_node(q, random, tid);  
            
            // Add page entry to hash 
            pthread_spin_lock(&hashtbl_lock);

            if (hash->array[random] == NULL) {
                hash->array[random] = req_page; 
            }
            // else {
            //     free_node(q,req_page,random);
            // }
        }
        pthread_spin_unlock(&hashtbl_lock); 

        while (access_node(q,req_page,random, tid) == FAIL) {
            req_page = allocate_node(q, random, tid);
            // Add page entry to hash
            pthread_spin_lock(&hashtbl_lock);

            if (hash->array[random] == NULL) {
                hash->array[random] = req_page; 
            }
            // else {
            //     free_node(q,req_page,random);
            // }
            pthread_spin_unlock(&hashtbl_lock);
        }

        // sleep(pow(5.0, -3));
        access_done(q,req_page, tid); 
    }
        
    printf("\n x--------- Thread: %ld job done-------------x \n", tid);   
    return NULL;
}

int main() {
    buffer_pool();
    print_buffer_pool();

    pthread_t threads[THREADS];
    int rc;
    q = createQueue();
    hash = createHash(HASHSPACE);

    if (pthread_spin_init(&buffer_lock, 0) != 0) {
        printf("\n spin buffer has failed\n");
        return 1;
    }

    if (pthread_spin_init(&hashtbl_lock, 0) != 0) {
        printf("\n spin hashtbl has failed\n");
        return 1;
    }

    if (pthread_spin_init(&LRU_lock, 0) != 0) {
        printf("\n spin LRU has failed\n");
        return 1;
    }

   for( int i = 0; i < THREADS; i++ ) {
        printf("\n main() : creating thread: %d \n",i);
        rc = pthread_create(&threads[i], 0, thread_fun, (void *) (intptr_t) i);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(-1);
      }
   } 

    for( int i = 0; i < THREADS; i++ ) {
        pthread_join(threads[i], NULL);
    }

    pthread_spin_destroy(&buffer_lock);
    pthread_spin_destroy(&hashtbl_lock);
    pthread_spin_destroy(&LRU_lock);

    // free
    int front = q->front->key;
    int middle = q->rear->prev->key;
    int last = q->rear->key;
    // free front node
    QNode* del_page_front = hash->array[front];
    printf("del_page_front : %p key: %d\n",del_page_front, del_page_front->key);
    free_node(q,del_page_front,front);
    hash->array[front] = NULL;

    // free middle node
    QNode* del_page_middle = hash->array[middle];
    printf("del_page_middle : %p key: %d\n",del_page_middle, del_page_middle->key);
    free_node(q,del_page_middle,middle);
    hash->array[middle] = NULL;

    // free last node
    QNode* del_page_last = hash->array[last];
    printf("del_page_last : %p key: %d\n",del_page_last, del_page_last->key);
    free_node(q,del_page_last,last);
    hash->array[last] = NULL;

	allocation_time_avg = allocation_time_avg/total_allocations;
    access_time_avg = access_time_avg/total_access;
    access_done_time_avg = access_done_time_avg/total_access_done;
	printf("\n Total number of allocation calls: %d, Average time taken to allocate : %d\n",total_allocations, allocation_time_avg);
    printf("\n Total number of access calls: %d, Average time taken to access : %d\n",total_access, access_time_avg);
    printf("\n Total number of access_done calls: %d, Average time taken for access_done : %d\n",total_access_done ,access_done_time_avg);
    return SUCCESS;
}

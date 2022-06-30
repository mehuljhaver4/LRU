#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "locks.h"

Queue* q;
Hash* hash;


void *thread_fun(void *ThreadID){
    
    long tid = (long)ThreadID;
    unsigned int random;

    printf("\n********** Using thread: %ld *************\n", tid);
    int rand_i = rand()%5;
    
    for(int i = 0; i< rand_i; i++){
        random = (rand()%(5 - 1 + 1)) + 1;
        printf("\n >>>>>> Thread: %ld, Inserting page number: %d <<<<<<<<< \n",tid, random);
        ReferencePage(q, hash, random);
        sleep(pow(15.0, -9));  //sleeping for 15 nano seconds
    }
    printf("\n--------- Thread: %ld job done-------------\n", tid);    
    return NULL;
}

void PrintResult(Queue* Q, Hash* H){
	printf("%d ", Q->front->pageNumber);
    while(Q->front->next!= NULL){
        printf("%d ", Q->front->next->pageNumber);
        Q->front->next = Q->front->next->next;
        }  
    }

int main()
{
   pthread_t threads[6];
   int rc;
   int i;

    q = createQueue(CACHESPACE);
    hash = createHash(HASHSPACE);

    if (pthread_spin_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }
   for( i = 0; i < 6; i++ ) {
        printf("\n main() : creating thread: %d \n",i);
        rc = pthread_create(&threads[i], NULL, thread_fun, (void *)i);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            exit(-1);
      }
   } 

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    pthread_join(threads[2], NULL);
    pthread_join(threads[3], NULL);
    pthread_join(threads[4], NULL);
    pthread_join(threads[5], NULL);
   
    printf("\n\n****Final cache:****\n\n ");
    PrintResult(q,hash);
    pthread_spin_destroy(&lock);
    return 0;
}
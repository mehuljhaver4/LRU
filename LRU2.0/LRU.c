#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include "utils.h"

Queue* q;
Hash* hash;

void print_buffer_pool(buffer_node *root) {
    printf("\n*****Block start: %p ****\n", buffer_pool_ptr);
    buffer_node *temp = root;
    int buf_counter = buffer_count;
    while(buf_counter){
        printf("\nBuffer %d: %p\n",buf_counter,temp);
        buf_counter--;
        temp = temp->next;
         }
    printf("NULL\n");
}

void DisplayCache(Queue* Q, long tid) {
    printf("\n\n****Final Cache (ThreadID: %ld)****\n\n",tid);
    
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

    printf("\n********** Using thread: %ld *************\n", tid);
    

    random = (rand()%(5)) + 1; 
    printf("\n >>> Thread: %ld, Inserting page number: %d \n",tid, random);

    pthread_spin_lock(&hashtbl_lock);
    QNode* reqPage = hash->array[random];  

    if (reqPage == NULL) {
        pthread_spin_unlock(&hashtbl_lock); 
        
        reqPage = allocate_node(q, hash, random, tid);  
        
        // Add page entry to hash 
        pthread_spin_lock(&hashtbl_lock);

        if (hash->array[random] == NULL)
            hash->array[random] = reqPage; 
        else
            free_node(q,reqPage,random, tid);
        
        pthread_spin_unlock(&hashtbl_lock);
    }

    if (access_node(q,hash,reqPage,random, tid)== -1) {
        reqPage = allocate_node(q, hash, random, tid);
        
        // Add page entry to hash
        pthread_spin_lock(&hashtbl_lock);

        if (hash->array[random] == NULL)
            hash->array[random] = reqPage; 
        else
            free_node(q,reqPage,random, tid);

        pthread_spin_unlock(&hashtbl_lock);
        access_node(q,hash,reqPage,random, tid);
    }
                
    // sleep(pow(5.0, -3));  //sleeping for 5 milliseconds
    access_done(q,hash,reqPage, tid); 
    DisplayCache(q, tid);
        
    printf("\n x--------- Thread: %ld job done-------------x \n", tid);   
    return NULL;
}

int main() {
    buffer_pool();
    print_buffer_pool(root);

    pthread_t threads[THREADS];
    int rc;
    q = createQueue();
    hash = createHash(HASHSPACE);

    if (pthread_spin_init(&buffer_lock, 0) != 0) {
        printf("\n spin buffer has failed\n");
        return 1;
    }

    if (pthread_spin_init(&hashtbl_lock, 0) != 0) {
        printf("\n spin hastbl has failed\n");
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

    for( int i = 0; i < THREADS; i++ )
        pthread_join(threads[i], NULL);
    
    pthread_spin_destroy(&buffer_lock);
    pthread_spin_destroy(&hashtbl_lock);
    // QNode* temp = NULL;
    // pthread_spin_destroy(&temp->node_lock);

     print_buffer_pool(root);
    return SUCCESS;
}
 
 
    // for (int i = 0; i<=19; i++) {
    //     QNode* temp =  allocate_node(q,hash,i);
    //     hash->array[i] = temp;
    //     printf("new allocation : %p key: %d\n",temp, temp->key);
    //     access_done(q,hash,temp);
    // }

    // // check the hash
    // for (int i=0; i<HASHSPACE; i++)
    //     printf("hash->array[%d]: %p \n",i, hash->array[i]);


    // // access 9
    // QNode* acc_9 = hash->array[9];
    // printf("\nAccessing page 9: %p key: %d\n",acc_9 ,acc_9->key);
    // if (access_node(q,hash,acc_9,9)== -1) {
	// 	acc_9 = allocate_node(q, hash, 9);
	// 	// add to hash
	// 	hash->array[9] = acc_9;
    // }
    // access_done(q,hash,acc_9);

    // DisplayCache(q);


    // free 1
    // QNode* del_page = hash->array[1];
    // printf("del_page : %p key: %d\n",del_page, del_page->key);
    // free_node(q,del_page,1);
    // hash->array[1] = NULL;
    // DisplayCache(q);



    // // check the hash
    // for (int i=0; i<HASHSPACE; i++)
    //     printf("hash->array[%d]: %p \n",i, hash->array[i]);

    // DisplayCache(q);
//     print_buffer_pool(root);
//     return SUCCESS;
// }
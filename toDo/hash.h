#ifndef HASH_
#define HASH_

#include "utils.h"

// A hash (Collection of pointers to Queue Nodes)
// typedef struct Hash {
// 	int capacity; // how many pages can be there
// 	QNode** array; // an array of queue nodes
// } Hash;

// A utility function to create an empty Hash of given capacity
Hash* createHash(int capacity);

#endif
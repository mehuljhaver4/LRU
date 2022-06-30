#include "utils.h"
#include "hash.h"

// A hash (Collection of pointers to Queue Nodes)
typedef struct Hash {
	int capacity; // how many pages can be there
	QNode** array; // an array of queue nodes
} Hash;

Hash* hash;

// A utility function to create an empty Hash of given capacity
Hash* createHash(int capacity)
{
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
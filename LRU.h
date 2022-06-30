#ifdef LRU_DOT_H
#define LRU_DOT_H

QNode* newQNode(Queue* queue, Hash* hash, unsigned pageNumber);
QNode* deQueue(Queue* queue);
QNode* allocate_node(Queue* queue, Hash* hash, unsigned pageNumber);
void access_node(Queue* queue, Hash* hash, QNode* reqPage);

#endif /*LRU_DOT_H*/
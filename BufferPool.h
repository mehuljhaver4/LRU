#ifndef BUFFERPOOL_DOT_H
#define BUFFERPOOL_DOT_H

buffer_node* append_buffer(buffer_node **ref_node, buffer_node **root, int size);

int buffer_pool();

QNode* get_buffer(buffer_node* root, Queue* queue, Hash* hash);

#endif /*BUFFERPOOL_DOT_H*/
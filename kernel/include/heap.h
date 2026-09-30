#ifndef HEAP_H
#define HEAP_H

#include "types.h"

void *kmalloc(uint64_t size);
void kfree(void *ptr);

#endif
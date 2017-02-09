#include <stdio.h>
#include <sys/time.h>

#include <netinet/in.h>

typedef int data_t;

void ring_init();
void ring_enqueue(data_t data);
data_t ring_dequeue();

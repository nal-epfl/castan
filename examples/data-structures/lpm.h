#include <stdio.h>
#include <sys/time.h>

#include <netinet/in.h>

typedef unsigned int data_t;

void init_lpm();
void set_prefix_data(struct in_addr *ip, int prefix_len, data_t data);
data_t get_ip_data(struct in_addr *ip);

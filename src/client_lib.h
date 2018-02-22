#ifndef H_CLIENT_LIB
#define H_CLIENT_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#include <stropts.h>
#include <unistd.h>
#include <fcntl.h>
#include <resolv.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <arpa/inet.h>

#define MSG_SIZE 80

struct packet
{
    struct iphdr myip;
    struct icmphdr myicmp;
    char msg[MSG_SIZE];
};

struct thread_info {
    pthread_t thread_id;
    struct sockaddr_in addr;
};

const char msg[] = "I'm a badass DDoSer!\n";

#endif //H_CLIENT_LIB

#ifndef _CONNECT_INFO_H_
#define _CONNECT_INFO_H_

#define MAX_PACKET_PAYLOAD 1024

#include "list/list.h"
#include <semaphore.h>
#include <stdbool.h>
#include <netdb.h>
#include <pthread.h>

struct connectInfo {
    int* socket_descriptor;
    struct addrinfo* p;
    struct addrinfo* hostReceive;
    List* list;
    sem_t* listLock;
    sem_t* listEmpty;
    sem_t* listFull;
    pthread_mutex_t* mut;
    bool* isShutdown;
    pthread_cond_t* cond;
};

#endif 
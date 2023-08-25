#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "connectInfo.h"
#include "list/list.h"

void* send_packets(void* connectInfo) {
    bool threadShutdown = false;
    struct connectInfo* package = (struct connectInfo*)connectInfo;

    while(!threadShutdown) {
        
        sem_wait(package->listFull);
        sem_wait(package->listLock);
        char* send = (char*)List_trim(package->list);
        sem_post(package->listLock);
        sem_post(package->listEmpty);

        if (send != NULL) {
            char check = send[0];
            if (check == '!' && strlen(send) == 2) {
                threadShutdown = true;
            }
            // break up send into smaller packets
            // while UDP limit is theoretically 65535 bytes (including header) it is more pragmatic
            // to use a smaller packet size
            size_t total = strlen(send);
            unsigned int iterations = total / MAX_PACKET_PAYLOAD;
            char * worker = send; 
            for (int i = 0; i < iterations; i++) {
                sendto(*(package->socket_descriptor), worker, MAX_PACKET_PAYLOAD, 0, (package->p)->ai_addr, (package->p)->ai_addrlen);
                //TODO: adjust offset if necessary
                worker += MAX_PACKET_PAYLOAD;
            }
            sendto(*(package->socket_descriptor), worker, strlen(worker), 0, (package->p)->ai_addr, (package->p)->ai_addrlen);
            free(send);
        }
    }
    //write(STDOUT_FILENO, "s_p about to exit\n", 19);

    pthread_mutex_lock(package->mut);
    *(package->isShutdown) = true;
    pthread_cond_signal(package->cond);
    pthread_mutex_unlock(package->mut);

    //send a packet to the receive socket on the same machine
    sendto(*(package->socket_descriptor), "!\n", 2, 0, (package->hostReceive)->ai_addr, (package->hostReceive)->ai_addrlen);

    pthread_exit(NULL);
}
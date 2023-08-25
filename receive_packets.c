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

void* receive_packets(void* connectInfo) {
    bool threadShutdown = false;
    struct connectInfo* package = (struct connectInfo*)connectInfo;
    int bytesRead;
    char receiveBuffer[MAX_PACKET_PAYLOAD+1];
    
    while (!threadShutdown) {
        bytesRead = recvfrom(*(package->socket_descriptor), receiveBuffer, MAX_PACKET_PAYLOAD, 0, NULL, NULL);
        if (bytesRead < 0) {
            threadShutdown = true;
        }
        receiveBuffer[bytesRead] = '\0';
        if (receiveBuffer[0] == '!' && strlen(receiveBuffer) == 2) {
            threadShutdown = true;
        }
        char* sharedReceive = (char*)malloc(strlen(receiveBuffer)+1);
        memset(sharedReceive, 0, strlen(receiveBuffer) + 1);
        strcpy(sharedReceive, receiveBuffer);

        sem_wait(package->listEmpty);
        sem_wait(package->listLock);
        List_prepend(package->list, sharedReceive);
        sem_post(package->listLock);
        sem_post(package->listFull);

    }
    //write(STDOUT_FILENO, "r_p about to exit\n", 19);

    pthread_mutex_lock(package->mut);
    *(package->isShutdown) = true;
    pthread_cond_signal(package->cond);
    pthread_mutex_unlock(package->mut);

    pthread_exit(NULL);
}
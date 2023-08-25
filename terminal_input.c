#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "connectInfo.h"
#include "list/list.h"

void* terminal_input(void* connectInfo) {
    struct connectInfo* package = (struct connectInfo*)connectInfo;
    bool threadShutdown = false;
    char inputBuffer[MAX_PACKET_PAYLOAD+1];
    int bytesread;  

    while(!threadShutdown){
        bytesread = read(STDIN_FILENO, inputBuffer, MAX_PACKET_PAYLOAD);

        if (bytesread <= 0){
            threadShutdown = true;
        }
        inputBuffer[bytesread] = '\0';
        // length of "!\n" == 2
        if (inputBuffer[0] == '!' && strlen(inputBuffer) == 2) {
            threadShutdown = true;
        }

        sem_wait(package->listEmpty);
        
        char* sharedLocation = (char*)malloc(strlen(inputBuffer)+1);
        memset(sharedLocation, 0, strlen(inputBuffer)+1);
        strcpy(sharedLocation, inputBuffer);
        
        sem_wait(package->listLock);
        List_prepend(package->list, sharedLocation);
        sem_post(package->listLock);
        sem_post(package->listFull);
    }
    //write(STDOUT_FILENO, "t_i about to exit\n", 19);

    pthread_mutex_lock(package->mut);
    *(package->isShutdown) = true;
    pthread_cond_signal(package->cond);
    pthread_mutex_unlock(package->mut);

    pthread_exit(NULL);
}
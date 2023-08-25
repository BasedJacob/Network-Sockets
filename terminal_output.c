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
#include <stdio.h>

#include "connectInfo.h"
#include "list/list.h"

void* terminal_output(void* connectInfo) {
    bool threadShutdown = false;
    struct connectInfo* package = (struct connectInfo*)connectInfo;

    while(!threadShutdown) {
        sem_wait(package->listFull);
        sem_wait(package->listLock);
        char* output = (char*)List_trim(package->list);
        sem_post(package->listLock);
        sem_post(package->listEmpty);

        if (output != NULL) {
            if (output[0] == '!' && strlen(output) == 2) {
                threadShutdown = true;
            }
            
            write(STDOUT_FILENO, output, strlen(output));
            free(output);
        }
    }
    //write(STDOUT_FILENO, "t_o about to exit\n", 19);

    pthread_mutex_lock(package->mut);
    *(package->isShutdown) = true;
    pthread_cond_signal(package->cond);
    pthread_mutex_unlock(package->mut);

    pthread_exit(NULL);
}
//see stackoverflow.com/questions/37541985/storage-size-of-addrinfo-isnt-known
#define _POSIX_C_SOURCE 200112L
// only needed for usleep
// #define _XOPEN_SOURCE 500

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h> // addrinfo
#include <sys/socket.h> //socket

#include "list/list.h"
#include "connectInfo.h"
#include "send_packets.h"
#include "terminal_output.h"
#include "receive_packets.h"
#include "terminal_input.h"

#define NUM_THREADS 4
#define NUM_SOCKETS 2

sem_t list_send_lock;
sem_t list_receive_lock;
sem_t send_full;
sem_t send_empty;
sem_t recv_full;
sem_t recv_empty;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
bool isShutdown = false;

struct connectInfo threadDataArray[NUM_THREADS];
int socket_descriptors[NUM_SOCKETS];

static void freeStrings(void* pItem) {
    char* item = NULL;
    if (pItem != NULL) {
        item = (char*)pItem;
        free(item);
    }
} 

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s HOST_PORT REMOTE_HOSTNAME REMOTE_PORT\n", argv[0]);
        exit(1);
    }

    int send_sd, receive_sd, status;
    struct addrinfo hints_send, *peerSend, *pSend;
    struct addrinfo hints_receive, *peerReceive, *pReceive;

    memset(&hints_send, 0, sizeof(hints_send));
    memset(&hints_receive, 0, sizeof(hints_receive));

    hints_send.ai_family = AF_INET;
    hints_send.ai_socktype = SOCK_DGRAM;
    hints_send.ai_flags = AI_PASSIVE;

    hints_receive.ai_family = AF_INET;
    hints_receive.ai_socktype = SOCK_DGRAM;
    hints_receive.ai_flags = AI_PASSIVE;

    status = getaddrinfo(argv[2], argv[3], &hints_send, &peerSend);
    if (status != 0) {
        const char * message = "Could not get address info for sending socket.\n";
        write(STDERR_FILENO, message, strlen(message));
        exit(1);
    }
    
    status = getaddrinfo(NULL, argv[1], &hints_receive, &peerReceive);
    if (status != 0) {
        const char* message = "Could not get address info for receiving socket.\n";
        write(STDERR_FILENO, message, strlen(message));
        exit(1);
    }

    for (pSend = peerSend; pSend != NULL; pSend = pSend->ai_next) {
        send_sd = socket(pSend->ai_family, pSend->ai_socktype, pSend->ai_protocol);
        if (send_sd == -1) continue;
        break;
    }

    if (pSend == NULL) {
        const char * message = "Could not create a valid send socket.\n";
        write(STDERR_FILENO, message, strlen(message));
        exit(2);
    }

    for (pReceive = peerReceive; pReceive != NULL; pReceive = pReceive->ai_next) {
        receive_sd = socket(pReceive->ai_family, pReceive->ai_socktype, pReceive->ai_protocol);
        if (receive_sd == -1) continue;

        status = bind(receive_sd, pReceive->ai_addr, pReceive->ai_addrlen);
        if (status == -1) {
            close(receive_sd);
            continue;
        }
        break;
    }

    if (pReceive == NULL) {
        const char* message = "Could not create and bind a valid receive socket.\n";
        write(STDERR_FILENO, message, strlen(message));
        exit(2);
    }

    socket_descriptors[0] = send_sd;
    socket_descriptors[1] = receive_sd;

    List* sendQueue = List_create();
    List* receiveQueue = List_create();

    // 3 semaphores per list (per lecture producer-consumer problem solution)
    if (sem_init(&list_send_lock, 0, 1)) {
        printf("Error initializing sender queue semaphore.\n");
        exit(1);
    }
    if (sem_init(&list_receive_lock, 0, 1)) {
        printf("Error initializing receive queue semaphore.\n");
        exit(1);
    }
    if (sem_init(&send_full, 0, 0)) {
        printf("Error initializing receive queue semaphore.\n");
        exit(1);
    }
    if (sem_init(&send_empty, 0, LIST_MAX_NUM_NODES)) {
        printf("Error initializing receive queue semaphore.\n");
        exit(1);
    }
    if (sem_init(&recv_full, 0, 0)) {
        printf("Error initializing receive queue semaphore.\n");
        exit(1);
    }
    if (sem_init(&recv_empty, 0, LIST_MAX_NUM_NODES)) {
        printf("Error initializing receive queue semaphore.\n");
        exit(1);
    }

    // Send packets
    threadDataArray[0].socket_descriptor = socket_descriptors+0;
    threadDataArray[0].p = pSend;
    threadDataArray[0].hostReceive = pReceive;
    threadDataArray[0].list = sendQueue;
    threadDataArray[0].listLock = &list_send_lock;
    threadDataArray[0].listEmpty = &send_empty;
    threadDataArray[0].listFull = &send_full;
    threadDataArray[0].mut = &mut;
    threadDataArray[0].cond = &cond;
    threadDataArray[0].isShutdown = &isShutdown;

    // Receive packets
    threadDataArray[1].socket_descriptor = socket_descriptors+1;
    threadDataArray[1].list = receiveQueue;
    threadDataArray[1].listLock = &list_receive_lock;
    threadDataArray[1].listEmpty = &recv_empty;
    threadDataArray[1].listFull = &recv_full;
    threadDataArray[1].mut = &mut;
    threadDataArray[1].cond = &cond;
    threadDataArray[1].isShutdown = &isShutdown;

    // Terminal input
    threadDataArray[2].list = sendQueue;
    threadDataArray[2].listLock = &list_send_lock;
    threadDataArray[2].listEmpty = &send_empty;
    threadDataArray[2].listFull = &send_full;
    threadDataArray[2].mut = &mut;
    threadDataArray[2].cond = &cond;
    threadDataArray[2].isShutdown = &isShutdown;

    // Terminal output
    threadDataArray[3].list = receiveQueue;
    threadDataArray[3].listLock = &list_receive_lock;
    threadDataArray[3].listEmpty = &recv_empty;
    threadDataArray[3].listFull = &recv_full;
    threadDataArray[3].mut = &mut;
    threadDataArray[3].cond = &cond;
    threadDataArray[3].isShutdown = &isShutdown;

    // Create threads
    pthread_attr_t attribute;
    pthread_attr_init(&attribute);
    pthread_attr_setdetachstate(&attribute, PTHREAD_CREATE_JOINABLE);

    pthread_t threads[NUM_THREADS];
    if (pthread_create(&threads[0], &attribute, send_packets, (void*)&threadDataArray[0])) {
        printf("Error creating send_packets thread.\n");
        exit(-1);
    }
    
    if (pthread_create(&threads[1], &attribute, receive_packets, (void*)&threadDataArray[1])) {
        printf("Error creating receive_packets thread.\n");
        exit(-1);
    }
    
    if (pthread_create(&threads[2], &attribute, terminal_input, (void*)&threadDataArray[2])) {
        printf("Error creating terminal_input thread.\n");
        exit(-1);
    }
    
    if (pthread_create(&threads[3], &attribute, terminal_output, (void*)&threadDataArray[3])) {
        printf("Error creating terminal_output thread.\n");
        exit(-1);
    }
    
    pthread_attr_destroy(&attribute);

    pthread_mutex_lock(&mut);
    while (!isShutdown) {
        pthread_cond_wait(&cond, &mut);
    }
    pthread_mutex_unlock(&mut);

    // shut down terminal_input
    pthread_cancel(threads[2]);

    // shut down send_packets
    char shutdown_send[2] = "!\n";
    char* sharedLocation = (char*)malloc(3);
    memset(sharedLocation, 0, 3);
    strcpy(sharedLocation, shutdown_send);
    
    sem_wait(&send_empty);
    sem_wait(&list_send_lock);
    List_prepend(sendQueue, sharedLocation);
    sem_post(&list_send_lock);
    sem_post(&send_full);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    freeaddrinfo(peerReceive);
    freeaddrinfo(peerSend);
    close(send_sd);
    close(receive_sd);

    void (*free_routine)(void*) = &freeStrings;
    
    sem_wait(&list_receive_lock);
    List_free(receiveQueue, free_routine);
    sem_post(&list_receive_lock);

    sem_wait(&list_send_lock);
    List_free(sendQueue, free_routine);
    sem_post(&list_send_lock);

    sem_destroy(&list_receive_lock);
    sem_destroy(&list_send_lock);
    sem_destroy(&send_empty);
    sem_destroy(&send_full);
    sem_destroy(&recv_empty);
    sem_destroy(&recv_full);

    return 0;
}
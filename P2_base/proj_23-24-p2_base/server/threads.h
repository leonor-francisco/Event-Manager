#ifndef THREADS_H
#define THREADS_H

#include <stdlib.h>
#include <pthread.h>

#include "common/constants.h"

typedef struct thread {
    pthread_t thread;
    int session_id;
}
Worker_thread;


struct Node {
    char buffer_resp[MAX_PIPE_NAME];
    char buffer_req[MAX_PIPE_NAME];
    struct Node *next;
};


struct Producer_consumer_queue {
    int event_counter;
    struct Node *head;
    struct Node *tail;
};


struct Producer_consumer_queue *initializePCQ();

void *executeCommands (void *Worker_thread);

void insertAtTail(struct Producer_consumer_queue *list, char *buffer_resp, char *buffer_req);
void deleteAtHead(struct Producer_consumer_queue *list);
void deleteQueue(struct Producer_consumer_queue *list);

int create_threads(Worker_thread *threads);


#endif
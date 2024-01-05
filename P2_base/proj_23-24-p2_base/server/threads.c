#include "threads.h"

#include <stdio.h>

#include <string.h>
#include "common/constants.h"
#include <pthread.h>

struct Producer_consumer_queue *initializePCQ() {
    struct Producer_consumer_queue *list = (struct Producer_consumer_queue *)malloc(sizeof(struct Producer_consumer_queue));
    list->head = NULL;
    list->tail = NULL;
    list->event_counter = 0;
    return list;
}


void insertAtTail(struct Producer_consumer_queue *list, char *buffer_resp, char *buffer_req) {
    struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));
    strcpy(new_node->buffer_resp, buffer_resp);
    strcpy(new_node->buffer_req, buffer_req);
    new_node->next = NULL;

    if (list->tail == NULL) {
        // If the list is empty, set both head and tail to the new node
        list->head = new_node;
        list->tail = new_node;
    } else {
        // Otherwise, update the tail and link the new node
        list->tail->next = new_node;  
        list->tail = new_node;
    }
    list->event_counter++;
}

void deleteAtHead(struct Producer_consumer_queue *list) {
    if (list->head == NULL) {
        return;
    }

    struct Node *temp = list->head;
    list->head = list->head->next;

    free(temp);
    
    // Update the tail if the list becomes empty after deletion
    if (list->head == NULL) {
        list->tail = NULL;
    }
    list->event_counter--;
}

int create_threads(Worker_thread *threads) {
    for(int i = 0; i < MAX_SESSION_COUNT; i++) {
        threads[i].session_id = i;
        if(pthread_create(&threads[i].thread, NULL, executeCommands, (void*)&threads[i]) < 0) {
            fprintf(stderr, "Failed to create thread\n");
            return 1;
        }
    }
    return 0;
}


void deleteQueue(struct Producer_consumer_queue *list) {
    while(list->head != NULL)
        deleteAtHead(list);
}
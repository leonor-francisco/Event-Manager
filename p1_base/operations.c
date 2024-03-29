#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#include "constants.h"
#include "eventlist.h"
#include <bits/pthreadtypes.h>

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
  
  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
  if (get_event_with_delay(event_id) != NULL) {
    fprintf(stderr, "Event already exists\n");
    return 1;
  }
  struct Event* event = malloc(sizeof(struct Event));
  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }
  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));
  event->res_locks = malloc(num_rows * num_cols * sizeof(pthread_rwlock_t));


  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }
  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }
  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }
  if (event->res_locks == NULL) {
    fprintf(stderr, "Error allocating memory for event res_locks\n");
    free(event);
    return 1;
  }
  if (pthread_rwlock_init(&event->lock_general, NULL) != 0) {
      fprintf(stderr,"Rwlock init has failed\n"); 
      free(event);  
      return 1; 
    }
  for (size_t i = 0; i < num_rows * num_cols; i++) {
    pthread_mutex_init(&event->res_locks[i], NULL);
  }
  return 0;
}

void bubbleSort(size_t *xs, size_t *ys, size_t num_coords) {
    for (size_t i = 0; i < num_coords - 1; i++) {
        int swapped = 0;
        for (size_t j = 0; j < num_coords - i - 1; j++) {
            // Consider x as more relevant than y
            if (xs[j] > xs[j + 1]) {
                // Swap xs
                size_t temp = xs[j];
                xs[j] = xs[j + 1];
                xs[j + 1] = temp;

                // Maintain the order in ys accordingly
                temp = ys[j];
                ys[j] = ys[j + 1];
                ys[j + 1] = temp;

                swapped = 1;
            }
            else if (xs[j] == xs[j + 1]){ //orders the vector in function of the less significant vector if the most significant is the same
              if (ys[j] > ys[j + 1]) {
                size_t temp = ys[j];
                ys[j] = ys[j + 1];
                ys[j + 1] = temp;

                temp = xs[j];
                xs[j] = xs[j + 1];
                xs[j + 1] = temp;

                swapped = 1;
              }
            }
        }

        // If no elements were swapped in the inner loop, the array is already sorted
        if (swapped == 0)
          break;
    }
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }
  pthread_rwlock_rdlock(&event->lock_general);
  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;

  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];
    pthread_mutex_lock(&event->res_locks[seat_index(event, row, col)]);
  }
  for (i = 0; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];
    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      pthread_mutex_unlock(&event->res_locks[seat_index(event, row, col)]);
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      fprintf(stderr, "Seat already reserved\n");
      pthread_mutex_unlock(&event->res_locks[seat_index(event, row, col)]);
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
    pthread_mutex_unlock(&event->res_locks[seat_index(event, row, col)]);
  }
  if (i < num_seats) {//if not all of the reservations were successful
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0; //frees the seats
    }
    pthread_rwlock_unlock(&event->lock_general);
    return 1;
  }
  pthread_rwlock_unlock(&event->lock_general);
  return 0;
}

int ems_show(unsigned int event_id, int writeFile) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }
  
  pthread_rwlock_wrlock(&event->lock_general);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));
      char buffer[256];
      sprintf(buffer, "%u", *seat);
      write(writeFile, buffer, strlen(buffer));


      if (j < event->cols) {
        write(writeFile, " ", 1);
      }
    }
    write(writeFile, "\n", 1);
  }
  pthread_rwlock_unlock(&event->lock_general);
  return 0;
}

int ems_list_events(int writeFile) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (event_list->head == NULL) {
    write(writeFile, "No events\n", strlen("No events\n"));
    return 0;
  }
  struct ListNode* current = event_list->head;
  while (current != NULL) {
    char buffer[256];
    sprintf(buffer, "Event: %u\n", (current->event)->id);
    write(writeFile, buffer, strlen(buffer));
    current = current->next;
  }

  return 0;
}

void ems_wait(unsigned int delay_ms) {
    struct timespec delay = delay_to_timespec(delay_ms);
    nanosleep(&delay, NULL);
  }
  


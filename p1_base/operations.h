#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include <stddef.h>

/// Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_ms);

/// Destroys the EMS state.
int ems_terminate();

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols);

void bubbleSort(size_t *xs, size_t *ys, size_t num_coords);

/// Swaps two elements from a vector
/// @param a pointer to element a
/// @param b pointer to element b
void swap(size_t *a, size_t *b); 

/// Sorts two vectors taking into account they are intertwined
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
void sortVectors(size_t* xs, size_t* ys, size_t numcoords);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @param writeFile File in which the output information will be shown 
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int writeFile);

/// Prints all the events.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events();

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
/// @param thread_pointer pointer to the thread which we will change
void ems_wait(unsigned int delay_ms);

#endif  // EMS_OPERATIONS_H

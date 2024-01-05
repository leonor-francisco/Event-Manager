#ifndef SERVER_OPERATIONS_H
#define SERVER_OPERATIONS_H

#include <stddef.h>

enum API {
  OP_CODE_SETUP = '1',
  OP_CODE_QUIT = '2',
  OP_CODE_CREATE = '3',
  OP_CODE_RESERVE = '4',
  OP_CODE_SHOW = '5',
  OP_CODE_LIST = '6'
};

/// Initializes the EMS state.
/// @param delay_us Delay in microseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_us);

/// Destroys the EMS state.
int ems_terminate();

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys);

/// Prints the given event.
/// @param out_fd File descriptor to print the event to.
/// @param event_id Id of the event to print.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, char **sec_buffer);

/// Prints all the events.
/// @param out_fd File descriptor to print the events to.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(char **sec_buffer);

int sig_show();



#endif  // SERVER_OPERATIONS_H

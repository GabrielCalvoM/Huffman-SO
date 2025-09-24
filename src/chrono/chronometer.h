#ifndef CHRONOMETER_H
#define CHRONOMETER_H

#include <time.h>

// Function declarations
extern inline double get_time_taken();
void start_chronometer(void);
void stop_chronometer(const char* operation);

#endif // CHRONOMETER_H

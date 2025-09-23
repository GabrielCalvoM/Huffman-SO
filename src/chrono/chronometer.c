////////////////////////////////////////////////////////////////////////
// Chrono Tracker

#include "chronometer.h"
#include <stdio.h>
#include <time.h>

clock_t start_time, end_time;

void start_chronometer() {
    start_time = clock();
}

void stop_chronometer(const char* operation) {
    end_time = clock();
    double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("\n=== CHRONOMETER ===\n");
    printf("%s completed in: %.6f seconds\n", operation, time_taken);
    printf("===================\n\n");
}

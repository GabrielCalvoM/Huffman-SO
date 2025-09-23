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
    double time_taken = ((double)(end_time - start_time)) * 1000.0 / CLOCKS_PER_SEC;
    printf("\n");
    printf("\n=== CHRONOMETER ===\n");
    printf("%s completed in: %.6f ms\n", operation, time_taken);
    printf("===================\n\n");
	printf("\n");
}

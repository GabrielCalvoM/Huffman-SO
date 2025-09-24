////////////////////////////////////////////////////////////////////////
// Chrono Tracker

#include "chronometer.h"
#include <stdio.h>
#include <time.h>

double start_time, end_time;

static inline double get_time(struct timespec tp) {
    return tp.tv_sec * 1000.0 + tp.tv_nsec / 1e6;
}

inline double get_time_taken() {
    return end_time - start_time;
}

void start_chronometer() {
    struct timespec start_tp;
    clock_gettime(CLOCK_MONOTONIC, &start_tp);
    start_time = get_time(start_tp);
}

void stop_chronometer(const char* operation) {
    struct timespec end_tp;
    clock_gettime(CLOCK_MONOTONIC, &end_tp);
    end_time = get_time(end_tp);
    
    double time_taken = get_time_taken();
    printf("\n");
    printf("\n=== CHRONOMETER ===\n");
    printf("%s completed in: %.6f ms\n", operation, time_taken);
    printf("===================\n\n");
	printf("\n");
}

#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H

typedef enum {
    SEQUENTIAL,
    CONCURRENT,
    PARALLEL
} program_mode_t;

int dir_is_valid(const char *dir_path); // XD
void construct_dir(const char *dir_path); // XD

#endif // PATH_MANAGER_H
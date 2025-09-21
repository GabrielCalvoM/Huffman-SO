#include <dirent.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "path_manager.h"

int dir_is_valid(const char *dir_path) {
    struct stat st;

    if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }

    return 0;
}

void construct_dir(const char *dir_path) {
    if (dir_is_valid(dir_path)) {
        return;
    }
    
    char *path = calloc(strlen(dir_path) + 1, 1);
    strcpy(path, dir_path);

    construct_dir(dirname(path));
    mkdir(dir_path, 0755);

    free(path);
}
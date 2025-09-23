#ifndef HUFFMAN_ENCODE_H
#define HUFFMAN_ENCODE_H

#include <pthread.h>

#include "path_manager.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif // BUFFER_SIZE

typedef struct {
    void (*file_func) (char const *, int);
    char *dir_path;
    int file_pos;
} thread_encode_args_t;

void compress_dir(const char *dir_path, const char *output_path);
void compress_single_file(const char *file_path, const char *output_path);
void scan_dir_characters(const char *path);
void scan_single_file_characters(const char *file_path);
int encode_main(int argc, char *argv[]);

#endif // HUFFMAN_ENCODE_H

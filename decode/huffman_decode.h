#ifndef HUFFMAN_DECODE_H
#define HUFFMAN_DECODE_H

#include <pthread.h>

#include "path_manager.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif // BUFFER_SIZE

typedef struct {
    FILE *file;
    char *dir_path;
    int file_pos;
} thread_decode_args_t;

void scan_huff(const char *file_path);
void decompress_dir(const char *compressed_path, const char *dir_path);
int decode_main(int argc, char *argv[]);

#endif // HUFFMAN_DECODE_H
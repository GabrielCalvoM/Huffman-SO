#ifndef HUFFMAN_ENCODE_H
#define HUFFMAN_ENCODE_H

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif // BUFFER_SIZE

void scan_dir_characters(const char *path);
void compress_dir(const char *dir_path);

#endif // HUFFMAN_ENCODE_H
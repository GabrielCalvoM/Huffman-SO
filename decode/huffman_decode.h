#ifndef HUFFMAN_DECODE_H
#define HUFFMAN_DECODE_H

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif // BUFFER_SIZE

void scan_huff(const char *file_path);
void decompress_dir(const char *compressed_path, const char *dir_path);

#endif // HUFFMAN_DECODE_H
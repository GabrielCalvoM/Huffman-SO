#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman_encode.h"
#include "huffman_decode.h"

int encode_main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("ERROR: not sufficient arguments");
        return 0;
    }
    
    scan_dir_characters(argv[2]);
    compress_dir(argv[2]);
}

int decode_main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("ERROR: not sufficient arguments");
        return 0;
    }

    char *file_path = calloc(1, strlen(argv[2]) + 3);
    char *dir_path = calloc(1, strlen(argv[3]) + 3);
    sprintf(file_path, "./%s", argv[2]);
    sprintf(dir_path, "./%s", argv[3]);
    
    scan_huff(file_path);
    decompress_dir(file_path, dir_path);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <huffman_flag> <parameters>\n\n", argv[0]);
        printf("Flags:\n");
        printf("  - Encode: %s -e <dir_path>", argv[0]);
        printf("  - Encode: %s -d <compressed_huff_file> <destination_dir_path>", argv[0]);
        return 0;
    }

    int encode_flag = strcmp(argv[1], "-e");
    int decode_flag = strcmp(argv[1], "-d");

    if (encode_flag == 0) return encode_main(argc, argv);
    if (decode_flag == 0) return decode_main(argc, argv);

    printf("ERROR: %s is not a flag", argv[1]);

    return 0;
}
#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utf8proc.h>

#include "cnvchar.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
int files_number;

#endif // BUFFER_SIZE

int start_files = 0;
char_freq_t *tree, *tree_pointer;

void decompress_file(FILE*, const char*);
void decompress_char(char, char[], int*, int*, int);

void scan_huff(const char *file_path) {
    FILE *file = fopen(file_path, "rb");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    fread(&code_list.size, sizeof(int), 1, file);
    start_files += sizeof(int);

    char buffer[BUFFER_SIZE] = {'\0'};
    utf8proc_uint8_t *utf8_buffer = (utf8proc_uint8_t*) buffer;

    code_list.head = calloc(sizeof(char_code_t), code_list.size);
    char_code_t* head = code_list.head;
    
    fread(buffer, 1, BUFFER_SIZE, file);
    
    for (int i = 0; i < code_list.size; i++) {
        int utf8_codepoint = 0, size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        utf8_buffer += size;
        start_files += size;
        head[i].character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, head[i].character);
        }
        
        char code[20] = {0};
        int c = 0;
        
        do {
            code[c] = (char)*utf8_buffer;
            c++;
        } while (*utf8_buffer++ != '\0');

        start_files += c;

        head[i].code = calloc(1, strlen(code) + 1);
        strcpy(head[i].code, code);
        
        if (buffer + BUFFER_SIZE - (char*)utf8_buffer < (BUFFER_SIZE / 10)) {
            fseek(file, start_files, SEEK_SET);
            memset(buffer, 0, BUFFER_SIZE);
            utf8_buffer = (utf8proc_uint8_t*) buffer;
        }
    }
    
    tree = restore_tree(code_list);
    fclose(file);
}

void decompress_dir(const char *compressed_path, const char *dir_path) {
    FILE *file = fopen(compressed_path, "rb");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    fseek(file, start_files, SEEK_SET);

    int files_number;
    fread(&files_number, sizeof(files_number), 1, file);

    mkdir(dir_path, 0755);
    for (int i = 0; i < files_number; i++) {
        decompress_file(file, dir_path);
    }
}

void decompress_file(FILE *compressed_file, const char *dir_path) {
    char filename[100] = {0}, buffer[BUFFER_SIZE] = {0}, decompressed_buffer[BUFFER_SIZE] = {0};
    int filename_bits = 0, file_bits = 0, decompressed_bits = 0, index = 0;
    
    fread(&filename_bits, sizeof(filename_bits), 1, compressed_file);
    fread(&file_bits, sizeof(file_bits), 1, compressed_file);
    tree_pointer = tree;
    
    do {
        int filename_bytes = filename_bits - decompressed_bits > BUFFER_SIZE * 8
                             ? BUFFER_SIZE
                             : ceil(filename_bits / 8.0);

        fread(buffer, 1, filename_bytes, compressed_file);

        for (int i = 0; i < filename_bytes && decompressed_bits < filename_bits; i++) {
            decompress_char(buffer[i], filename, &index, &decompressed_bits, filename_bits);
        }    
    } while (decompressed_bits < filename_bits);

    sprintf(decompressed_buffer, "%s/%s", dir_path, filename);
    
    FILE *file = fopen(decompressed_buffer, "w");

    memset(buffer, 0, BUFFER_SIZE);
    memset(decompressed_buffer, 0, BUFFER_SIZE);

    tree_pointer = tree;
    decompressed_bits = 0;
    index = 0;

    do {
        int file_bytes = file_bits - decompressed_bits > BUFFER_SIZE * 8
                             ? BUFFER_SIZE
                             : ceil(file_bits / 8.0);

        fread(buffer, 1, file_bytes, compressed_file);

        for (int i = 0; i < file_bytes && decompressed_bits < file_bits; i++) {
            decompress_char(buffer[i], decompressed_buffer, &index, &decompressed_bits, file_bits);

            if (BUFFER_SIZE - index > BUFFER_SIZE / 10) continue;

            fprintf(file,"%s", decompressed_buffer);
            memset(decompressed_buffer, 0, BUFFER_SIZE);
            index = 0;
        }
        
        memset(buffer, 0, BUFFER_SIZE);
    } while (decompressed_bits < file_bits);
    
    fprintf(file, "%s", decompressed_buffer);
    fclose(file);
}

void decompress_char(char character, char buffer[], int *index, int *decompressed_bits, int bits_bound) {
    for (int i = 0; i < 8 && *decompressed_bits < bits_bound; i++) {
        char mask = 0x80 >> i;
        tree_pointer = (character & mask) == 0 ? tree_pointer->left : tree_pointer->right;
        (*decompressed_bits)++;
        
        if (tree_pointer->is_char == 0) continue;
        
        strcat(buffer, tree_pointer->character);
        *index += strlen(tree_pointer->character);
        tree_pointer = tree;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <compressed_file_path> <dir_path>\n", argv[0]);
        return 1;
    }

    char *file_path = calloc(1, strlen(argv[1]) + 3);
    char *dir_path = calloc(1, strlen(argv[2]) + 3);
    sprintf(file_path, "./%s", argv[1]);
    sprintf(dir_path, "./%s", argv[2]);
    
    scan_huff(file_path);
    decompress_dir(file_path, dir_path);
    
    return 0;
}

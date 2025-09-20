#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

#include "cnvchar.h"

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
int files_number;

#endif // BUFFER_SIZE


void scan_file_characters(const char*, va_list);
void scan_str(char buffer[], int *start_buffer);
void compress_file(const char*, va_list);
void encode_str(char[], int*, FILE*, unsigned char[], int*, int*);
void record_encoded_str(const char*, FILE*, unsigned char*, int*, int*);

void dir_iterate(void (*file_func) (char const *, va_list args), const char *dir_path, ...) {
    struct dirent *file;
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("opendir");
        return;
    }
    
    char *file_path;
    va_list args;
    va_start(args, dir_path);

    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        file_path = calloc(100, 1);
        strcpy(file_path, dir_path);
        strcat(file_path, "/");
        strcat(file_path, file->d_name);

        va_list args_copy;
        va_copy(args_copy, args);
        file_func(file_path, args_copy);
        va_end(args_copy);
    }
    
    va_end(args);
    closedir(dir);
}

void scan_dir_characters(const char *path) {
    dir_iterate(scan_file_characters, path);
    char_freq_t *tree = construct_tree(char_list);
    code_tree(tree, &code_list);
}

void scan_file_characters(const char *file_path, va_list args) {
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    files_number++;
    char buffer[BUFFER_SIZE], *filename = calloc(strlen(file_path) + 1, 1);
    int start_buffer = 0;
    
    strcpy(filename, file_path);
    strcpy(buffer, basename(filename));
            
    scan_str(buffer, &start_buffer);

    while (fgets(buffer + start_buffer, BUFFER_SIZE - start_buffer, file)) {
        scan_str(buffer, &start_buffer);
    }

    fclose(file);
}

void scan_str(char buffer[], int *start_buffer) {
    utf8proc_uint8_t *utf8_buffer = (utf8proc_uint8_t*) buffer;
    int utf8_codepoint = 0;
        
    while ((char *)utf8_buffer < buffer + BUFFER_SIZE && utf8_buffer[0] != '\0') {
        int size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        if (utf8_codepoint < 0) {
            *start_buffer = buffer + BUFFER_SIZE - (char *)utf8_buffer;
            strcpy(buffer, (char *)utf8_buffer);
            break;
        }
        
        utf8_buffer += size;
        char *utf8_character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, utf8_character);
            add_character(&char_list, utf8_character);
        }
    }
}

void compress_dir(const char *dir_path) {
    char compressed_file_name[100];
    sprintf(compressed_file_name, "./%s.huff", dir_path);
    FILE *compressed_file = fopen(compressed_file_name, "wb");

    if (compressed_file == NULL) {
        perror("fopen");
        return;
    }

    fwrite(&code_list.size, sizeof(code_list.size), 1, compressed_file);

    for (int i = 0; i < code_list.size; i++) {
        fwrite(code_list.head[i].character, 1, strlen(code_list.head[i].character), compressed_file);
        fwrite(code_list.head[i].code, 1, strlen(code_list.head[i].code) + 1, compressed_file);
    }

    fwrite(&files_number, sizeof(files_number), 1, compressed_file);

    dir_iterate(compress_file, dir_path, compressed_file);

    fclose(compressed_file);
}

void compress_file(const char *file_path, va_list args) {
    FILE *compressed_file = va_arg(args, FILE*);
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    char buffer[BUFFER_SIZE] = {0}, *filename = calloc(strlen(file_path) + 1, 1);
    unsigned char compressed_buffer[BUFFER_SIZE] = {0};
    int start_buffer = 0, compressed_bits = 0, filename_bit_count = 0, file_bit_count = 0;
    
    strcpy(filename, file_path);
    strcpy(buffer, basename(filename));
    fwrite(&filename_bit_count, sizeof(filename_bit_count), 1, compressed_file);
    fwrite(&file_bit_count, sizeof(file_bit_count), 1, compressed_file);
            
    encode_str(buffer, &start_buffer, compressed_file, compressed_buffer, &compressed_bits, &filename_bit_count);

    if (compressed_bits > 0) fwrite(compressed_buffer, 1, strlen(compressed_buffer), compressed_file);
    memset(buffer, 0, BUFFER_SIZE);
    memset(compressed_buffer, 0, BUFFER_SIZE);

    start_buffer = 0;
    compressed_bits = 0;

    while (fgets(buffer, BUFFER_SIZE - start_buffer, file)) {
        encode_str(buffer, &start_buffer, compressed_file, compressed_buffer, &compressed_bits, &file_bit_count);
        memset(buffer, 0, BUFFER_SIZE);
    }

    if (compressed_bits > 0) fwrite(compressed_buffer, 1, strlen(compressed_buffer), compressed_file);

    fseek(compressed_file, -(ceil(filename_bit_count / 8.0) + ceil(file_bit_count / 8.0) + sizeof(int) * 2), SEEK_CUR);
    fwrite(&filename_bit_count, sizeof(filename_bit_count), 1, compressed_file);
    fwrite(&file_bit_count, sizeof(filename_bit_count), 1, compressed_file);
    fseek(compressed_file, 0, SEEK_END);

    fclose(file);
}

void encode_str(char buffer[], int *start_buffer, FILE *file, unsigned char compressed_buffer[], int *encoded_bits, int *bit_count) {
    utf8proc_uint8_t *utf8_buffer;
    int utf8_codepoint = 0;

    utf8_buffer = (utf8proc_uint8_t*) buffer;
        
    while ((char *)utf8_buffer < buffer + BUFFER_SIZE && utf8_buffer[0] != '\0') {
        int size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        if (utf8_codepoint < 0) {
            *start_buffer = buffer + BUFFER_SIZE - (char *)utf8_buffer;
            strcpy(buffer, (char *)utf8_buffer);
            break;
        }
        
        utf8_buffer += size;
        char *utf8_character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, utf8_character);
            char *character_code = search_code(&code_list, utf8_character);
            record_encoded_str(character_code, file, compressed_buffer, encoded_bits, bit_count);
        }
    }
}

void record_encoded_str(const char *str, FILE *file, unsigned char buffer[], int *recorded_bits, int *count) {
    int bit_pointer = *recorded_bits % 8, buffer_bytes = *recorded_bits / 8;
    unsigned char char_buffer = 0x00;

    for (int i = 0; i < strlen(str); i++) {
        char_buffer <<= 1;
        bit_pointer++;
        (*recorded_bits)++;
        (*count)++;

        if (str[i] == '1') char_buffer++;
        
        if (bit_pointer >= 8) {
            buffer[buffer_bytes] |= char_buffer;
            buffer_bytes++;
            char_buffer = 0x00;
            bit_pointer = 0;
        }
        
        if (buffer_bytes >= BUFFER_SIZE) {
            fwrite(buffer, 1, buffer_bytes, file);
            memset(buffer, 0, BUFFER_SIZE);
            buffer_bytes = 0;
            *recorded_bits = 0;
        }
    }

    if (bit_pointer > 0) {
        char_buffer <<= (8 - bit_pointer);
        buffer[buffer_bytes] |= char_buffer;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }
    
    scan_dir_characters(argv[1]);
    compress_dir(argv[1]);
    
    return 0;
}

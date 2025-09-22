#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <utf8proc.h>

#include "cnvchar.h"
#include "huffman_decode.h"
#include "path_manager.h"

#define MAX_COUNTER 100

int start_files = 0, compressed_files_number;
long *file_pointers;                            // byte del archivo donde comienza la lectura de cada archivo
char_freq_t *tree, *tree_pointer;
// tree = arbol de codigos
// tree_pointer = nodo actual del arbol
program_mode_t decode_mode = SEQUENTIAL;
pthread_t decode_threads[MAX_COUNTER] = {0};
pid_t decode_forks[MAX_COUNTER] = {0};
int decode_counter = MAX_COUNTER;

pthread_mutex_t decode_mutex;
pthread_cond_t decode_cond;

void decompress_file(FILE*, const char*, int);
void decompress_char(char, char[], int*, int*, int);

void scan_huff(const char *file_path) {
    // Escanea los códigos de cada caracter

    FILE *file = fopen(file_path, "rb");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    fread(&code_list.size, sizeof(int), 1, file);
    start_files = sizeof(int);

    char buffer[BUFFER_SIZE] = {'\0'};
    utf8proc_uint8_t *utf8_buffer = (utf8proc_uint8_t*) buffer;

    code_list.head = calloc(sizeof(char_code_t), code_list.size);
    char_code_t* head = code_list.head;
    
    fread(buffer, 1, BUFFER_SIZE, file);
    
    for (int i = 0; i < code_list.size; i++) {
        // Para cada caracter distinto scaneado

        int utf8_codepoint = 0, size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        utf8_buffer += size;
        head[i].character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, (utf8proc_uint8_t*) head[i].character);    // Guarda el caracter en la lista
        }
        
        char code[40] = {0};
        int c = 0;
        
        do {
            code[c] = (char)*utf8_buffer;
            c++;
        } while (*utf8_buffer++ != '\0');
        
        
        head[i].code = calloc(1, strlen(code) + 1);
        strcpy(head[i].code, code);                 // Guarda el codigo del caracter respectivo
        
        int rest = BUFFER_SIZE - ((char*)utf8_buffer - buffer); // Bytes del buffer restantes por leer

        if (rest <= (32)) {
            // Si al buffer le quedan 32 bytes o menos, posiciona el restante al inicio y rellena el buffer

            memmove(buffer, utf8_buffer, rest);
            fread(&(buffer[rest]), 1, BUFFER_SIZE - rest, file);
            start_files += (char*) utf8_buffer - buffer;
            utf8_buffer = (utf8proc_uint8_t*) buffer;
        }
    }

    start_files += (char*) utf8_buffer - buffer;
    tree = restore_tree(code_list);                 // Crea el árbol de codigo segun los codigos leidos
    
    fclose(file);
}

void decompress_dir(const char *compressed_path, const char *dir_path) {
    FILE *file = fopen(compressed_path, "rb");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    fseek(file, start_files, SEEK_SET); // Posiciona al inicio de lectura de los archivos

    int compressed_files_number;
    fread(&compressed_files_number, sizeof(compressed_files_number), 1, file);
    file_pointers = calloc(compressed_files_number, sizeof(long));

    for (int i = 0; i < compressed_files_number; i++) {
        // Para cada archivo guarda el puntero del archivo dentro del comprimido y se mueve al siguiente
        int move = 0;
        fread(&move, sizeof(move), 1, file);

        file_pointers[i] = ftell(file);
        
        fseek(file, move - sizeof(move), SEEK_CUR);
    }
    
    fclose(file);

    construct_dir(dir_path);    // Si el directorio para guardar los archivos no existe, lo crea
    
    for (int i = 0; i < compressed_files_number; i++) {
        FILE *actual_file = fopen(compressed_path, "rb");
        decompress_file(actual_file, dir_path, i);
    }
}

void decompress_file(FILE *compressed_file, const char *dir_path, int file_pos) {
    char filename[400] = {0}, buffer[BUFFER_SIZE] = {0}, decompressed_buffer[BUFFER_SIZE] = {0};
    int filename_bits = 0, file_bits = 0, decompressed_bits = 0, index = 0;
    // decompressed_bits = bits actualmente decomprimidos en el buffer de descompresion
    // filename_bits = bits ocupados por la compresion del nombre del archivo
    // file_bits = bits ocupados por la compresion del contenido del archivo
    // index = indice actual de decompressed_buffer
    
    fseek(compressed_file, file_pointers[file_pos], SEEK_SET);          // Comienza a leer desde el inicio del archivo
    fread(&filename_bits, sizeof(filename_bits), 1, compressed_file);
    fread(&file_bits, sizeof(file_bits), 1, compressed_file);
    tree_pointer = tree;
    
    do {
        int filename_bytes = ceil(filename_bits / 8.0);
        
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
        fread(buffer, 1, BUFFER_SIZE, compressed_file);
        
        for (int i = 0; i < BUFFER_SIZE && decompressed_bits < file_bits; i++) {
            decompress_char(buffer[i], decompressed_buffer, &index, &decompressed_bits, file_bits);
            
            if (BUFFER_SIZE - index >= 32) continue;
            
            fprintf(file,"%s", decompressed_buffer);
            memset(decompressed_buffer, 0, BUFFER_SIZE);
            index = 0;
        }
        
        memset(buffer, 0, BUFFER_SIZE);
    } while (decompressed_bits < file_bits);
    
    fprintf(file, "%s", decompressed_buffer);
    fclose(file);
    fclose(compressed_file);
}

void decompress_char(char character, char buffer[], int *index, int *decompressed_bits, int bits_bound) {
    for (int i = 0; i < 8 && *decompressed_bits < bits_bound; i++) {
        // Para cada bit del byte actual del buffer de descompresion

        char mask = 0x80 >> i;
        tree_pointer = (character & mask) == 0 ? tree_pointer->left : tree_pointer->right;
        // Si el bit es 0 o 1, tree_pointer viaja al hijo izquierdo o derecho

        (*decompressed_bits)++;
        
        if (tree_pointer->is_char == 0) continue;

        // Si el nodo actual es un caracter, lo guarda en el buffer y tree_pointer vuelve al nodo raíz       
        strcat(buffer, tree_pointer->character);
        *index += strlen(tree_pointer->character);
        tree_pointer = tree;
    }
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

    if (argc >= 5) decode_mode = strcmp(argv[4], "--thread") == 0
                                 ? CONCURRENT : strcmp(argv[4], "--fork") == 0
                                 ? PARALLEL : SEQUENTIAL;
    
    scan_huff(file_path);
    decompress_dir(file_path, dir_path);

    return 0;
}
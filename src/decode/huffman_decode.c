#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utf8proc.h>
#include <time.h>

#include "cnvchar.h"
#include "huffman_decode.h"
#include "path_manager.h"
#include "chronometer.h"

#define MAX_COUNTER 100

int start_files = 0, compressed_files_number;
long *file_pointers;                            // byte del archivo donde comienza la lectura de cada archivo
char_freq_t *tree, *tree_pointer;
// tree = arbol de codigos
// tree_pointer = nodo actual del arbol
pthread_t decode_threads[MAX_COUNTER] = {0};
thread_decode_args_t decode_thread_args[MAX_COUNTER] = {0};
pid_t *decode_forks;
int *decode_occupied;

program_mode_t decode_mode = SEQUENTIAL;
pthread_mutex_t *decode_mutex;
pthread_cond_t *decode_cond;

int decode_counter = MAX_COUNTER;

void *unpack_thread_decode(void*);
void decompress_file(FILE*, const char*, int);
void decompress_char(char, char[], int*, int*, int);
void decompress_single_file(const char*, const char*);

void wait_decode() {
    pthread_mutex_lock(decode_mutex);
    while (decode_counter <= 0) {
        pthread_cond_wait(decode_cond, decode_mutex);
    }
    decode_counter--;
    pthread_mutex_unlock(decode_mutex);
}

void signal_decode() {
    pthread_mutex_lock(decode_mutex);
    decode_counter++;
    pthread_cond_signal(decode_cond);
    pthread_mutex_unlock(decode_mutex);
}

void wait_decode_threads() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        pthread_join(decode_threads[i], NULL);
    }
}

void wait_decode_forks() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        waitpid(decode_forks[i], NULL, 0);
    }
}

void wait_decode_program() {
    if (decode_mode == CONCURRENT) wait_decode_threads();
    else if (decode_mode == PARALLEL) wait_decode_forks();
}

int get_decode_avilable() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        if (decode_occupied[i] == 0) {
            wait_decode();
            decode_occupied[i] = 1;

            return i;
        }
    }

    return -1;
}

void free_decode(int i) {
    if (decode_mode == CONCURRENT) decode_thread_args[i] = (thread_decode_args_t) {NULL, NULL, 0};
    if (decode_mode == PARALLEL) decode_forks[i] = 0;
    
    decode_occupied[i] = 0;
    signal_decode();
}

void decode_thread(FILE *compressed_file, char const *file_path, int file_pos) {
    int index = get_decode_avilable();
    decode_thread_args[index] = (thread_decode_args_t) {compressed_file, file_path, file_pos};
    
    pthread_create(&decode_threads[index], NULL, unpack_thread_decode, &decode_thread_args[index]);
}

void *unpack_thread_decode(void *args_pointer) {
    thread_decode_args_t *args = (thread_decode_args_t*) args_pointer;

    int index = 0;

    decompress_file(args->file, args->dir_path, args->file_pos);

    for (index = 0; index < MAX_COUNTER - 1; index++) if (args == &decode_thread_args[index]) break;
    free_decode(index);

    return NULL;
}

void decode_fork(FILE *compressed_file, char const *file_path, int file_pos) {
    int index = get_decode_avilable();
    pid_t pid = fork();

    if (pid == 0) {
        decompress_file(compressed_file, file_path, file_pos);
        free_decode(index);

        exit(0);
    }

    decode_forks[index] = pid;
}

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

void decompress_single_file(const char *compressed_path, const char *output_dir) {
    FILE *file = fopen(compressed_path, "rb");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    fseek(file, start_files, SEEK_SET);
    
    int compressed_files_number;
    fread(&compressed_files_number, sizeof(compressed_files_number), 1, file);
    
    if (compressed_files_number != 1) {
        printf("Warning: This compressed file contains %d files, but treating as single file\n", compressed_files_number);
    }
    
    file_pointers = calloc(1, sizeof(long));
    
    // Read the single file pointer
    int move = 0;
    fread(&move, sizeof(move), 1, file);
    file_pointers[0] = ftell(file);
    
    fclose(file); // Close the file here, before calling decompress_file
    
    // Create output directory if it doesn't exist
    if (output_dir != NULL) {
        construct_dir(output_dir);
        // Open a fresh file pointer for decompress_file
        FILE *fresh_file = fopen(compressed_path, "rb");
        decompress_file(fresh_file, output_dir, 0);
    } else {
        // If no output directory specified, extract to current directory
        FILE *fresh_file = fopen(compressed_path, "rb");
        decompress_file(fresh_file, ".", 0);
    }
    
    printf("Single file decompressed successfully\n");
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
        if (decode_mode == PARALLEL) {}
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
    FILE *output_file = fopen(decompressed_buffer, "w");
    
    if (output_file == NULL) {
        perror("fopen output file");
        fclose(compressed_file);
        return;
    }
    
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
            
            fprintf(output_file,"%s", decompressed_buffer);
            memset(decompressed_buffer, 0, BUFFER_SIZE);
            index = 0;
        }
        
        memset(buffer, 0, BUFFER_SIZE);
    } while (decompressed_bits < file_bits);
    
    fprintf(output_file, "%s", decompressed_buffer);
    fclose(output_file); // Close the output file
    fclose(compressed_file); // Close the compressed file
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
        printf("Usage: %s <decode> <compressed_file.huff> <output_directory> [--thread|--fork]\n", argv[0]);
        printf("  Decompresses a .huff file to the specified directory\n");
        return 0;
    }
    
    start_chronometer();
    
    char *file_path = calloc(1, strlen(argv[2]) + 3);
    char *dir_path = calloc(1, strlen(argv[3]) + 3);
    sprintf(file_path, "./%s", argv[2]);
    sprintf(dir_path, "./%s", argv[3]);

    if (argc >= 5) decode_mode = strcmp(argv[4], "--thread") == 0
                                 ? CONCURRENT : strcmp(argv[4], "--fork") == 0
                                 ? PARALLEL : SEQUENTIAL;

    if (decode_mode == CONCURRENT) {
        decode_mutex = calloc(1, sizeof(pthread_mutex_t));
        decode_cond = calloc(1, sizeof(pthread_cond_t));
        decode_occupied = calloc(MAX_COUNTER, sizeof(pthread_mutex_t));

        pthread_mutex_init(decode_mutex, NULL);
        pthread_cond_init(decode_cond, NULL);
    }

    if (decode_mode == PARALLEL) {
        decode_mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        decode_cond = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        decode_forks = mmap(NULL, MAX_COUNTER * sizeof(pid_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        decode_occupied = mmap(NULL, MAX_COUNTER * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

        pthread_mutex_init(decode_mutex, NULL);
        pthread_cond_init(decode_cond, NULL);
    }
    
    scan_huff(file_path);
    
    // Check if the compressed file contains a single file or multiple files
    FILE *temp_file = fopen(file_path, "rb");
    if (temp_file != NULL) {
        fseek(temp_file, start_files, SEEK_SET);
        int files_in_archive;
        fread(&files_in_archive, sizeof(files_in_archive), 1, temp_file);
        fclose(temp_file);
        
        if (files_in_archive == 1) {
            printf("Decompressing single file from: %s\n", argv[2]);
            decompress_single_file(file_path, dir_path);
        } else {
            printf("Decompressing directory (%d files) from: %s\n", files_in_archive, argv[2]);
            decompress_dir(file_path, dir_path);
        }
    }

    stop_chronometer("Decompression");

    if (decode_mode != SEQUENTIAL) {
        pthread_mutex_destroy(decode_mutex);
        pthread_cond_destroy(decode_cond);
    }

    return 0;
}

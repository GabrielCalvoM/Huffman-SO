//////////////////////////////////////////////////////////////////////////////

#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utf8proc.h>
#include <sys/stat.h>

#include "cnvchar.h"
#include "huffman_encode.h"
#include "path_manager.h"
#include "chronometer.h"

#define MAX_COUNTER 100

int *files_number;
pthread_t encode_threads[MAX_COUNTER] = {0};
thread_encode_args_t encode_thread_args[MAX_COUNTER] = {0};
pid_t *encode_forks;
int *is_occupied;
long *encode_file_pointer;

program_mode_t encode_mode = SEQUENTIAL;
pthread_mutex_t *encode_mutex;
pthread_cond_t *encode_cond;

int encode_counter = MAX_COUNTER;

int is_directory(const char* path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

void file_func_thread(void (*file_func) (char const *, int), char const*, int);
void *unpack_thread(void*);
void file_func_fork(void (*file_func) (char const *, int), char const*, int);
void scan_file_characters(const char*, int);
void scan_str(char[], int*, char_list_t*);
void add_compressed_file(const char*, int);
void compress_file(const char*, int);
void encode_str(char[], int*, FILE*, unsigned char[], int*, int*);
void record_encoded_str(const char*, FILE*, unsigned char*, int*, int*);
void compress_single_file(const char*, const char*);
void compress_dir(const char*, const char*);

void wait_encode() {
    pthread_mutex_lock(encode_mutex);
    while (encode_counter <= 0) {
        pthread_cond_wait(encode_cond, encode_mutex);
    }
    encode_counter--;
    pthread_mutex_unlock(encode_mutex);
}

void signal_encode() {
    pthread_mutex_lock(encode_mutex);
    encode_counter++;
    pthread_cond_signal(encode_cond);
    pthread_mutex_unlock(encode_mutex);
}

void wait_encode_threads() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        pthread_join(encode_threads[i], NULL);
    }
}

void wait_encode_forks() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        waitpid(encode_forks[i], NULL, 0);
    }
}

void wait_encode_program() {
    if (encode_mode == CONCURRENT) wait_encode_threads();
    else if (encode_mode == PARALLEL) wait_encode_forks();
}

int get_first_avilable() {
    for (int i = 0; i < MAX_COUNTER; i++) {
        if (is_occupied[i] == 0) {
            wait_encode();
            is_occupied[i] = 1;

            return i;
        }
    }

    return -1;
}

void free_space(int i) {
    if (encode_mode == CONCURRENT) encode_thread_args[i] = (thread_encode_args_t) {NULL, NULL, 0};
    if (encode_mode == PARALLEL) encode_forks[i] = 0;
    
    is_occupied[i] = 0;
    signal_encode();
}

void dir_iterate(void (*file_func) (char const *, int), const char *dir_path, program_mode_t mode) {
    // Aplica una función a cada archivo de un directorio
    
    struct dirent *file;    // Archivo de directorio
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("opendir");
        return;
    }
    
    char *file_path;
    int pos = 0;

    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        file_path = calloc(strlen(dir_path) + 102, 1);
        strcpy(file_path, dir_path);
        strcat(file_path, "/");
        strcat(file_path, file->d_name);    // Obtiene la ruta del archivo

        if (dir_is_valid(file_path)) continue;    // No soporta subdirectorios

        if (mode == CONCURRENT) file_func_thread(file_func, file_path, pos);
        else if (mode == PARALLEL) file_func_fork(file_func, file_path, pos);
        else file_func(file_path, pos);

        pos++;
    }

    wait_encode_program();
    closedir(dir);
}

void file_func_thread(void (*file_func) (char const *, int), char const *file_path, int file_pos) {
    int index = get_first_avilable();
    encode_thread_args[index] = (thread_encode_args_t) {file_func, file_path, file_pos};
    
    pthread_create(&encode_threads[index], NULL, unpack_thread, &encode_thread_args[index]);
}

void *unpack_thread(void *args_pointer) {
    thread_encode_args_t *args = (thread_encode_args_t*) args_pointer;

    void (*file_func) (char const *, int) = args->file_func;
    int index = 0;

    file_func(args->dir_path, args->file_pos);

    for (index = 0; index < MAX_COUNTER - 1; index++) if (args == &encode_thread_args[index]) break;
    free_space(index);

    return NULL;
}

void file_func_fork(void (*file_func) (char const *, int), char const *file_path, int file_pos) {
    int index = get_first_avilable();
    pid_t pid = fork();

    if (pid == 0) {
        file_func(file_path, file_pos);
        free_space(index);

        exit(0);
    }

    encode_forks[index] = pid;
}

void scan_dir_characters(const char *path) {
    dir_iterate(scan_file_characters, path, SEQUENTIAL);    // Scanea la frecuencia de caracteres de cada archivo
    char_freq_t *tree = construct_tree(char_list);          // Construye el árbol de frecuencias
    code_tree(tree, &code_list);                            // Genera una lista con el código respectivo de cada caracter
}

void scan_single_file_characters(const char *file_path) {
    scan_file_characters(file_path, 0);
    char_freq_t *tree = construct_tree(char_list);
    code_tree(tree, &code_list);
}

void scan_file_characters(const char *file_path, int pos) {
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    if (encode_mode != SEQUENTIAL) {
        pthread_mutex_lock(encode_mutex);
        (*files_number)++;
        pthread_mutex_unlock(encode_mutex);
    }
    else (*files_number)++;

    char buffer[BUFFER_SIZE], *filename;
    int start_buffer = 0;
    char_list_t list = {NULL, 0};
    
    filename = strrchr(file_path, '/');
    if (filename == NULL) filename = (char*)file_path;
    else filename++; // Skip the '/' character

    strcpy(buffer, filename);
    scan_str(buffer, &start_buffer, &list);

    while (fgets(buffer + start_buffer, BUFFER_SIZE - start_buffer, file)) {
        scan_str(buffer, &start_buffer, &list);
    }

    for (int i = 0; i < list.size; i++) {
        if (encode_mode != SEQUENTIAL) {
            pthread_mutex_lock(encode_mutex);
            add_frequency(&char_list, &list.head[i]);
            pthread_mutex_unlock(encode_mutex);
        }
        else add_frequency(&char_list, &list.head[i]);
    }
    
    fclose(file);
}

void scan_str(char buffer[], int *start_buffer, char_list_t *list) {
    // Aumenta la frecuencia de los caracteres en 1 según los vaya leyendo

    utf8proc_uint8_t *utf8_buffer = (utf8proc_uint8_t*) buffer; // Puntero al caracter actual analizando
    int utf8_codepoint = 0;
        
    while ((char *)utf8_buffer < buffer + BUFFER_SIZE && utf8_buffer[0] != '\0') {
        // Siempre que lea un caracter utf8 válido

        int size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        if (utf8_codepoint < 0) {
            // Cuando el caracter leído no es válido registra el punto donde quedó la lectura

            *start_buffer = buffer + BUFFER_SIZE - (char *)utf8_buffer;
            strcpy(buffer, (char *)utf8_buffer);
            break;
        }
        
        utf8_buffer += size;
        char *utf8_character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, (utf8proc_uint8_t*) utf8_character);
            add_character(list, utf8_character); // Incrementa la frecuencia del caracter encontrado en 1
        }
    }
}

void compress_single_file(const char *file_path, const char *output_path) {
    char compressed_filename[400];
    
    if (output_path != NULL) {
        // If output path is specified, use it
        if (strstr(output_path, ".huff") != NULL) {
            // If output already has .huff extension, use as-is
            strcpy(compressed_filename, output_path);
        } else {
            // Add .huff extension
            sprintf(compressed_filename, "%s.huff", output_path);
        }
    } else {
        // Default behavior: use original filename with .huff extension
        char *base_name = strrchr(file_path, '/');
        if (base_name == NULL) base_name = (char*)file_path;
        else base_name++; // Skip the '/' character
        
        sprintf(compressed_filename, "./%s.huff", base_name);
    }
    
    FILE *compressed_file = fopen(compressed_filename, "wb");

    if (compressed_file == NULL) {
        perror("fopen");
        return;
    }

    fwrite(&code_list.size, sizeof(code_list.size), 1, compressed_file);

    for (int i = 0; i < code_list.size; i++) {
        fwrite(code_list.head[i].character, 1, strlen(code_list.head[i].character), compressed_file);
        fwrite(code_list.head[i].code, 1, strlen(code_list.head[i].code) + 1, compressed_file);
    }

    int single_file_number = 1;
    fwrite(&single_file_number, sizeof(single_file_number), 1, compressed_file);
    
    long file_actual_pos = ftell(compressed_file);
    encode_file_pointer = calloc(2, sizeof(long));
    
    compress_file(file_path, 0);
    
    encode_file_pointer[0] += file_actual_pos;
    add_compressed_file(compressed_filename, 0);
    
    fclose(compressed_file);
    printf("Single file compressed successfully: %s\n", compressed_filename);
}

void compress_dir(const char *dir_path, const char *output_path) {
    char compressed_filename[400];
    
    if (output_path != NULL) {
        // If output path is specified, use it
        if (strstr(output_path, ".huff") != NULL) {
            // If output already has .huff extension, use as-is
            strcpy(compressed_filename, output_path);
        } else {
            // Add .huff extension
            sprintf(compressed_filename, "%s.huff", output_path);
        }
    } else {
        // Default behavior: use directory name with .huff extension
        sprintf(compressed_filename, "./%s.huff", dir_path);
    }
    
    FILE *compressed_file = fopen(compressed_filename, "wb"); // Crea archivo comprimido

    if (compressed_file == NULL) {
        perror("fopen");
        return;
    }

    fwrite(&code_list.size, sizeof(code_list.size), 1, compressed_file);

    for (int i = 0; i < code_list.size; i++) {
        fwrite(code_list.head[i].character, 1, strlen(code_list.head[i].character), compressed_file);
        fwrite(code_list.head[i].code, 1, strlen(code_list.head[i].code) + 1, compressed_file);
    }

    fwrite(files_number, sizeof(*files_number), 1, compressed_file);
    long file_actual_pos = ftell(compressed_file);

    encode_file_pointer = encode_mode == PARALLEL
                            ? mmap(NULL, (*files_number) + 1 * sizeof(long), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0)
                            : calloc((*files_number) + 1, sizeof(long));

    dir_iterate(compress_file, dir_path, encode_mode); // Agrega la compresion de cada archivo al archivo comprimido
    fclose(compressed_file);

    for (int i = 0; i < (*files_number); i++) {
        encode_file_pointer[i] += file_actual_pos;
        file_actual_pos = encode_file_pointer[i];
        
        if (encode_mode == CONCURRENT) file_func_thread(add_compressed_file, compressed_filename, i);
        else if (encode_mode == PARALLEL) file_func_fork(add_compressed_file, compressed_filename, i);
        else add_compressed_file(compressed_filename, i);
    }
    
    wait_encode_program();
    printf("Directory compressed successfully: %s\n", compressed_filename);
}

void add_compressed_file(const char *compressed_filename, int i) {
    char file_buff_name[20];
    sprintf(file_buff_name, ".buff_f%d", i);
    FILE *file = fopen(file_buff_name, "rb"), *compressed_file = fopen(compressed_filename, "rb+");
    
    char buffer[BUFFER_SIZE] = {0};
    int read_size = 0;
    
    fseek(compressed_file, encode_file_pointer[i], SEEK_SET);
    printf("%ld\n", ftell(compressed_file));
    
    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        fwrite(buffer, 1, read_size, compressed_file);
    }

    fclose(compressed_file);
    fclose(file);
    remove(file_buff_name);
}

void compress_file(const char *file_path, int file_pos) {
    char file_buff_name[20];
    sprintf(file_buff_name, ".buff_f%d", file_pos);
    
    FILE *compressed_file = fopen(file_buff_name, "wb");    // Archivo de compresion
    FILE *file = fopen(file_path, "r");                     // Archivo a comprimir

    if (file == NULL) {
        perror("fopen");
        return;
    }

    char buffer[BUFFER_SIZE] = {0}, *filename;              // buffer = buffer del archivo a comprimir
    utf8proc_uint8_t compressed_buffer[BUFFER_SIZE] = {0};  // compressed_buffer = buffer con los bits a escribir en el archivo comprimido
    int start_buffer = 0, compressed_bits = 0, filename_bit_count = 0, file_bit_count = 0, file_bytes = 0;
    // start_buffer = registro donde quedó la lectura interrumpida del buffer
    // compressed_bits = bits actualmente compresos en el buffer de compresion
    // filename_bit_count = bits ocupados para la compresion del nombre del archivo
    // file_bit_count = bits ocupados para la compresion del contenido del archivo
    // file_bytes = bytes que ocupa toda la informacion comprimida del archivo
    
    filename = strrchr(file_path, '/');
    
    if (filename == NULL) filename = (char*)file_path;
    else filename++; // Skip the '/' character

    strcpy(buffer, filename);

    long file_start = ftell(compressed_file);   // Inicio del archivo en la compresión

    fwrite(&file_bytes, sizeof(file_bytes), 1, compressed_file);                    // guarda espacio para los datos de descompresion
    fwrite(&filename_bit_count, sizeof(filename_bit_count), 1, compressed_file);    //
    fwrite(&file_bit_count, sizeof(file_bit_count), 1, compressed_file);            //
            
    encode_str(buffer, &start_buffer, compressed_file, compressed_buffer, &compressed_bits, &filename_bit_count);

    if (compressed_bits > 0) fwrite(compressed_buffer, 1, strlen((char*) compressed_buffer), compressed_file);
    memset(buffer, 0, BUFFER_SIZE);
    memset(compressed_buffer, 0, BUFFER_SIZE);  // Guarda el nombre comprimido y restaura los buffers

    start_buffer = 0;
    compressed_bits = 0;

    while (fgets(buffer, BUFFER_SIZE - start_buffer, file)) {
        encode_str(buffer, &start_buffer, compressed_file, compressed_buffer, &compressed_bits, &file_bit_count);
        memset(buffer, 0, BUFFER_SIZE);
    }

    if (compressed_bits > 0) fwrite(compressed_buffer, 1, strlen((char*) compressed_buffer), compressed_file);

    file_bytes = (int) ftell(compressed_file) - file_start;
    encode_file_pointer[file_pos + 1] = file_bytes;

    fseek(compressed_file, file_start, SEEK_SET);
    fwrite(&file_bytes, sizeof(file_bytes), 1, compressed_file);                // registra los datos de descompresion
    fwrite(&filename_bit_count, sizeof(filename_bit_count), 1, compressed_file);//
    fwrite(&file_bit_count, sizeof(filename_bit_count), 1, compressed_file);    //
    fseek(compressed_file, 0, SEEK_END);

    fclose(file);
}

void encode_str(char buffer[], int *start_buffer, FILE *file, unsigned char compressed_buffer[], int *encoded_bits, int *bit_count) {
    // Transforma el texto en código comprimido
    
    utf8proc_uint8_t *utf8_buffer; // Puntero al caracter actual analizando
    int utf8_codepoint = 0;

    utf8_buffer = (utf8proc_uint8_t*) buffer;
        
    while ((char *)utf8_buffer < buffer + BUFFER_SIZE && utf8_buffer[0] != '\0') {
        // Siempre que lea un caracter utf8 válido

        int size = utf8proc_iterate(utf8_buffer, -1, &utf8_codepoint);
        
        if (utf8_codepoint < 0) {
            // Cuando el caracter leído no es válido registra el punto donde quedó la lectura

            *start_buffer = buffer + BUFFER_SIZE - (char *)utf8_buffer;
            strcpy(buffer, (char *)utf8_buffer);
            break;
        }
        
        utf8_buffer += size;
        char *utf8_character = calloc(5, sizeof(char));
        
        if (utf8proc_codepoint_valid(utf8_codepoint)) {
            utf8proc_encode_char(utf8_codepoint, (utf8proc_uint8_t*) utf8_character);
            char *character_code = search_code(&code_list, utf8_character);                         // Código binario del caracter
            record_encoded_str(character_code, file, compressed_buffer, encoded_bits, bit_count);   // Guarda el caracter en el buffer
        }
    }
}

void record_encoded_str(const char *str, FILE *file, unsigned char buffer[], int *recorded_bits, int *count) {
    // Guarda el caracter en el buffer

    int bit_pointer = *recorded_bits % 8, buffer_bytes = *recorded_bits / 8;
    unsigned char char_buffer = 0x00;
    // bit_pointer = bit actual del byte actual del buffer
    // buffer_bytes = byte actual del buffer
    // char_buffer = guarda los bits del código antes de agregarlos al buffer

    for (int i = 0; i < strlen(str); i++) {
        // Para cada bit del código
        
        char_buffer <<= 1;      // Corre los bits a la derecha
        bit_pointer++;
        (*recorded_bits)++;
        (*count)++;

        if (str[i] == '1') char_buffer++;   // Coloca el bit en 0 o 1 según el código
        
        if (bit_pointer >= 8) {
            buffer[buffer_bytes] |= char_buffer; // Guarda el los bits en el buffer
            buffer_bytes++;
            char_buffer = 0x00;     // Reinicia conteo de bits en el byte
            bit_pointer = 0;        //
        }
        
        if (buffer_bytes >= BUFFER_SIZE) {
            fwrite(buffer, 1, buffer_bytes, file);  // Registra los bits del buffer en el archivo comprimido
            memset(buffer, 0, BUFFER_SIZE);         // Reinicia el conteo del buffer
            buffer_bytes = 0;                       //
            *recorded_bits = 0;                     //
        }
    }

    if (bit_pointer > 0) {
        // Si todavía quedan bits sin guardar, ajusta la posición y los guarda en el buffer
        char_buffer <<= (8 - bit_pointer);
        buffer[buffer_bytes] |= char_buffer;
    }
}

int encode_main(int argc, char *argv[]) {
    start_chronometer();
    
    char *output_path = NULL;
    int mode_arg_index = 3;
    
    // Check if output path is provided
    if (argc >= 4 && argv[3][0] != '-') {
        output_path = argv[3];
        mode_arg_index = 4;
    }
    
    // Check for threading mode
    if (argc > mode_arg_index) {
        encode_mode = strcmp(argv[mode_arg_index], "--thread") == 0
                     ? CONCURRENT : strcmp(argv[mode_arg_index], "--fork") == 0
                     ? PARALLEL : SEQUENTIAL;
    }

    if (encode_mode == CONCURRENT) {
        encode_mutex = calloc(1, sizeof(pthread_mutex_t));
        encode_cond = calloc(1, sizeof(pthread_cond_t));
        is_occupied = calloc(MAX_COUNTER, sizeof(pthread_mutex_t));

        pthread_mutex_init(encode_mutex, NULL);
        pthread_cond_init(encode_cond, NULL);
    }

    if (encode_mode == PARALLEL) {
        encode_mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        encode_cond = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        encode_forks = mmap(NULL, MAX_COUNTER * sizeof(pid_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        is_occupied = mmap(NULL, MAX_COUNTER * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
        files_number = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

        pthread_mutex_init(encode_mutex, NULL);
        pthread_cond_init(encode_cond, NULL);
    } 
    
    else {
        files_number = calloc(1, sizeof(int));
    }
    
    // Check if input is a file or directory
    if (is_directory(argv[2])) {
        printf("Compressing directory: %s\n", argv[2]);
        if (output_path) printf("Output file: %s\n", output_path);
        scan_dir_characters(argv[2]);
        compress_dir(argv[2], output_path);
    } else {
        printf("Compressing file: %s\n", argv[2]);
        if (output_path) printf("Output file: %s\n", output_path);
        *files_number = 1;
        scan_single_file_characters(argv[2]);
        compress_single_file(argv[2], output_path);
    }

    stop_chronometer("Compression");

    if (encode_mode != SEQUENTIAL) {
        pthread_mutex_destroy(encode_mutex);
        pthread_cond_destroy(encode_cond);
    }
    
    return 0;
}

#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

#include "cnvchar.h"
#include "huffman_encode.h"
#include "path_manager.h"

int files_number;

void scan_file_characters(const char*, va_list);
void scan_str(char buffer[], int *start_buffer);
void compress_file(const char*, va_list);
void encode_str(char[], int*, FILE*, unsigned char[], int*, int*);
void record_encoded_str(const char*, FILE*, unsigned char*, int*, int*);


void dir_iterate(void (*file_func) (char const *, va_list args), const char *dir_path, ...) {
    // Aplica una función a cada archivo de un directorio
    
    struct dirent *file;    // Archivo de directorio
    DIR *dir = opendir(dir_path);

    if (dir == NULL) {
        perror("opendir");
        return;
    }
    
    char *file_path;
    va_list args;
    va_start(args, dir_path);   // Inicializa los parámetros variadicos

    while ((file = readdir(dir)) != NULL) {
        if (file->d_name[0] == '.') continue;

        file_path = calloc(strlen(dir_path) + 102, 1);
        strcpy(file_path, dir_path);
        strcat(file_path, "/");
        strcat(file_path, file->d_name);    // Obtiene la ruta del archivo

        va_list args_copy;
        va_copy(args_copy, args);
        file_func(file_path, args_copy);
        va_end(args_copy);
    }
    
    va_end(args);
    closedir(dir);
}

void scan_dir_characters(const char *path) {
    dir_iterate(scan_file_characters, path);    // Scanea la frecuencia de caracteres de cada archivo
    char_freq_t *tree = construct_tree(char_list);  // Construye el árbol de frecuencias
    code_tree(tree, &code_list);    // Genera una lista con el código respectivo de cada caracter
}

void scan_file_characters(const char *file_path, va_list args) {
    if (dir_is_valid(file_path)) return;    // No soporta subdirectorios
    
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        perror("fopen");
        return;
    }

    files_number++;
    char buffer[BUFFER_SIZE], *filename;
    int start_buffer = 0;
    
    filename = strchr(file_path, '/') + 1;
    if (filename == NULL) filename = file_path; // Obtiene nombre del archivo

    strcpy(buffer, filename);
    scan_str(buffer, &start_buffer);

    while (fgets(buffer + start_buffer, BUFFER_SIZE - start_buffer, file)) {
        scan_str(buffer, &start_buffer);
    }

    fclose(file);
}

void scan_str(char buffer[], int *start_buffer) {
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
            add_character(&char_list, utf8_character); // Incrementa la frecuencia del caracter encontrado en 1
        }
    }
}

void compress_dir(const char *dir_path) {
    char compressed_file_name[200];
    sprintf(compressed_file_name, "./%s.huff", dir_path);
    FILE *compressed_file = fopen(compressed_file_name, "wb"); // Crea archivo comprimido

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

    dir_iterate(compress_file, dir_path, compressed_file); // Agrega la compresion de cada archivo al archivo comprimido

    fclose(compressed_file);
}

void compress_file(const char *file_path, va_list args) {
    if (dir_is_valid(file_path)) return;    // No soporta subdirectorios
    
    FILE *compressed_file = va_arg(args, FILE*);    // Archivo de compresion
    FILE *file = fopen(file_path, "r");             // Archivo a comprimir

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
    
    filename = strchr(file_path, '/') + 1;
    
    if (filename == NULL) filename = file_path;

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

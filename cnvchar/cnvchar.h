#ifndef CNVCHAR_H
#define CNVCHAR_H

typedef struct char_freq {
    char* character;
    int is_char;
    int frequency;
    struct char_freq *left;
    struct char_freq *right;
} char_freq_t; // Guarda las frecuencias y se usa como nodo para el árbol de los códigos

typedef struct {
    char_freq_t *head;
    int size;
} char_list_t; // XD

typedef struct {
    char *character;
    char *code;
} char_code_t; // XD

typedef struct {
    char_code_t *head;
    int size;
} code_list_t; // XD


// Globales
extern char_list_t char_list;
extern code_list_t code_list;


void add_character(char_list_t *list, char *character);             // agrega un caracter o aumenta la frecuencia en 1
void add_frequency(char_list_t *list, char_freq_t *frequency);      // Incrementa la frecuencia según frequency
void order_list_by_frequency(char_list_t *list);                    // ordena la lista de frecuencias de mayor a menor
char_freq_t *search_character(char_list_t *list, char *character);  // XD
char_freq_t *construct_tree(char_list_t list);                      // XD
void code_tree(char_freq_t *root, code_list_t *code_list);          // XD
char_freq_t *restore_tree(code_list_t list);                        // Crea un árbol para convertir código a caracter
char *search_code(code_list_t *list, char *character);              // XD
void print_tree(char_freq_t*, char*);                               // XD
void test_list(char_list_t list);                                   // XD

#endif // CNVCHAR_H

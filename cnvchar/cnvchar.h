#ifndef CNVCHAR_H
#define CNVCHAR_H

typedef struct char_freq {
    char* character;
    int is_char;
    int frequency;
    struct char_freq *left;
    struct char_freq *right;
} char_freq_t;

typedef struct {
    char_freq_t *head;
    int size;
} char_list_t;

typedef struct {
    char *character;
    char *code;
} char_code_t;

typedef struct {
    char_code_t *head;
    int size;
} code_list_t;


extern char_list_t char_list;
extern code_list_t code_list;


void add_character(char_list_t *list, char *character);
void order_list_by_frequency(char_list_t *list);
char_freq_t *search_character(char_list_t *list, char *character);
char_freq_t *construct_tree(char_list_t list);
void code_tree(char_freq_t *root, code_list_t *code_list);
char_freq_t *restore_tree(code_list_t list);
char *search_code(code_list_t *list, char *character);
void print_tree(char_freq_t*, char*);
void test_list(char_list_t list);

#endif // CNVCHAR_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cnvchar.h"

char_list_t char_list;
code_list_t code_list;

void binary_search_by_frequency(char_freq_t*, char_freq_t, int, int, int*);
char_freq_t *construct_tree_aux(char_list_t);
void code_tree_aux(char_freq_t*, char*, code_list_t*);
char_freq_t *restore_tree_aux(code_list_t, int);

// add_character
void add_character(char_list_t *list, char *character) {
    char_freq_t *found = search_character(list, character);

    if (found != NULL) {
        found->frequency++;
        return;
    }

    list->size++;
    list->head = realloc(list->head, list->size * sizeof(char_freq_t));
    list->head[list->size - 1].character = malloc(strlen(character) + 1);
    strcpy(list->head[list->size - 1].character, character);
    list->head[list->size - 1].is_char = 1;
    list->head[list->size - 1].frequency = 1;
    list->head[list->size - 1].left = NULL;
    list->head[list->size - 1].right = NULL;
}

// order_list_by_frequency
void order_list_by_frequency(char_list_t *list) {
    if (list->head == NULL || list->size <= 1) return;

    char_freq_t *sorted = calloc(list->size, sizeof(char_freq_t));

    for (int i = 0; i < list->size; i++) {
        char_freq_t current = list->head[i];
        int start_index = 0, end_index = i, actual_index = floor(i / 2);

        binary_search_by_frequency(sorted, current, start_index, end_index, &actual_index);

        for (int j = end_index; j > actual_index; j--) {
            sorted[j] = sorted[j - 1];
        }

        sorted[actual_index] = current;
    }

    memcpy(list->head, sorted, list->size * sizeof(char_freq_t));
    free(sorted);
}

void binary_search_by_frequency(char_freq_t *list, char_freq_t current, int start, int end, int *actual) {
    while (start < end && list[*actual].frequency > 0) {
        if (current.frequency < list[*actual].frequency) {
            end = *actual;
            *actual = floor((start + end) / 2);
        } else {
            start = *actual + 1;
            *actual = floor((start + end) / 2);
        }
    }
}

// search_character
char_freq_t *search_character(char_list_t *list, char *character) {
    for (int i = 0; i < list->size; i++) {
        if (strcmp(list->head[i].character, character) == 0) return &list->head[i];
    }

    return NULL;
}

// construct_tree
char_freq_t *construct_tree(char_list_t list) {
    if (list.head == NULL || list.size <= 0) return NULL;
    else if (list.size == 1) return &list.head[0];

    order_list_by_frequency(&list);
    return construct_tree_aux(list);
}

char_freq_t *construct_tree_aux(char_list_t list) {
    if (list.size == 1) return &list.head[0];
    
    int size = --list.size;

    char_freq_t *head = list.head;
    char_freq_t *left = malloc(sizeof(char_freq_t)), *right = malloc(sizeof(char_freq_t));

    memcpy(left, &list.head[0], sizeof(char_freq_t));
    memcpy(right, &list.head[1], sizeof(char_freq_t));

    char_freq_t new_node = (char_freq_t) {NULL, 0, head[0].frequency + head[1].frequency, left, right};

    int i, jump = 2;

    for (i = 0; i < size; i++) {
        if ((head[i + jump].frequency >= new_node.frequency || i + jump > size) && jump > 1) {
            head[i] = new_node;
            jump--;
            continue;
        }

        head[i] = head[i + jump];
    }

    head = realloc(head, (size) * sizeof(char_freq_t));

    return construct_tree_aux(list);
}

// code_tree
void code_tree(char_freq_t *root, code_list_t *code_list) {
    if (root == NULL) return;

    code_tree_aux(root, "", code_list);
    
}

void code_tree_aux(char_freq_t *root, char *code, code_list_t *code_list) {
    if (root->left == NULL && root->right == NULL) {
        (code_list->size)++;
        code_list->head = realloc(code_list->head, code_list->size * sizeof(char_code_t));

        code_list->head[code_list->size - 1].character = malloc(strlen(root->character) + 1);
        strcpy(code_list->head[code_list->size - 1].character, root->character);

        code_list->head[code_list->size - 1].code = malloc(strlen(code) + 1);
        strcpy(code_list->head[code_list->size - 1].code, code);
        return;
    }

    char left_code[64];
    snprintf(left_code, sizeof(left_code), "%s0", code);
    code_tree_aux(root->left, left_code, code_list);
    

    char right_code[64];
    snprintf(right_code, sizeof(right_code), "%s1", code);
    code_tree_aux(root->right, right_code, code_list);
}

// Restore Tree
char_freq_t *restore_tree(code_list_t list) {
    if (list.head == NULL || list.size <= 0) return NULL;
    code_list_t new_list = {calloc(list.size, sizeof(char_code_t)), list.size};

    for (int i = 0; i < list.size; i++) {
        int j = 0;
        while (new_list.head[j].code != NULL
            && strcmp(new_list.head[j].code, list.head[i].code) < 0) j++;

        for (int k = i; k > j; k--) new_list.head[k] = new_list.head[k-1];
        new_list.head[j] = list.head[i];
    }
    
    return restore_tree_aux(new_list, 0);
}

char_freq_t *restore_tree_aux(code_list_t list, int bits) {
    char_freq_t *node = calloc(1, sizeof(char_freq_t));

    if (list.size <= 0) return NULL;
    
    if (list.size == 1) {
        node->character = list.head[0].character;
        node->is_char = 1;
        
        return node;
    }
    
    int index = 1, new_size;
    bits++;
    
    while (index < list.size && list.head[index].code != NULL
        && strncmp(list.head[0].code, list.head[index].code, bits) == 0) {
        index++;
    }

    if (index >= list.size && list.head[index - 1].code[bits - 1] == 0) {
        index = 0;
    }

    new_size = list.size - index;
    code_list_t new_list = {calloc(new_size, sizeof(char_code_t)), new_size};
    
    memcpy(new_list.head, &list.head[index], sizeof(char_code_t) * new_size);
    list.head = realloc(list.head, sizeof(char_code_t) * index);
    list.size = index;

    node->left = restore_tree_aux(list, bits);
    node->right = restore_tree_aux(new_list, bits);

    return node;
}

// Search Code
char *search_code(code_list_t *list, char *character) {
    for (int i = 0; i < list->size; i++) {
        if (strcmp(list->head[i].character, character) == 0) return list->head[i].code;
    }

    return NULL;
}

// Print Tree
void print_tree(char_freq_t *root, char *spacing) {
    if (root->is_char != 0) {
        printf("%s%s\n", spacing, root->character);
        return;
    }
    
    char *new_spacing = calloc(strlen(spacing) + 3, 1);
    sprintf(new_spacing, "  %s", spacing);
    
    printf("%s-\n", spacing);
    print_tree(root->left, new_spacing);
    print_tree(root->right, new_spacing);

    free(new_spacing);
}

// Test List
void test_list(char_list_t list) {
    order_list_by_frequency(&list);

    for (int i = 0; i < list.size; i++) {
        printf("%s: %d\n", list.head[i].character, list.head[i].frequency);
    }

    printf("\n");
    char_freq_t *tree = construct_tree(list);
    code_list_t code_list = {NULL, 0};
    code_tree(tree, &code_list);
    
    for (int i = 0; i < code_list.size; i++) {
        printf("%s: %s\n", code_list.head[i].character, code_list.head[i].code);
    }
}

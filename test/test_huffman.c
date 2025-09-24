#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_QUANT 10


int main(int argc, char *argv[]) {
    char encode_com[100], decode_com[100];
    strcpy(encode_com, "./huffman -e testingFiles ");
    if (argc > 1) strcat(encode_com, argv[1]);
    
    strcpy(decode_com, "./huffman -d testingFiles.huff descompreso/testingFiles ");
    if (argc > 1) strcat(decode_com, argv[1]);

    printf("=============ENCODE=============\n\n\n");
    
    for (int i = 0; i < TEST_QUANT; i++) {
        system(encode_com);
    }
    
    printf("\n\n\n\n=============DECODE=============\n\n\n");
    
    for (int i = 0; i < TEST_QUANT; i++) {
        system(decode_com);
    }
    
    return 0;
}
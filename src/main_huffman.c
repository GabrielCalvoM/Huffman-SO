#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman_encode.h"
#include "huffman_decode.h"

void print_usage(const char *program_name) {
	printf("╔══════════════════════════════╗ \n");
    printf("║ [OS - P1] Huffman Algorithm  ║ \n");
    printf("╚══════════════════════════════╝ \n\n");
    
    printf("Usage:\n");
    printf("  %s < -e encode | -d decode > <input> [output] [options]\n\n", program_name);
    
    printf("Commands:\n");
    printf("  encode  - Compress a file or directory\n");
    printf("  decode  - Decompress a .huff file\n\n");
    
    printf("<Encode> Examples:\n");
    printf("  %s -e file.txt                      		# Creates file.txt.huff\n", program_name);
    printf("  %s -e file.txt compressed.huff	    	# Creates compressed.huff\n", program_name);
    printf("  %s -e dir			                	    # Creates dir.huff\n", program_name);
    printf("  %s -e dir archive.huff	        		# Creates archive.huff\n", program_name);
    printf("  %s -e myfile.txt output.huff --thread 	# Use threading [CONCURRENT] \n", program_name);
    printf("  %s -e mydirectory archive --fork      	# Use forking [PARALELISM] \n\n", program_name);
    
    printf("<Decode> Examples:\n");
    printf("  %s -d compressed.huff output_dir      	# Decompress to output_dir/\n", program_name);
    printf("  %s -d archive.huff extracted_files    	# Decompress to extracted_files/\n", program_name);
    printf("  %s -d file.huff . 		            	# Decompress to current directory\n", program_name);
    printf("  %s -d file.huff . --thread				# Use threading [CONCURRENT]\n", program_name);
    printf("  %s -d file.huff . --fork      			# Use forking [PARALELISM ]\n\n", program_name);
    
    printf("[Options]\n");
    printf("  --thread    Use multithreading (concurrent processing)\n");
    printf("  --fork      Use multiprocessing (parallel processing)\n");
    printf("  (no option) Sequential processing (default)\n\n");
    
    printf("[INFO]: \n");
    printf("  • For encoding, output parameter is optional\n");
    printf("  • For decoding, output directory is required\n");
    printf("  • .huff extension is added automatically if not provided\n");
    printf("  • Type -h for help instead of < -e|-d > to read this info again \n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "-e") == 0) {
        return encode_main(argc, argv);
    } 
    else if (strcmp(argv[1], "-e") == 0) {
        return decode_main(argc, argv);
    } 
    else if (strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    else {
        printf("Error: Unknown command '%s'\n\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
}

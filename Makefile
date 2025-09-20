CC = gcc
CFLAGS = -Wall
LIBS =  -lutf8proc -lm

huffman: cnvchar.o huffman_encode.o huffman_decode.o huffman.c
	$(CC) $(CFLAGS) -o huffman cnvchar.o huffman_encode.o huffman_decode.o huffman.c $(LIBS)

cnvchar.o: cnvchar.c
	$(CC) $(CFLAGS) -c cnvchar.c -o cnvchar.o

huffman_encode.o: cnvchar.o huffman_encode.c
	$(CC) $(CFLAGS) -c huffman_encode.c -o huffman_encode.o

huffman_decode.o: cnvchar.o huffman_decode.c
	$(CC) $(CFLAGS) -c huffman_decode.c -o huffman_decode.o

clean:
	rm -f *.o huffman
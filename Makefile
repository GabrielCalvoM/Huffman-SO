CNV = cnvchar/cnvchar
ENCODE = encode/huffman_encode
DECODE = decode/$(DECODE)

CC = gcc
CFLAGS = -Wall
LIBS =  -lutf8proc -lm

huffman: $(CNV).o $(ENCODE).o $(DECODE).o huffman.c
	$(CC) $(CFLAGS) -o huffman $(CNV).o $(ENCODE).o $(DECODE).o huffman.c $(LIBS)

$(CNV).o: $(CNV).c
	$(CC) $(CFLAGS) -c $(CNV).c -o $(CNV).o

$(ENCODE).o: $(ENCODE).c
	$(CC) $(CFLAGS) -c $(ENCODE).c -o $(ENCODE).o

$(DECODE).o: $(DECODE).c
	$(CC) $(CFLAGS) -c $(DECODE).c -o $(DECODE).o

clean:
	rm -f *.o huffman
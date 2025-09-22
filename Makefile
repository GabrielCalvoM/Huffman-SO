CNV = cnvchar/cnvchar
PATHM = path_manager/path_manager
ENCODE = encode/huffman_encode
DECODE = decode/huffman_decode

CC = gcc
CFLAGS = -Wall
LIBS =  -lutf8proc -lm

huffman: $(CNV).o $(PATHM).o $(ENCODE).o $(DECODE).o main_huffman.c
	$(CC) $(CFLAGS) -Iencode -Idecode -Ipath_manager -o huffman $(CNV).o $(PATHM).o $(ENCODE).o $(DECODE).o main_huffman.c $(LIBS)

$(CNV).o: $(CNV).c
	$(CC) $(CFLAGS) -c $(CNV).c -o $(CNV).o

$(PATHM).o: $(PATHM).c
	$(CC) -c $(PATHM).c -o $(PATHM).o

$(ENCODE).o: $(ENCODE).c
	$(CC) $(CFLAGS) -Icnvchar -Ipath_manager -c $(ENCODE).c -o $(ENCODE).o

$(DECODE).o: $(DECODE).c
	$(CC) $(CFLAGS) -Icnvchar -Ipath_manager -c $(DECODE).c -o $(DECODE).o

clean:
	rm -f */*.o huffman
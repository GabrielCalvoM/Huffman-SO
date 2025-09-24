########################################################################
# PROGRAM MADE BY
#			Carlos Cabrera de la Espriella
#			Gabriel Calvo Montero
########################################################################
# BASE DIR
SRCDIR = src
OBJDIR = src/obj

# Source files
SOURCES = $(SRCDIR)/cnvchar/cnvchar.c \
          $(SRCDIR)/path_manager/path_manager.c \
          $(SRCDIR)/encode/huffman_encode.c \
          $(SRCDIR)/decode/huffman_decode.c \
          $(SRCDIR)/chrono/chronometer.c \
          $(SRCDIR)/main_huffman.c

# Object Files
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# All Project Dirs
INCLUDES = -I$(SRCDIR)/encode \
           -I$(SRCDIR)/decode \
           -I$(SRCDIR)/path_manager \
           -I$(SRCDIR)/chrono \
           -I$(SRCDIR)/cnvchar

########################################################################
CC = gcc
CFLAGS = -Wall
LIBS = -lutf8proc -lm

# Build
$(shell mkdir -p $(OBJDIR)/cnvchar $(OBJDIR)/path_manager $(OBJDIR)/encode $(OBJDIR)/decode $(OBJDIR)/chrono)

# Main target
huffman: $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) -o huffman $(OBJECTS) $(LIBS)

test: test/test_huffman.c
	$(CC) $(CFLAGS) -o test_huffman test/test_huffman.c

# Generic rule for object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR) huffman

.PHONY: clean

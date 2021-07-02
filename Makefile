.PHONY:clean

CC = gcc
CFLAGS = -Wall -g 
OBJ = local_cache_test.o

BIN=test
all: $(BIN)
$(BIN):$(OBJ)
    $(CC) $(CFLAGS) $^ -o $@
#%.o:%.c
.c.o:
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -f *.o main
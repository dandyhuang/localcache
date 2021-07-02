.PHONY:clean

CXX= g++
CPPFLAGS= -g --std=c++11 -Wall -O2 -fPIC
CFLAGS := $(CPPFLAGS)

OBJ = local_cache_test.o

BIN=test
all: $(BIN)
$(BIN):$(OBJ)
    $(CXX) $(CFLAGS) $^ -o $@
#%.o:%.cpp
.cpp.o:
    $(CXX) $(CFLAGS) -c $< -o $@

clean:
    rm -f *.o main
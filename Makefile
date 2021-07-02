.PHONY:clean

CXX= g++
CPPFLAGS= -g --std=c++11 -Wall -O2 -fPIC
CFLAGS := $(CPPFLAGS)

OBJ = local_cache_test.o

BIN=test
all: $(BIN)
$(BIN):$(OBJ)
    ${CXX} ${DLFLAGS} ${LDFLAGS} ${OBJECTS} ${LIBS}  -o ${BIN}

%.o: %.cpp
	${CXX} -c ${CFLAGS} $(INCLUDE) $(CPPINCS) $< -o $@

clean:
    rm -f *.o  $(BIN) 
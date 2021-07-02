.PHONY:clean

CXX= g++
CPPFLAGS= -g --std=c++11 -Wall -O2 -fPIC
CFLAGS := $(CPPFLAGS)

OBJ = local_cache_test.o

BIN=test
$(BIN): $(OBJ)
	${CXX} ${CFLAGS} ${LDFLAGS} ${OBJ} ${LIBS} -o ${BIN}

%.o: %.cpp
	${CXX} -c ${CFLAGS} $(INCLUDE) $(CPPINCS) $< -o $@

all: $(BIN)

clean:
	rm -f *.o  $(BIN) 

CC_FLAGS = -Wall -I.
LD_FLAGS = -Wall -L./ 
BIN_DIR  = bin

all: $(BIN_DIR) libcalc.a $(BIN_DIR)/test $(BIN_DIR)/client $(BIN_DIR)/server

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

servermain.o: servermain.cpp
	$(CXX) $(CC_FLAGS) $(CFLAGS) -c servermain.cpp 

clientmain.o: clientmain.cpp
	$(CXX) $(CC_FLAGS) $(CFLAGS) -c clientmain.cpp 

main.o: main.cpp
	$(CXX) $(CC_FLAGS) $(CFLAGS) -c main.cpp 

$(BIN_DIR)/test: main.o libcalc.a | $(BIN_DIR)
	$(CXX) $(LD_FLAGS) -o $@ main.o -lcalc

$(BIN_DIR)/client: clientmain.o libcalc.a | $(BIN_DIR)
	$(CXX) $(LD_FLAGS) -o $@ clientmain.o -lcalc

$(BIN_DIR)/server: servermain.o libcalc.a | $(BIN_DIR)
	$(CXX) $(LD_FLAGS) -o $@ servermain.o -lcalc

calcLib.o: calcLib.c calcLib.h
	gcc -Wall -fPIC -c calcLib.c

libcalc.a: calcLib.o
	ar -rc libcalc.a calcLib.o

clean:
	rm -rf *.o *.a $(BIN_DIR)
EXECUTABLES = bin bin/graph_test
CPP = g++
CFLAGS = -Wall -Wextra -std=c++11

all: $(EXECUTABLES)

bin:
	mkdir bin

clean:
	rm -f bin/*

bin/graph_test: src/graph_test.cpp src/graph.cpp src/graph.h
	$(CPP) $(CFLAGS) -o bin/graph_test src/graph_test.cpp src/graph.cpp

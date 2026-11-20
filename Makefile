EXECUTABLES = bin bin/osm_convert
CPP = g++
CFLAGS = -Wall -Wextra -std=c++11

all: $(EXECUTABLES)

bin:
	mkdir bin

clean:
	rm -f bin/*

bin/osm_convert: src/osm_convert.cpp
	$(CPP) $(CFLAGS) -o bin/osm_convert src/osm_convert.cpp

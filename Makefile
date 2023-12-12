PROJECT = pngminimizer
SOURCES1 = $(wildcard src/*.cpp)
SOURCES2 = $(filter-out zopfli/src/zopfli/zopfli_bin.c, $(wildcard zopfli/src/zopfli/*.c))
OBJECTS = $(SOURCES1:.cpp=.o) $(SOURCES2:.c=.o)
CC = g++
CFLAGS  = -c -O3 -std=c++11 -Wall -pedantic -Wno-unknown-pragmas -Wno-unused-function
LDFLAGS = -s -lpng16 -lz

all: $(PROJECT)
	@echo Note that you can enable OpenMP support by using \"make openmp\"

setopenmp:
	$(eval OPENMP := -fopenmp)

openmp: setopenmp $(PROJECT)

%.o: %.cpp
	$(CC) $(CFLAGS) $(OPENMP) $< -o $@

$(PROJECT): $(OBJECTS)
	$(CC) $(OPENMP) $(OBJECTS) $(LDFLAGS) -o $(PROJECT)

clean:
	rm $(OBJECTS) -f

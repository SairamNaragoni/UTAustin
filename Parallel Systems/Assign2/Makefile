CC = nvcc
SRCS = ./src/*.cpp ./src/*.cu
INC = ./src/
OPTS = -std=c++17 -O3

EXEC = bin/kmeans

all: clean compile

compile:
	$(CC) $(SRCS) $(OPTS) -I$(INC) -o $(EXEC)

clean:
	rm -f $(EXEC)

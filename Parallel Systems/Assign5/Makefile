CC = mpic++
SRCS = ./src/*.cpp
INC = ./src/
LIBS = -lglfw -lGL -lglut
OPTS = --std=c++17

EXEC = nbody

all: clean compile

compile:
	$(CC) -O3 $(SRCS) $(OPTS) -I$(INC) $(LIBS) -o $(EXEC)

clean:
	rm -f $(EXEC)

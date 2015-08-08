CXXFLAGS=-std=c++14 -Wall
CXXFLAGS+=-I/home/patrick/Development/libraries/fmodstudio/api/lowlevel/inc
LIBS=-Wl,--rpath=. libfmod.so.6 -lSDL2
SRC=$(shell find . -name "*.cpp")
OBJ=$(SRC:%.cpp=%.o)

.PHONY: build

# WTF apparently I don't need to explicitly thing the .os'
build: $(OBJ)
	g++ $(OBJ) $(LIBS) -obuild

run: build
	./build
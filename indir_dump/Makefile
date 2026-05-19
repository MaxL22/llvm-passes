CXX = clang++
CC = clang

# Ask LLVM for the correct flags
LLVM_CXXFLAGS = $(shell llvm-config --cxxflags) 
LLVM_LDFLAGS = $(shell llvm-config --ldflags)
CFLAGS=-fPIC -pthread

all: indir-dump.so logger.o

indir-dump.so: indir_dump.cc
	$(CXX) -fPIC -shared $(LLVM_CXXFLAGS) $(LLVM_LDFLAGS) $< -o $@

logger.o: logger.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f indir-dump.so logger.o

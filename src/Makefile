# Project name
PROJECT = kobra

# Executable name
EXE = $(PROJECT)

ifeq ($(OS),Windows_NT)
	EXE += $(.exe)
endif

# Source files
SRC = board.cpp history.cpp main.cpp movegen.cpp movesorter.cpp \
		search.cpp timeman.cpp tt.cpp uci.cpp

# Object files
OBJS = $(subst .cpp,.o,$(SRC))

# Paths
THIS_PATH := $(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))
ROOT_PATH = $(dir $(THIS_PATH))

# High-level configuration
debug = no
optimize = yes
native = yes

# Low-level configuration
COMP = gcc
CXX = g++
CXXFLAGS = -std=c++17 -static

# Debugging
ifeq ($(debug),no)
	CXXFLAGS += -DNDEBUG
else
	CXXFLAGS += -g
endif

# Optimization
ifeq ($(optimize),yes)
	CXXFLAGS += -O3
endif

ifeq ($(native),yes)
	CXXFLAGS += -march=native
endif

# Targets
.PHONY: build clean

build: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(EXE) $(OBJS)

clean:
	rm -f *.o

depend: .depend

.depend: $(SRC)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

dist-clean: clean
	rm -f *~ .depend

include .depend
# ============================================
#   Kobra MinGW Static Embedded Build
# ============================================

ifeq ($(OS),Windows_NT)
    EXE := kobra_mingw.exe
else
    EXE := kobra_mingw
endif

# ---- Source files ----
SRC  := $(wildcard *.cpp)
OBJS := $(SRC:.cpp=.o)
DEPS := $(OBJS:.o=.d)

# ---- Compiler ----
CXX := g++

# ---- Build Options ----
debug    := no
optimize := yes
native   := yes

# ---- Compiler Flags ----
CXXFLAGS := -std=c++20 -static -DEMBED_NNUE

ifeq ($(debug),no)
    CXXFLAGS += -DNDEBUG
else
    CXXFLAGS += -g
endif

ifeq ($(optimize),yes)
    CXXFLAGS += -O3
endif

ifeq ($(native),yes)
    CXXFLAGS += -march=native
endif

# ---- Linker Flags ----
LDFLAGS := -s

# ============================================

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(CXXFLAGS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# ---- Clean only objects (keep exe) ----
clean-obj:
	rm -f *.o *.d

# ---- Full clean (remove exe too) ----
clean:
	rm -f *.o *.d $(EXE)

-include $(DEPS)

.PHONY: all clean clean-obj

# ============================================
#   Kobra MinGW Static Embedded Build
# ============================================

ifeq ($(OS),Windows_NT)
    EXE := kobra_mingw.exe
else
    EXE := kobra_mingw
endif

# ---- Paths ----
CRYPTO_DIR := ../../crypto

# ---- Engine sources (local) ----
LOCAL_CPP := $(wildcard *.cpp)

# ---- Shared C sources ----
SHARED_C := $(CRYPTO_DIR)/aes.c

# ---- Object files (all local output) ----
OBJS := $(LOCAL_CPP:.cpp=.o) \
        key.o \
        aes.o 

DEPS := $(OBJS:.o=.d)

# ---- Compiler ----
CXX := g++

# ---- Build Options ----
debug    := no
optimize := yes
native   := yes

# ---- Compiler Flags ----
CXXFLAGS := -std=c++20 -static -DEMBED_NNUE
CXXFLAGS += -I$(CRYPTO_DIR)

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

LDFLAGS := -s

# ============================================

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(CXXFLAGS) $(LDFLAGS)

# ---- Compile local cpp files ----
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# ---- Compile shared crypto_key.cpp ----
key.o:
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(CRYPTO_DIR)/key.cpp -o $@

# ---- Compile shared C files ----
aes.o:
	gcc -O3 -I$(CRYPTO_DIR) -c $(CRYPTO_DIR)/aes.c -o $@

# ---- Clean ----
clean:
	rm -f *.o *.d $(EXE)

clean-obj:
	rm -f *.o *.d
	
-include $(DEPS)

.PHONY: all clean
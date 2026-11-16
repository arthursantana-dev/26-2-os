# Barbearia do Sono: compila com `make` e roda com `make run`.
# Requisitos: compilador com C++20 (GCC 11+ ou Clang 14+) e ncurses.

BIN      ?= barbearia
BUILD    ?= build
CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic
CXXFLAGS += -pthread
CPPFLAGS += -MMD -MP
LDFLAGS  += -pthread

# ncurses: no macOS já vem no sistema; no Linux usamos a versão wide
# (acentos e ■ □), achada pelo pkg-config quando disponível.
ifeq ($(shell uname -s),Darwin)
  LDLIBS += -lncurses
else
  NCURSES_LIBS := $(shell pkg-config --libs ncursesw 2>/dev/null)
  LDLIBS += $(if $(NCURSES_LIBS),$(NCURSES_LIBS),-lncursesw)
endif

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=$(BUILD)/%.o)

.PHONY: all run demo tsan clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

run: $(BIN)
	./$(BIN)

demo: $(BIN)
	./$(BIN) --demo

# Versão com ThreadSanitizer, para caçar condição de corrida durante o
# desenvolvimento: `make tsan && ./barbearia-tsan --dia 20 2> tsan.log`
tsan:
	$(MAKE) BIN=barbearia-tsan BUILD=build-tsan \
	  CXXFLAGS="-std=c++20 -O1 -g -Wall -Wextra -fsanitize=thread" \
	  LDFLAGS="-fsanitize=thread"

clean:
	rm -rf build build-tsan barbearia barbearia-tsan

-include $(OBJ:.o=.d)

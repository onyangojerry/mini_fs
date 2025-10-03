CXX := g++
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra
SRC := src/mini_fs.cpp
BIN := mini_fs

all: $(BIN)

$(BIN): $(SRC)
$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

clean:
rm -f $(BIN)

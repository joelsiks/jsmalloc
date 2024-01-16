CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++14

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))
TEST_FILE = test

# Targets
all: prep $(OBJ_FILES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@

prep:
	mkdir -p $(BUILD_DIR)

# TODO
format:
	find src src/include test test/include -name '*.c' -o -name '*.h' | \
	xargs clang-format --style=file -i 

clean:
	rm -rf $(BUILD_DIR) $(TEST_FILE)
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++14 -O2

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))
TEST_OBJ_FILES = $(filter-out $(BUILD_DIR)/TLSFMalloc.o, $(OBJ_FILES))
LIB_OBJ_FILES = $(filter-out $(BUILD_DIR)/test.o, $(TEST_OBJ_FILES))
SHARED_LIB = libtlsf.so

# Targets
all: prep $(LIB_OBJ_FILES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

test: prep $(TEST_OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(TEST_OBJ_FILES) -o test

sharedlib: prep $(OBJ_FILES)
	$(CXX) -shared -o $(SHARED_LIB) $(OBJ_FILES)

prep:
	mkdir -p $(BUILD_DIR)

# TODO
format:
	find src src/include test test/include -name '*.c' -o -name '*.h' | \
	xargs clang-format --style=file -i 

clean:
	rm -rf $(BUILD_DIR) test $(SHARED_LIB) *.out


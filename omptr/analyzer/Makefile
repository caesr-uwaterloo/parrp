CXX := g++
PROTOBUF_FLAGS := $(shell pkg-config protobuf --cflags) -lprotobuf
LIBBOOST_FLAGS := -lboost_graph
CXXFLAGS := -std=c++11 -Wall -Wextra -pedantic -Wno-unused-parameter -O2
CXXFLAGS_DEBUG := -std=c++11 -Wall -Wextra -pedantic -g
ANALYZER_SRC := analyzer.cc proto/protoio.cc proto/custom_mem_trace.pb.cc
CHECKER_SRC := checker.cc proto/protoio.cc proto/custom_mem_trace.pb.cc
# OBJ := $(SRC:.cc=.o)
TARGET := analyzer checker
TARGET_DEBUG := analyzer_debug checker_debug

# Default target
all: $(TARGET)

debug: $(TARGET_DEBUG)

# Compile source file into object file
# %.o: %.cc
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link object file(s) into executable
analyzer: $(ANALYZER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(PROTOBUF_FLAGS) $(LIBBOOST_FLAGS)

checker: $(CHECKER_SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(PROTOBUF_FLAGS) $(LIBBOOST_FLAGS)

analyzer_debug: $(ANALYZER_SRC)
	$(CXX) $(CXXFLAGS_DEBUG) $^ -o $@ $(PROTOBUF_FLAGS) $(LIBBOOST_FLAGS)

checker_debug: $(CHECKER_SRC)
	$(CXX) $(CXXFLAGS_DEBUG) $^ -o $@ $(PROTOBUF_FLAGS) $(LIBBOOST_FLAGS)

# Clean generated files
clean:
	rm -f $(TARGET) $(TARGET_DEBUG)

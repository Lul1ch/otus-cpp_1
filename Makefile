CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

BIN_HOME := bin

.PHONY: all clean test

all: $(BIN_HOME)/mean_mapper $(BIN_HOME)/mean_reducer \
     $(BIN_HOME)/variance_mapper $(BIN_HOME)/variance_reducer

$(BIN_HOME)/mean_mapper: mean_mapper.cpp
	mkdir -p $(BIN_HOME)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN_HOME)/mean_reducer: mean_reducer.cpp
	mkdir -p $(BIN_HOME)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN_HOME)/variance_mapper: variance_mapper.cpp
	mkdir -p $(BIN_HOME)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN_HOME)/variance_reducer: variance_reducer.cpp
	mkdir -p $(BIN_HOME)
	$(CXX) $(CXXFLAGS) -o $@ $<

test: all
	./run_locally.sh

clean:
	rm -rf bin output
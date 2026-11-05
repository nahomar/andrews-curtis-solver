CXX ?= c++
CXXFLAGS ?= -O3 -std=c++17 -pthread
PROGRAMS = acs sacs nielsen_weighted bucket_dedup_test

.PHONY: all check clean
all: $(PROGRAMS:%=build/%)

build/sacs: solver/sacs.cpp solver/ball_bridge.cpp solver/acs.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) solver/sacs.cpp solver/ball_bridge.cpp -o $@

build/%: solver/%.cpp $(wildcard solver/*.cpp solver/*.inc)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $< -o $@

check: all
	./build/sacs selftest
	./build/bucket_dedup_test
	./build/acs ball --cap 6 --out build/test-ac.bin
	./build/acs ball --cap 6 --stable --out build/test-sac.bin
	./build/nielsen_weighted --self-test --ac-ball build/test-ac.bin --sac-ball build/test-sac.bin

clean:
	rm -rf build

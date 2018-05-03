CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Weverything -Wno-c++98-compat -Werror -pedantic -Ofast -march=native

user-counter: user-counter.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f user-counter

.PHONY: clean

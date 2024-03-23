abm-debug: abm.hpp abm.cpp
	g++ -g -Wall -Wextra -Werror abm.cpp -o abm-debug

abm-release: abm.hpp abm.cpp
	g++ -O3 -Wall -Wextra -Werror abm.cpp -o abm-release

check: tests
	./tests

tests: abm.hpp tests.cpp
	g++ -g -Wall tests.cpp -o tests

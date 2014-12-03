all:
	g++ --std=c++11 example.cpp block_buffer.cpp -o block
	./block > test_log

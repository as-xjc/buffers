#ifndef __BLOCK_BUFFER__
#define __BLOCK_BUFFER__

#include <cstdint>
#include <list>

namespace buffer {

class block {
public:
	block(size_t capacity);
	~block();

	size_t capacity();
	size_t free();
	size_t size();
	char* malloc();
	void reset();
	size_t skip(size_t length);
	size_t append(block& other);

	size_t write(char* src, size_t length, bool skip = true);
	size_t read(char* des, size_t length, bool skip = true);

	void debug(size_t format_offset);

private:
	char* _data = nullptr;
	size_t _pos = 0;
	size_t _capacity = 0;
	size_t _head = 0;

};

class block_buffer {
public:
	block_buffer();
	~block_buffer();

	size_t size();
	void clear();
	void merge();
	char* malloc(size_t size);
	size_t skip(size_t length);

	size_t write(char* src, size_t length, bool skip = true);
	size_t read(char* des, size_t length, bool skip = true);

	void debug(size_t format_offset = 16);

private:
	block* allocate_block(size_t capacity);
	block* get_block(size_t size);
	void free_block(block* _block);

	std::list<block*> _blocks;
	std::list<block*> _free_blocks;
};

}

#endif

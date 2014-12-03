#ifndef __BLOCK_BUFFER__
#define __BLOCK_BUFFER__

#include <cstdint>
#include <list>

namespace buffer {

enum class skip_type {write, read};
enum class debug_type {hex, chars};

class block {
public:
	block(size_t capacity);
	~block();

	size_t capacity() const;
	size_t free() const;
	size_t size() const;
	void* malloc();
	void* data() const;
	void reset();
	size_t skip(skip_type type, size_t length);
	size_t append(const block& other);

	size_t write(void* src, size_t length, bool skip = true);
	size_t read(void* des, size_t length, bool skip = true);

	void debug(debug_type type = debug_type::hex);

private:
	uint8_t* _data = nullptr;
	size_t _pos = 0;
	size_t _capacity = 0;
	size_t _head = 0;

};

class block_buffer {
public:
	block_buffer();
	~block_buffer();

	block* pop();
	void clear();
	size_t size();
	block* merge();
	void* malloc(size_t size);
	size_t merge(block_buffer& buffer);
	size_t append(block_buffer& buffer);
	size_t skip(skip_type type, size_t length);
	size_t write(void* src, size_t length, bool skip = true);
	size_t read(void* des, size_t length, bool skip = true);

	block* allocate(size_t capacity);
	void free(block* _block);
	void push(block* _block);

	void debug(debug_type type = debug_type::hex);

private:
	block* get_block(size_t size);
	void remove_free_block(block* _block);

	std::list<block*> _blocks;
	std::list<block*> _free_blocks;
};

}

#endif

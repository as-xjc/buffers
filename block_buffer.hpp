// The MIT License (MIT)
//
// Copyright (c) 2014 xiao jian cheng
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// SOFTWARE.
//

#pragma once

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
	block_buffer(size_t min_block_size = 1024);
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

	block* allocate(size_t capacity = 1);
	void free(block* _block);
	void push(block* _block);

	void debug(debug_type type = debug_type::hex);

private:
	block* get_block(size_t size);
	void remove_free_block(block* _block);

	size_t calc_block_size(size_t size);

	std::list<block*> _blocks;
	std::list<block*> _free_blocks;

	const size_t _min_block_size;
};

}

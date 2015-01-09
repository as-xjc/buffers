#include <cstdio>
#include <cstring>
#include <cassert>
#include "block_buffer.hpp"

namespace buffer {

block::block(size_t capacity)
{
	assert(capacity > 0);
	_data = new uint8_t[capacity];
	_capacity = capacity;
}

block::~block()
{
	delete[] _data;
}

size_t block::capacity() const
{
	return _capacity;
}

size_t block::free() const
{
	return _capacity - _pos;
}

size_t block::size() const
{
	return _pos - _head;
}

void* block::malloc()
{
	return &_data[_pos];
}

void* block::data() const
{
	return &_data[_head];
}

void block::reset()
{
	_pos = 0;
	_head = 0;
}

size_t block::skip(skip_type type, size_t length)
{
	if (length < 1) return 0;

	size_t skip_length = length;
	if (type == skip_type::write) {
		if (free() < length) skip_length = free();
		_pos += skip_length;
	} else {
		if (size() < length) skip_length = size();
		_head += skip_length;
	}

	return skip_length;
}

size_t block::append(const block& other)
{
	auto length = other.size();
	if (length < 1) return 0;

	if (free() < length) length = free();

	std::memcpy(this->malloc(), other.data(), length);
	return this->skip(skip_type::write, length);
}

size_t block::write(void* src, size_t length, bool skip)
{
	if (length < 1) return 0;

	auto free_size = free();
	auto write_size = length;

	if (length > free_size) write_size = free_size;
	
	std::memcpy(this->malloc(), src, write_size);

	if (skip) _pos += write_size;

	return write_size;
}

size_t block::read(void* des, size_t length, bool skip)
{
	if (length < 1) return 0;

	auto used_size = size();
	auto write_size = length;

	if (length > used_size) write_size = used_size;
	
	std::memcpy(des, this->data(), write_size);
	
	if (skip) _head += write_size;

	return write_size;
}

void block::debug(debug_type type)
{
	int format_offset = 16;
	std::printf("capacity:%d, used:%d, free:%d\n", capacity(), size(), free());
	if (size() < 1) {
		std::printf("    <none>");
		return;
	}

	std::printf("    ");
	int index = 0;
	for (int i = _head; i < _pos; i++) {
		switch (type) {
			case debug_type::hex :
				std::printf("%3x", _data[i]);
				break;
			case debug_type::chars :
				std::printf("%c", _data[i]);
				break;
			default:
				std::printf("%d", _data[i]);
		}

		index++;
		if (index % format_offset == 0 && i != (_pos-1)) std::printf("\n    ");
	}
}

block_buffer::block_buffer(size_t min_block_size):
_min_block_size(min_block_size)
{
	assert(min_block_size > 0);
}

block_buffer::~block_buffer()
{
	for (auto _block: _blocks) {
		delete _block;
	}

	for (auto _block : _free_blocks) {
		delete _block;
	}
}

size_t block_buffer::size()
{
	size_t total = 0;
	for (auto _block : _blocks) {
		total += _block->size();
	}

	return total;
}

void block_buffer::clear()
{
	for (auto _block : _blocks) free(_block);
	_blocks.clear();
}

block* block_buffer::merge()
{
	if (_blocks.size() <= 1) return nullptr;

	size_t total = 0;
	for (auto _block : _blocks) total += _block->size();

	if (total < 1) return nullptr;

	auto total_block = allocate(total);
	for (auto _block: _blocks) {
		if (_block->size() > 0) {
			total_block->append(*_block);
		}

		free(_block);
	}
	_blocks.clear();

	this->push(total_block);
	return total_block;
}

void* block_buffer::malloc(size_t size)
{
	auto tmp = get_block(size);
	return tmp->malloc();	 
}

std::tuple<void*, size_t> block_buffer::malloc()
{
	block* tmp = nullptr;
	if (_blocks.empty()) tmp = new_push_block(_min_block_size);
	else {
		tmp = _blocks.back();
		if (tmp->free() < 1) tmp = new_push_block(_min_block_size);
	}

	return std::make_tuple(tmp->malloc(), tmp->free());
}

size_t block_buffer::skip(skip_type type, size_t length)
{
	if (_blocks.empty()) return 0;

	if (type == skip_type::write) {
		auto tmp = _blocks.back();
		return tmp->skip(skip_type::write, length);
	} else {
		size_t skip_length = 0;
		for (auto it = _blocks.begin(); it != _blocks.end();) {
			if (skip_length >= length) break;

			skip_length += (*it)->skip(skip_type::read, length);

			if ((*it)->size() < 1) {
				free(*it);
				it = _blocks.erase(it);
			} else ++it;
		}

		return skip_length;
	}

	return 0;
}

void block_buffer::debug(debug_type type)
{
	std::printf("******************** Debug information ********************\n");
	std::printf("in use:");
	int index = 0;
	for (auto _block: _blocks) {
		std::printf("\nblock[%d] ", index++);
		_block->debug(type);
	}
	std::printf("\n-----------------------------------------------------------\n");
	std::printf("in free:");
	index = 0;
	for (auto _block: _free_blocks) {
		std::printf("\nblock[%d] capacity:%d", index++, _block->capacity());
	}
	std::printf("\n***********************************************************\n");
}

void block_buffer::free(block* _block)
{
	_block->reset();

	if (_free_blocks.empty()) {
		_free_blocks.push_back(_block);
		return;
	}

	for (auto it = _free_blocks.begin(); it != _free_blocks.end(); ++it) {
		if (_block->capacity() <= (*it)->capacity()) {
			_free_blocks.insert(it, _block);	 
			return;
		}
	}

	_free_blocks.push_back(_block);
}

block* block_buffer::allocate(size_t capacity)
{
	if (_free_blocks.empty()) {
		return new block(calc_block_size(capacity));
	}

	block* found = nullptr;
	for (auto it = _free_blocks.begin(); it != _free_blocks.end(); ++it) {
		if (capacity <= (*it)->capacity()) {
			found = *it;
			_free_blocks.remove(found);
			break;
		}
	}

	if (found == nullptr) found = new block(calc_block_size(capacity));

	return found;
}

block* block_buffer::get_block(size_t size)
{
	if (_blocks.empty()) return new_push_block(size);

	auto back = _blocks.back();
	if (size > back->free()) back = new_push_block(size);

	return back;
}

size_t block_buffer::write(void* src, size_t length, bool skip)
{
	if (length < 1) return 0;

	auto tmp = get_block(length);
	return tmp->write(src, length, skip);
}

size_t block_buffer::write(block_buffer& buffer)
{
	size_t total = 0;
	for (;;) {
		auto block = buffer.pop();
		if (block == nullptr) break;

		total += this->write(block->data(), block->size());

		buffer.free(block);
	}

	return total;
}

size_t block_buffer::read(void* des, size_t length, bool skip)
{
	if (length < 1 || _blocks.empty()) return 0;

	size_t read_pos = 0;
	size_t need_read = length;

	for (auto it = _blocks.begin(); it != _blocks.end();) {
		if (need_read < 1) break;

		if ((*it)->size() < 1) {
			free(*it);
			it = _blocks.erase(it);
			continue;
		}

		size_t cur_read = need_read;
		if ((*it)->size() < need_read) cur_read = (*it)->size();

		size_t ret = (*it)->read(&((uint8_t*)des)[read_pos], cur_read, skip);
		read_pos += ret;
		need_read -= ret;

		if ((*it)->size() < 1) {
			free(*it);
			it = _blocks.erase(it);
		} else ++it;
	}

	return read_pos;
}

size_t block_buffer::merge(block_buffer& buffer)
{
	size_t total = 0;

	for (;;) {
		auto _block = buffer.pop();
		if (_block == nullptr) break;

		total += _block->size();
		this->push(_block);
	}

	return total;
}

size_t block_buffer::append(block_buffer& buffer)
{
	size_t total = 0;
	for (auto _block : buffer._blocks) {
		if (_block->size() < 1) continue;

		total += _block->size();
		this->write(_block->data(), _block->size());
	}

	return total;
}

block* block_buffer::pop()
{
	if (_blocks.empty()) return nullptr;

	auto front = _blocks.front();
	_blocks.pop_front();

	return front;
}

void block_buffer::push(block* _block)
{
	if (_block->size()) _blocks.push_back(_block);
	else free(_block);
}

std::list<block*>& block_buffer::blocks()
{
	return _blocks;
}

block* block_buffer::new_push_block(size_t size)
{
	auto _block = allocate(size);
	_blocks.push_back(_block);
	return _block;
}

size_t block_buffer::calc_block_size(size_t size)
{
	size_t allocate_size = size / _min_block_size;
	if (size % _min_block_size > 0) allocate_size++;

	if (allocate_size < 1) allocate_size = 1;

	allocate_size *= _min_block_size;

	return allocate_size;
}

}

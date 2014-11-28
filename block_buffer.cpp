#include <cstdio>
#include <cstring>
#include <set>
#include "block_buffer.hpp"

namespace {

const size_t buff_base_size = 1024;

size_t calc_size(size_t size)
{
	size_t allocate_size = size/buff_base_size;
	if (size%buff_base_size> 0) allocate_size++;

	if (allocate_size < 1) allocate_size= 1;

	allocate_size *= buff_base_size;

	return allocate_size;
}

}

namespace buffer {

block::block(size_t capacity)
{
	size_t allocate_size = calc_size(capacity);

	_data = new char[allocate_size];
	_capacity = allocate_size;
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

char* block::malloc()
{
	return &_data[_pos];
}

void block::reset()
{
	_pos = 0;
	_head = 0;
}

size_t block::skip(size_t length)
{
	size_t skip_length = length;
	if (free() < length) skip_length = free();

	_pos += skip_length;

	return skip_length;
}

size_t block::append(const block& other)
{
	size_t length = other.size();
	if (length < 1) return 0;

	if (free() < length) length = free();

	std::strncpy(this->malloc(), &other._data[other._head], length);
	return this->skip(length);
}

size_t block::write(char* src, size_t length, bool skip)
{
	size_t free_size = free();
	size_t write_size = length;

	if (length > free_size) write_size = free_size;
	
	std::strncpy(&_data[_pos], src, write_size);

	if (skip) _pos += write_size;

	return write_size;
}

size_t block::read(char* des, size_t length, bool skip)
{
	size_t used_size = size();
	size_t write_size = length;

	if (length > used_size) write_size = used_size;
	
	std::strncpy(des, &_data[_head], write_size);
	
	if (skip) _head += write_size;

	return write_size;
}

void block::debug(size_t format_offset)
{
	std::printf("capacity:%d, used:%d, free:%d\n", capacity(), size(), free());
	if (size() < 1) return;

	int index = 0;
	for (int i = _head; i < _pos; i++) {
		std::printf("%c", _data[i]);
		index++;
		if (index % format_offset == 0 && i != (_pos-1)) std::printf("\n");
	}
}

block_buffer::block_buffer()
{
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
	for (auto _block : _blocks) free_block(_block);
	_blocks.clear();
}

void block_buffer::merge()
{
	if (_blocks.size() <= 1) return;

	size_t total = 0;
	for (auto _block : _blocks) total += _block->size();

	if (total < 1) return;

	block* total_block = allocate_block(total);
	for (auto _block: _blocks) {
		if (_block->size() > 0) {
			total_block->append(*_block);
		}

		free_block(_block);
	}
	_blocks.clear();

	_blocks.push_back(total_block);
}

char* block_buffer::malloc(size_t size)
{
	block* tmp = get_block(size);
	return tmp->malloc();	 
}

size_t block_buffer::skip(size_t length)
{
	if (_blocks.empty()) return 0;

	block* tmp = _blocks.back();
	if (tmp == nullptr) return 0;
	return tmp->skip(length);
}

void block_buffer::debug(size_t format_offset)
{
	std::printf("********** Debug information **********\n");
	std::printf("in use:");
	int index = 0;
	for (auto _block: _blocks) {
		std::printf("\nblock[%d] ", index++);
		_block->debug(format_offset);
	}
	std::printf("\n---------------------------------------\n");
	std::printf("in free:");
	index = 0;
	for (auto _block: _free_blocks) {
		std::printf("\nblock[%d] capacity:%d", index++, _block->capacity());
	}
	std::printf("\n***************************************\n");
}

void block_buffer::free_block(block* _block)
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

block* block_buffer::allocate_block(size_t capacity)
{
	if (_free_blocks.empty()) {
		return new block(capacity);
	}

	block* found = nullptr;
	for (auto it = _free_blocks.begin(); it != _free_blocks.end(); ++it) {
		if (capacity <= (*it)->capacity()) {
			found = *it;
			_free_blocks.remove(found);
			break;
		}
	}

	if (found == nullptr) found = new block(capacity);

	return found;
}

block* block_buffer::get_block(size_t size)
{
	if (_blocks.empty()) {
		block* tmp = allocate_block(size);
		_blocks.push_back(tmp);
		return tmp;
	}

	block* back = _blocks.back();
	if (size > back->free()) {
		back = allocate_block(size);
		_blocks.push_back(back);
	}

	return back;
}

size_t block_buffer::write(char* src, size_t length, bool skip)
{
	block* tmp = get_block(length);
	return tmp->write(src, length, skip);
}

size_t block_buffer::read(char* des, size_t length, bool skip)
{
	if (_blocks.empty()) return 0;

	size_t read_pos = 0;
	size_t need_read = length;
	std::set<block*> delBlock;

	for (auto _block : _blocks) {
		if (need_read < 1) break;

		if (_block->size() < 1) {
			delBlock.insert(_block);
			continue;
		}

		size_t cur_read = need_read;
		if (_block->size() < need_read) cur_read = _block->size();

		size_t ret = _block->read(&des[read_pos], cur_read, skip);
		read_pos += ret;
		need_read -= ret;

		if (_block->size() < 1) delBlock.insert(_block);
	}

	for (auto it : delBlock) {
		_blocks.remove(it);
		free_block(it);
	}

	return read_pos;
}

}

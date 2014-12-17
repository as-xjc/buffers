#include <iostream>
#include <cstring>
#include <iostream>
#include <string>

#include "block_buffer.hpp"

static char* progress = nullptr;

void test_move_append()
{
	buffer::block_buffer buff_1(2);
	buffer::block_buffer buff_2(100);

	char _1[] = "this is buff 1";
	char _2[] = "this is buff 2";

	buff_1.write(_1, sizeof(_1) - 1);
	buff_2.write(_2, sizeof(_2) - 1);

	buff_1.debug(buffer::debug_type::chars);
	buff_2.debug(buffer::debug_type::chars);

	buff_1.merge(buff_2);

	buff_1.debug(buffer::debug_type::chars);
	buff_2.debug(buffer::debug_type::chars);

	buff_2.append(buff_1);

	buff_1.debug(buffer::debug_type::chars);
	buff_2.debug(buffer::debug_type::chars);
}

void test_string_merge()
{
	buffer::block_buffer buff;

	buffer::block* _block = buff.allocate(100);
	char a[] = "1234";
	_block->write(a, sizeof(a)-1);
	buff.push(_block);

	buffer::block* _block2 = buff.allocate(100);
	char aa[] = "56789";
	_block2->write(aa, sizeof(aa)-1);
	buff.push(_block2);

	buff.debug();
	buff.merge();
	buff.debug();
}

void test_file()
{
	buffer::block_buffer buff;

	std::FILE* file = std::fopen(progress, "rb");
	if (file == nullptr) {
		std::cout << "read file error" << std::endl;
		return;
	}

	while (!std::feof(file)) {
		buffer::block* _block = buff.allocate();
		size_t ret = std::fread(_block->malloc(), sizeof(char), _block->free(), file);
		_block->skip(buffer::skip_type::write, ret);
		buff.push(_block);
	}

	buff.debug();

	std::fclose(file);

	std::string path = progress;
	path += "_test_file";

	std::FILE* wfile = std::fopen(path.c_str(), "wb");
	size_t length = buff.size();
	std::cout << "total read size:" << length << std::endl;

	buffer::block* _block = nullptr;
	while ((_block = buff.pop()) != nullptr) {
		std::fwrite(_block->data(), 1, _block->size(), wfile);
		buff.free(_block);
	}

	buff.debug();

	std::fclose(wfile);
}

void test_merge_file()
{
	buffer::block_buffer buff;

	std::FILE* file = std::fopen(progress, "rb");
	if (file == nullptr) {
		std::cout << "read file error" << std::endl;
		return;
	}

	while (!std::feof(file)) {
		buffer::block* _block = buff.allocate();
		size_t ret = std::fread(_block->malloc(), sizeof(char), _block->free(), file);
		_block->skip(buffer::skip_type::write, ret);
		buff.push(_block);
	}

	std::fclose(file);

	std::string path = progress;
	path += "_test_merge_file";
	std::FILE* wfile = std::fopen(path.c_str(), "wb");

	size_t length = buff.size();
	auto _block = buff.merge();
	std::cout << "total read size:" << buff.size() << std::endl;
	std::fwrite(_block->data(), 1, _block->size(), wfile);

	buff.debug();
	buff.skip(buffer::skip_type::read, _block->size());
	buff.debug();

	std::fclose(wfile);
}

void test_malloc()
{
	buffer::block_buffer buff;
	buff.debug();
	void* b;
	size_t len;
	std::tie(b, len) = buff.malloc();
	buff.debug();
	std::cout << "malloc len:" << len << std::endl;
	std::tie(b, len) = buff.malloc();
	buff.debug();
	std::cout << "malloc len:" << len << std::endl;
}

void test_crc32()
{
	char _1[] = "this is buff 1, it used to test crc32";

	buffer::block_buffer buff_1;
	buffer::block_buffer buff_2(2);
	buff_1.write(_1, sizeof(_1)-1);
	buff_1.write(_1, sizeof(_1)-1);
	buff_1.debug();
	buff_2.write(_1, sizeof(_1)-1);
	buff_2.write(_1, sizeof(_1)-1);
	buff_2.debug();
	std::cout << "buff 1 crc32 :" << buff_1.crc32() << std::endl;
	std::cout << "buff 2 crc32 :" << buff_2.crc32() << std::endl;
}

int main(int argc, char* argv[])
{
	progress = argv[0];
	std::cout << " ------------- test for buff -------------" << std::endl;
	std::cout << " * test merge" << std::endl;
	test_string_merge();
	std::cout << " * test file" << std::endl;
	test_file();
	std::cout << " * test merge to file" << std::endl;
	test_merge_file();
	std::cout << " * test block merge/append" << std::endl;
	test_move_append();
	std::cout << " * test malloc" << std::endl;
	test_malloc();
	std::cout << " * test crc32" << std::endl;
	test_crc32();

	return 0;
}

#include <iostream>
#include <cstdio>

#include "buffer.hpp"

static char *progress = nullptr;

void test_move_append() {
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

void test_string_merge() {
  buffer::block_buffer buff(512, 3);
  buff.debug();

  buffer::block_ptr _block = buff.pop_free(100);
  char a[] = "1234";
  _block->write(a, sizeof(a) - 1);
  buff.push(_block);

  buffer::block_ptr _block2 = buff.pop_free(100);
  char aa[] = "56789";
  _block2->write(aa, sizeof(aa) - 1);
  buff.push(_block2);

  buff.debug();
  buff.merge();
  buff.debug();
}

void test_file() {
  buffer::block_buffer buff;

  std::FILE *file = std::fopen(progress, "rb");
  if (file == nullptr) {
    std::cout << "read file error" << std::endl;
    return;
  }

  while (!std::feof(file)) {
    buffer::block_ptr _block = buff.allocate(20 * 1024);
    size_t ret = std::fread(_block->malloc(), sizeof(char), _block->free(), file);
    _block->skip(buffer::skip_type::write, ret);
  }

  buff.debug();
  buff.merge();
  buff.debug();

  std::fclose(file);

  std::string path = progress;
  path += "_test_file";

  std::FILE *wfile = std::fopen(path.c_str(), "wb");
  size_t length = buff.size();
  std::cout << "total read size:" << length << std::endl;

  buffer::block_ptr _block;
  while (!buff.empty()) {
    auto _block = buff.pop();
    std::fwrite(_block->data(), 1, _block->size(), wfile);
    buff.recover(_block);
  }

  buff.debug();

  std::fclose(wfile);
}

void test_merge_file() {
  buffer::block_buffer buff;

  std::FILE *file = std::fopen(progress, "rb");
  if (file == nullptr) {
    std::cout << "read file error" << std::endl;
    return;
  }

  while (!std::feof(file)) {
    buffer::block_ptr _block = buff.allocate();
    size_t ret = std::fread(_block->malloc(), sizeof(char), _block->free(), file);
    _block->skip(buffer::skip_type::write, ret);
  }

  std::fclose(file);

  std::string path = progress;
  path += "_test_merge_file";
  std::FILE *wfile = std::fopen(path.c_str(), "wb");

  auto _block = buff.merge();
  std::cout << "total read size:" << buff.size() << std::endl;
  std::fwrite(_block->data(), 1, _block->size(), wfile);

  buff.debug();
  buff.skip(buffer::skip_type::read, _block->size());
  buff.debug();

  std::fclose(wfile);
}

void test_malloc() {
  buffer::block_buffer buff;
  void *b = nullptr;
  size_t len;
  std::tie(b, len) = buff.malloc();
  buff.debug();
  std::cout << "malloc len:" << len << std::endl;
  std::tie(b, len) = buff.malloc();
  buff.debug();
  std::cout << "malloc len:" << len << std::endl;
}

void test_bytebuffer()
{
  buffer::ByteBuffer buff;
  buff.debug();
  char* short_ = "1234";
  buff.write(short_, std::strlen(short_));
  buff.debug();
  char* test = "1234567890";
  buff.write(test, std::strlen(test));
  buff.debug();
  buff.zero();
  buff.write(test, std::strlen(test));
  buff.debug();

  char a[2];
  buff.read(a, 2);
  buff.debug();
  buff.write(a, 2);
  buff.debug();
}

int main(int argc, char *argv[]) {
  progress = argv[0];
  std::cout << " ------------- test for buff -------------" << std::endl;
  std::cout << " 1 test merge" << std::endl;
  test_string_merge();
  std::cout << " 2 test file" << std::endl;
  test_file();
  std::cout << " 3 test merge to file" << std::endl;
  test_merge_file();
  std::cout << " 4 test block merge/append" << std::endl;
  test_move_append();
  std::cout << " 5 test malloc" << std::endl;
  test_malloc();
  std::cout << " 6 test byte buffer" << std::endl;
  test_bytebuffer();

  return 0;
}

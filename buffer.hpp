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
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include <cstdint>
#include <list>
#include <tuple>
#include <memory>

namespace buffer {

typedef uint8_t byte;

enum class skip_type { write, read };
enum class debug_type { hex, chars };

const static size_t MIN_BUFFER_CAPACITY = 8;
const static size_t DEBUG_FORMAT_OFFSET = 16;

class BaseBuffer {
 public:
  virtual ~BaseBuffer() {}

  virtual size_t write(void* src, size_t len, bool skip = true) = 0;
  virtual size_t read(void* des, size_t len, bool skip = true) = 0;
  virtual size_t skip(skip_type type, size_t length) = 0;
  virtual void debug(debug_type type = debug_type::hex) = 0;
  virtual size_t capacity() = 0;
  virtual size_t size() = 0;
  virtual size_t free() = 0;
  virtual byte* data() = 0;

  template<typename T>
  T* cast_ptr(size_t len = 1) {
    if (continuous() < (len > 0 ? len : 1) * sizeof(T)) return nullptr;

    return reinterpret_cast<T*>(data());
  }

 protected:
  BaseBuffer() {};
  virtual size_t continuous() = 0;

  byte* data_ = nullptr;
};

class ByteBuffer : public BaseBuffer {
 public:
  ByteBuffer(size_t capacity = MIN_BUFFER_CAPACITY) : capacity_(
      capacity < MIN_BUFFER_CAPACITY ? MIN_BUFFER_CAPACITY : capacity) {
    data_ = new byte[capacity_];
  }

  virtual ~ByteBuffer() { delete[] data_; }

  void zero() {
    std::memset(reinterpret_cast<void*>(data_), 0, capacity_);
    head_ = 0;
    tail_ = 0;
  }

  size_t capacity() { return capacity_; }
  size_t size() { return tail_ - head_; }
  size_t free() { return capacity_ - tail_; }
  byte* data() { return &data_[head_]; }
  byte* tail() { return &data_[tail_]; }

  size_t write(void* src, size_t len, bool skip = true) {
    if (free() < len) grow(size() + len);

    std::memcpy(reinterpret_cast<void*>(tail()), src, len);

    if (skip) this->skip(skip_type::write, len);

    return len;
  }

  size_t read(void* des, size_t len, bool skip = true) {
    if (size() < 1) return 0;

    size_t rlen = size() < len ? size() : len;
    std::memcpy(des, reinterpret_cast<void*>(data()), rlen);

    if (skip) this->skip(skip_type::read, rlen);
    if (head_ == tail_) {
      head_ = 0;
      tail_ = 0;
    }
    return rlen;
  }

  size_t skip(skip_type type, size_t length) {
    if (type == skip_type::read) {
      head_ += length;
    } else {
      tail_ += length;
    }

    return length;
  }

  void debug(debug_type type = debug_type::hex) {
    std::printf("%p capacity:%zu, used:%zu, free:%zu, head:%zu, tail:%zu\n", data_, capacity(), size(), free(), head_, tail_);
    if (size() < 1) {
      std::printf("    <none>\n");
      return;
    }

    std::printf("    ");
    int index = 0;
    for (size_t i = 0; i < capacity_; ++i) {
      switch (type) {
        case debug_type::hex :std::printf("%3x", data_[i]);
          break;
        case debug_type::chars :std::printf("%c", data_[i]);
          break;
      }

      ++index;
      if (index % DEBUG_FORMAT_OFFSET == 0 && i != (capacity_ - 1)) std::printf("\n    ");
    }
    std::printf("\n");
  }

 protected:
  size_t continuous() { return size(); }
  void grow (size_t capacity) {
    size_t newcapacity = capacity_;
    while (newcapacity < capacity) {
      newcapacity *= 2;
    }
    byte* newdata = new byte[newcapacity];

    if (size() > 0) {
      std::memcpy(reinterpret_cast<void*>(newdata), reinterpret_cast<void*>(data()), size());
      tail_ = size();
    } else {
      tail_ = 0;
    }

    head_ = 0;
    capacity_ = newcapacity;

    delete[] data_;
    data_ = newdata;
  }

 private:
  size_t capacity_{0};
  size_t head_{0};
  size_t tail_{0};
};

class block;

typedef std::shared_ptr<block> block_ptr;

class block_buffer;

class block {
 public:
  ~block() { delete[] data_; }

  static block_ptr allocate(size_t size) { return block_ptr(new block(size)); }

  size_t capacity() const { return capacity_; }

  size_t free() const { return capacity_ - pos_; }

  size_t size() const { return pos_ - head_; }

  void* malloc() { return &data_[pos_]; }

  void* data() const { return &data_[head_]; }

  void reset() {
    pos_ = 0;
    head_ = 0;
  }

  size_t skip(skip_type type, size_t length) {
    if (length < 1) return 0;

    size_t skip_length = length;
    if (type == skip_type::write) {
      if (free() < length) skip_length = free();
      pos_ += skip_length;
    } else {
      if (size() < length) skip_length = size();
      head_ += skip_length;
    }

    return skip_length;
  }

  size_t append(const block_ptr &other) {
    auto length = other->size();
    if (length < 1) return 0;

    if (free() < length) length = free();

    std::memcpy(malloc(), other->data(), length);
    return skip(skip_type::write, length);
  }

  size_t write(void* src, size_t length, bool skip = true) {
    if (length < 1) return 0;

    auto free_size = free();
    auto write_size = length;

    if (length > free_size) write_size = free_size;

    std::memcpy(malloc(), src, write_size);

    if (skip) pos_ += write_size;

    return write_size;
  }

  size_t read(void* des, size_t length, bool skip = true) {
    if (length < 1) return 0;

    auto used_size = size();
    auto write_size = length;

    if (length > used_size) write_size = used_size;

    std::memcpy(des, data(), write_size);

    if (skip) head_ += write_size;

    return write_size;
  }

  void debug(debug_type type = debug_type::hex) {
    int format_offset = 16;
    std::printf("capacity:%zu, used:%zu, free:%zu\n", capacity(), size(), free());
    if (size() < 1) {
      std::printf("    <none>");
      return;
    }

    std::printf("    ");
    int index = 0;
    for (size_t i = head_; i < pos_; ++i) {
      switch (type) {
        case debug_type::hex :std::printf("%3x", data_[i]);
          break;
        case debug_type::chars :std::printf("%c", data_[i]);
          break;
      }

      ++index;
      if (index % format_offset == 0 && i != (pos_ - 1)) std::printf("\n    ");
    }
  }

 private:
  block() = delete;

  block(size_t capacity) {
    data_ = new uint8_t[capacity];
    capacity_ = capacity;
  }

  uint8_t* data_{nullptr};
  size_t pos_{0};
  size_t capacity_{0};
  size_t head_{0};

};

class block_buffer {
 public:
  block_buffer(size_t min_block_size = 1024, size_t block_size = 0) : min_block_size_(min_block_size) {
    if (max_block_size_ < block_size) max_block_size_ = block_size;

    for (size_t i = 0; i < block_size; ++i) {
      auto block = block::allocate(calc_block_size(min_block_size_));
      recover(block);
    }
  }

  ~block_buffer() {}

  block_ptr pop() {
    if (blocks_.empty()) return nullptr;

    block_ptr front = blocks_.front();
    blocks_.pop_front();

    return front;
  }

  block_ptr pop_free(size_t capacity) {
    if (free_blocks_.empty()) return block::allocate(calc_block_size(capacity));

    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
      if (capacity <= (*it)->capacity()) {
        block_ptr block = *it;
        free_blocks_.erase(it);
        return block;
      }
    }

    return block::allocate(calc_block_size(capacity));
  }

  void push(block_ptr block) {
    blocks_.push_back(block);
  }

  void recover(block_ptr block) {
    block->reset();

    if (free_blocks_.empty()) {
      free_blocks_.push_back(block);
    } else {
      bool insert = false;
      for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        if (block->capacity() <= (*it)->capacity()) {
          free_blocks_.insert(it, block);
          insert = true;
          break;
        }
      }
      if (!insert) free_blocks_.push_back(block);
    }

    while (free_blocks_.size() > max_block_size_) {
      free_blocks_.pop_front();
    }
  }

  void clear() {
    for (auto &_block : blocks_) recover(_block);
    blocks_.clear();
  }

  bool empty() { return blocks_.empty(); }

  size_t size() {
    size_t total = 0;
    for (auto &_block : blocks_) {
      total += _block->size();
    }

    return total;
  }

  block_ptr allocate(size_t capacity = 1) {
    if (free_blocks_.empty()) {
      auto tmp = block::allocate(calc_block_size(capacity));
      push(tmp);
      return tmp;
    }

    block_ptr found;
    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
      if (capacity <= (*it)->capacity()) {
        found = *it;
        free_blocks_.erase(it);
        break;
      }
    }

    if (!found) found = block::allocate(calc_block_size(capacity));
    push(found);
    return found;
  }

  void set_max_block_size(size_t size) { max_block_size_ = size; }

  void* malloc(size_t size) {
    block_ptr tmp;
    if (blocks_.empty()) {
      tmp = allocate(size);
    } else {
      tmp = blocks_.back();
      if (tmp->free() < size) tmp = allocate(size);
    }
    return tmp->malloc();
  }

  std::tuple<void*, size_t> malloc() {
    block_ptr tmp;
    if (blocks_.empty()) {
      tmp = allocate();
    } else {
      tmp = blocks_.back();
      if (tmp->free() < 1) tmp = allocate();
    }

    return std::make_tuple(tmp->malloc(), tmp->free());
  };

  block_ptr merge() {
    if (blocks_.empty()) return nullptr;

    size_t total = size();

    if (total < 1) return nullptr;

    block_ptr total_block;
    if (blocks_.front()->capacity() >= total) {
      total_block = blocks_.front();
      blocks_.pop_front();
    } else {
      total_block = allocate(total);
    }

    for (auto &_block: blocks_) {
      if (_block->size() > 0) {
        total_block->append(_block);
      }

      recover(_block);
    }

    blocks_.clear();
    blocks_.push_back(total_block);
    return total_block;
  }

  size_t merge(block_buffer &buffer) {
    size_t total = 0;

    for (;;) {
      auto _block = buffer.pop();
      if (_block == nullptr) break;

      total += _block->size();
      this->push(_block);
    }

    return total;
  }

  size_t append(block_buffer &buffer) {
    size_t total = 0;
    for (auto &_block : buffer.blocks_) {
      if (_block->size() < 1) continue;

      total += _block->size();
      this->write(_block->data(), _block->size());
    }

    return total;
  }

  size_t skip(skip_type type, size_t length) {
    if (blocks_.empty()) return 0;

    if (type == skip_type::write) {
      auto &tmp = blocks_.back();
      return tmp->skip(skip_type::write, length);
    } else {
      size_t skip_length = 0;
      for (auto it = blocks_.begin(); it != blocks_.end();) {
        if (skip_length >= length) break;

        skip_length += (*it)->skip(skip_type::read, length);

        if ((*it)->size() < 1) {
          recover(*it);
          it = blocks_.erase(it);
        } else ++it;
      }

      return skip_length;
    }
  }

  size_t write(void* src, size_t length, bool skip = true) {
    if (length < 1) return 0;

    block_ptr tmp;
    if (blocks_.empty()) {
      tmp = allocate(length);
    } else {
      tmp = blocks_.back();
    }

    if (skip) {
      if (tmp->free() < 1) tmp = allocate(length);

      size_t remain = length;
      remain -= tmp->write(src, tmp->free() > remain ? remain : tmp->free(), skip);
      if (remain > 0) {
        tmp = allocate(remain);
        uint8_t* newsrc = reinterpret_cast<uint8_t*>(src) + (length - remain);
        tmp->write(reinterpret_cast<void*>(newsrc), remain, skip);
      }
      return length;
    } else {
      if (tmp->free() < length) tmp = allocate(length);
      return tmp->write(src, length, skip);
    }
  }

  size_t write(block_buffer &buffer) {
    size_t total = 0;
    for (;;) {
      auto block = buffer.pop();
      if (block == nullptr) break;

      total += write(block->data(), block->size());

      buffer.recover(block);
    }

    return total;
  }

  size_t read(void* des, size_t length, bool skip = true) {
    if (length < 1 || blocks_.empty()) return 0;

    size_t read_pos = 0;
    size_t need_read = length;

    for (auto it = blocks_.begin(); it != blocks_.end();) {
      if (need_read < 1) break;

      if ((*it)->size() < 1) {
        recover(*it);
        it = blocks_.erase(it);
        continue;
      }

      size_t cur_read = need_read;
      if ((*it)->size() < need_read) cur_read = (*it)->size();

      size_t ret = (*it)->read(&(reinterpret_cast<uint8_t*>(des))[read_pos], cur_read, skip);
      read_pos += ret;
      need_read -= ret;

      if ((*it)->size() < 1) {
        recover(*it);
        it = blocks_.erase(it);
      } else {
        ++it;
      }
    }

    return read_pos;
  }

  void debug(debug_type type = debug_type::hex) {
    std::printf("******************** Debug information ********************\n");
    std::printf("in use:");
    int index = 0;
    if (blocks_.empty()) {
      std::printf("\n  <none> ");
    } else {
      for (auto &block: blocks_) {
        std::printf("\n  block[%d:%p] ", ++index, block.get());
        block->debug(type);
      }
    }
    std::printf("\n-----------------------------------------------------------\n");
    std::printf("in free:");
    index = 0;
    if (free_blocks_.empty()) {
      std::printf("\n  <none> ");
    } else {
      for (auto &block: free_blocks_) {
        std::printf("\n  block[%d:%p] capacity:%zu", ++index, block.get(), block->capacity());
      }
    }
    std::printf("\n***********************************************************\n");
  }

 private:
  size_t calc_block_size(size_t size) {
    size_t allocate_size = size / min_block_size_;
    if (size % min_block_size_ > 0) ++allocate_size;

    if (allocate_size < 1) allocate_size = 1;

    allocate_size *= min_block_size_;

    return allocate_size;
  }

  std::list<block_ptr> blocks_;
  std::list<block_ptr> free_blocks_;

  const size_t min_block_size_{0};

  size_t max_block_size_{10};
};

}

#include "block_buffer.hpp"

#include <cstdio>
#include <cassert>

namespace buffer {

    block::block(size_t capacity) {
        assert(capacity > 0);
        data_ = new uint8_t[capacity];
        capacity_ = capacity;
    }

    block::~block() {
        delete[] data_;
    }

    size_t block::capacity() const {
        return capacity_;
    }

    size_t block::free() const {
        return capacity_ - pos_;
    }

    size_t block::size() const {
        return pos_ - head_;
    }

    void *block::malloc() {
        return &data_[pos_];
    }

    void *block::data() const {
        return &data_[head_];
    }

    void block::reset() {
        pos_ = 0;
        head_ = 0;
    }

    size_t block::skip(skip_type type, size_t length) {
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

    size_t block::append(const block_ptr &other) {
        auto length = other->size();
        if (length < 1) return 0;

        if (free() < length) length = free();

        std::memcpy(malloc(), other->data(), length);
        return skip(skip_type::write, length);
    }

    size_t block::write(void *src, size_t length, bool skip) {
        if (length < 1) return 0;

        auto free_size = free();
        auto write_size = length;

        if (length > free_size) write_size = free_size;

        std::memcpy(malloc(), src, write_size);

        if (skip) pos_ += write_size;

        return write_size;
    }

    size_t block::read(void *des, size_t length, bool skip) {
        if (length < 1) return 0;

        auto used_size = size();
        auto write_size = length;

        if (length > used_size) write_size = used_size;

        std::memcpy(des, data(), write_size);

        if (skip) head_ += write_size;

        return write_size;
    }

    void block::debug(debug_type type) {
        int format_offset = 16;
        std::printf("capacity:%zu, used:%zu, free:%zu\n", capacity(), size(), free());
        if (size() < 1) {
            std::printf("    <none>");
            return;
        }

        std::printf("    ");
        int index = 0;
        for (size_t i = head_; i < pos_; i++) {
            switch (type) {
                case debug_type::hex :
                    std::printf("%3x", data_[i]);
                    break;
                case debug_type::chars :
                    std::printf("%c", data_[i]);
                    break;
            }

            index++;
            if (index % format_offset == 0 && i != (pos_ - 1)) std::printf("\n    ");
        }
    }

    block_ptr block::allocate(size_t size) {
        return block_ptr(new block(size));
    }

    block_buffer::block_buffer(size_t min_block_size, size_t block_size) :
            min_block_size_(min_block_size) {
        assert(min_block_size_ > 0);

        if (max_block_size_ < block_size) max_block_size_ = block_size;

        for (size_t i = 0; i < block_size; i++) {
            auto block = block::allocate(calc_block_size(min_block_size_));
            recover(block);
        }
    }

    block_buffer::~block_buffer() {
    }

    size_t block_buffer::size() {
        size_t total = 0;
        for (auto &_block : blocks_) {
            total += _block->size();
        }

        return total;
    }

    void block_buffer::clear() {
        for (auto &_block : blocks_) recover(_block);
        blocks_.clear();
    }

    bool block_buffer::empty() {
        return blocks_.empty();
    }

    block_ptr block_buffer::merge() {
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

    void *block_buffer::malloc(size_t size) {
        block_ptr tmp;
        if (blocks_.empty()) tmp = allocate(size);
        else {
            tmp = blocks_.back();
            if (tmp->free() < size) tmp = allocate(size);
        }
        return tmp->malloc();
    }

    std::tuple<void *, size_t> block_buffer::malloc() {
        block_ptr tmp;
        if (blocks_.empty()) tmp = allocate();
        else {
            tmp = blocks_.back();
            if (tmp->free() < 1) tmp = allocate();
        }

        return std::make_tuple(tmp->malloc(), tmp->free());
    }

    size_t block_buffer::skip(skip_type type, size_t length) {
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

    void block_buffer::debug(debug_type type) {
        std::printf("******************** Debug information ********************\n");
        std::printf("in use:");
        int index = 0;
        if (blocks_.empty()) {
            std::printf("\n  <none> ");
        } else {
            for (auto &block: blocks_) {
                std::printf("\n  block[%d:%p] ", index++, block.get());
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
                std::printf("\n  block[%d:%p] capacity:%zu", index++, block.get(), block->capacity());
            }
        }
        std::printf("\n***********************************************************\n");
    }

    void block_buffer::recover(block_ptr block) {
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

    block_ptr block_buffer::allocate(size_t capacity) {
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

    size_t block_buffer::write(void *src, size_t length, bool skip) {
        if (length < 1) return 0;

        block_ptr tmp;
        if (blocks_.empty()) tmp = allocate(length);
        else {
            tmp = blocks_.back();
        }

        if (skip) {
            if (tmp->free() < 1) tmp = allocate(length);

            size_t remain = length;
            remain -= tmp->write(src, tmp->free() > remain ? remain : tmp->free(), skip);
            if (remain > 0) {
                tmp = allocate(remain);
                uint8_t *newsrc = reinterpret_cast<uint8_t *>(src) + (length - remain);
                tmp->write(reinterpret_cast<void *>(newsrc), remain, skip);
            }
            return length;
        } else {
            if (tmp->free() < length) tmp = allocate(length);
            return tmp->write(src, length, skip);
        }
    }

    size_t block_buffer::write(block_buffer &buffer) {
        size_t total = 0;
        for (;;) {
            auto block = buffer.pop();
            if (block == nullptr) break;

            total += write(block->data(), block->size());

            buffer.recover(block);
        }

        return total;
    }

    size_t block_buffer::read(void *des, size_t length, bool skip) {
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

            size_t ret = (*it)->read(&(reinterpret_cast<uint8_t *>(des))[read_pos], cur_read, skip);
            read_pos += ret;
            need_read -= ret;

            if ((*it)->size() < 1) {
                recover(*it);
                it = blocks_.erase(it);
            } else ++it;
        }

        return read_pos;
    }

    size_t block_buffer::merge(block_buffer &buffer) {
        size_t total = 0;

        for (;;) {
            auto _block = buffer.pop();
            if (_block == nullptr) break;

            total += _block->size();
            this->push(_block);
        }

        return total;
    }

    size_t block_buffer::append(block_buffer &buffer) {
        size_t total = 0;
        for (auto &_block : buffer.blocks_) {
            if (_block->size() < 1) continue;

            total += _block->size();
            this->write(_block->data(), _block->size());
        }

        return total;
    }

    block_ptr block_buffer::pop() {
        if (blocks_.empty()) return nullptr;

        block_ptr front = blocks_.front();
        blocks_.pop_front();

        return front;
    }

    block_ptr block_buffer::pop_free(size_t capacity) {
        if (free_blocks_.empty()) return block::allocate(calc_block_size(capacity));

        for (auto it = free_blocks_.begin(); it != free_blocks_.end(); it++) {
            if (capacity <= (*it)->capacity()) {
                block_ptr block = *it;
                free_blocks_.erase(it);
                return block;
            }
        }

        return block::allocate(calc_block_size(capacity));
    }

    void block_buffer::push(block_ptr block) {
        blocks_.push_back(block);
    }

    size_t block_buffer::calc_block_size(size_t size) {
        size_t allocate_size = size / min_block_size_;
        if (size % min_block_size_ > 0) allocate_size++;

        if (allocate_size < 1) allocate_size = 1;

        allocate_size *= min_block_size_;

        return allocate_size;
    }

    void block_buffer::set_max_block_size(size_t size) {
        max_block_size_ = size;
    }

}

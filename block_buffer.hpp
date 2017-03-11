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

    enum class skip_type {
        write, read
    };
    enum class debug_type {
        hex, chars
    };

    class block;

    typedef std::shared_ptr<block> block_ptr;

    class block_buffer;

    class block {
    public:
        ~block();

        static block_ptr allocate(size_t);

        size_t capacity() const;

        size_t free() const;

        size_t size() const;

        void *malloc();

        void *data() const;

        void reset();

        size_t skip(skip_type type, size_t length);

        size_t append(const block_ptr &other);

        size_t write(void *src, size_t length, bool skip = true);

        size_t read(void *des, size_t length, bool skip = true);

        void debug(debug_type type = debug_type::hex);

    private:
        block() = delete;

        block(size_t capacity);

        uint8_t *data_ = nullptr;
        size_t pos_ = 0;
        size_t capacity_ = 0;
        size_t head_ = 0;

    };

    class block_buffer {
    public:
        block_buffer(size_t min_block_size = 1024, size_t block_size = 0);

        ~block_buffer();

        block_ptr pop();

        block_ptr pop_free(size_t capacity);

        void push(block_ptr _block);

        void recover(block_ptr block);

        void clear();

        bool empty();

        size_t size();

        block_ptr allocate(size_t capacity = 1);

        void set_max_block_size(size_t size);

        void *malloc(size_t size);

        std::tuple<void *, size_t> malloc();

        block_ptr merge();

        size_t merge(block_buffer &buffer);

        size_t append(block_buffer &buffer);

        size_t skip(skip_type type, size_t length);

        size_t write(void *src, size_t length, bool skip = true);

        size_t write(block_buffer &buffer);

        size_t read(void *des, size_t length, bool skip = true);

        void debug(debug_type type = debug_type::hex);

    private:
        size_t calc_block_size(size_t size);

        std::list<block_ptr> blocks_;
        std::list<block_ptr> free_blocks_;

        const size_t min_block_size_;

        size_t max_block_size_ = 10;
    };

}

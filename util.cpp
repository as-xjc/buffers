#include <zlib.h>
#include "util.hpp"

namespace buffer 
{

namespace util
{

uint32_t crc32(uint32_t crc, block& buffer)
{
	return ::crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), buffer.size());
}

uint32_t crc32(uint32_t crc, block_buffer& buffer)
{
	auto& blocks = buffer.blocks();
	for (auto& block : blocks) {
		crc = crc32(crc, *block);
	}

	return crc;
}

uint32_t adler32(uint32_t adler, block& buffer)
{
	return ::adler32(adler, reinterpret_cast<const Bytef*>(buffer.data()), buffer.size());
}

uint32_t adler32(uint32_t adler, block_buffer& buffer)
{
	auto& blocks = buffer.blocks();
	for (auto& block : blocks) {
		adler = adler32(adler, *block);
	}

	return adler;
}

}

}

#include <Helium/Assert.hpp>
#include <Helium/Memory/LinearAllocator.hpp>

#include <cstdint>

namespace Helium {

template<typename Type>
constexpr Type align(Type value, Type alignment)
{
    helium_assert_debug(alignment && !(alignment & (alignment - 1)));

    return (value + alignment - 1) & ~(alignment - 1);
}

LinearAllocator::~LinearAllocator() {
    for (BlockHeader* block = lastBlock; block;) {
        auto prev = block->prev;
        delete[] block;
        block = prev;
    }
}

void* LinearAllocator::allocate(size_t size, size_t alignment) {
    helium_assert(size <= blockSize);

    if (lastBlock == nullptr)
        lastBlock = allocateBlock(nullptr);

    lastBlock->used = align(lastBlock->used, alignment);

    if (lastBlock->used + size >= blockSize)
        lastBlock = allocateBlock(lastBlock);

    void* return_ = reinterpret_cast<uint8_t*>(lastBlock) + blockHeaderSize + lastBlock->used;
    lastBlock->used += size;
    memoryUsage += size;
    return return_;
}

LinearAllocator::BlockHeader* LinearAllocator::allocateBlock(BlockHeader* previous) {
    BlockHeader* block = reinterpret_cast<BlockHeader*>(new uint8_t[blockHeaderSize + blockSize]);
    block->prev = previous;
    block->used = 0;

    memoryUsageInclOverhead += blockHeaderSize + blockSize;
    return block;
}

}

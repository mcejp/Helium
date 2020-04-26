#ifndef HELIUM_COMPILER_LINEARALLOCATOR_HPP
#define HELIUM_COMPILER_LINEARALLOCATOR_HPP

#include <cstddef>
#include <utility>

#include <Helium/Memory/PoolPtr.hpp>

namespace Helium {

template<size_t alignment, typename Type>
constexpr Type align(Type value) {
    static_assert(alignment && !(alignment & (alignment - 1)), "alignment must be power of 2");

    return (value + alignment - 1) & ~(alignment - 1);
}

class LinearAllocator {
public:
    enum { kDefaultBlockSize = 64 * 1024 };

    LinearAllocator(size_t blockSize) : blockSize(blockSize) {
    }

    ~LinearAllocator();

    void* allocate(size_t size, size_t alignment);

    size_t getMemoryUsage() const { return memoryUsage; }
    size_t getMemoryUsageInclOverhead() const { return memoryUsageInclOverhead; }

    template<typename _Tp, typename... _Args>
    inline auto make_pooled(_Args&&... __args)
    {
        auto memory = allocate(sizeof(_Tp), alignof(_Tp));
        return pool_ptr<_Tp>(new(memory) _Tp(std::forward<_Args>(__args)...));
    }

private:
    struct BlockHeader {
        BlockHeader* prev;
        size_t used;
    };

    enum { blockHeaderSize = align<16>(sizeof(BlockHeader)) };

    BlockHeader* allocateBlock(BlockHeader* previous);

    size_t blockSize;
    size_t memoryUsage = 0;
    size_t memoryUsageInclOverhead = 0;
    BlockHeader* lastBlock = nullptr;
};

}

#endif //HELIUM_COMPILER_LINEARALLOCATOR_HPP

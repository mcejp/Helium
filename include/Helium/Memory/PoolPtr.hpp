#ifndef HELIUM_MEMORY_POOLPTR_HPP
#define HELIUM_MEMORY_POOLPTR_HPP

#include <functional>
#include <utility>

namespace Helium {

    template <class T>
    class pool_ptr {
    public:
        using pointer = T*;

        constexpr pool_ptr() noexcept : the_pointer(nullptr) {}
        constexpr pool_ptr(std::nullptr_t) noexcept : the_pointer(nullptr) {}
        explicit pool_ptr(pointer p) noexcept : the_pointer(p) {}
        pool_ptr(const pool_ptr& other) noexcept : the_pointer(other.get()) {}

        template<class U>
        pool_ptr(const pool_ptr<U>& other) noexcept : the_pointer(other.get()) {}

        ~pool_ptr() { reset(); }

        pool_ptr& operator =(const pool_ptr& other) noexcept { reset(other.get()); return *this; }

        template<class U>
        pool_ptr& operator=(const pool_ptr<U>& other) noexcept { reset(other.get()); return *this; }

        typename std::add_lvalue_reference<T>::type operator *() const { return *the_pointer; }
        pointer operator ->() const noexcept { return the_pointer; }
        explicit operator bool() const noexcept { return the_pointer != nullptr; }

        T* get() const { return the_pointer; }
        T* release() noexcept { auto* p = the_pointer; the_pointer = nullptr; return p; }
        void reset(pointer p = nullptr) { the_pointer = p; }

    protected:
        T* the_pointer;
    };

    template<class T1, class T2>
    bool operator<(const pool_ptr<T1>& x, const pool_ptr<T2>& y) {
        using CT = typename std::common_type<typename pool_ptr<T1>::pointer, typename pool_ptr<T2>::pointer>::type;

        return std::less<CT>()(x.get(), y.get());
    };

}

#endif

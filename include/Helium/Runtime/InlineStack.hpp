#pragma once

#include <Helium/Assert.hpp>
#include <Helium/Runtime/Value.hpp>

#include <cstdlib>

namespace Helium
{
    template <typename Type>
    class InlineStack
    {
        Type* data;
        size_t pos, size;

        public:
            InlineStack() : data( nullptr ), pos( 0 ), size( 4 )
            {
                // FIXME: handle failure
                data = static_cast<Type*>(malloc(size * sizeof(Type)));
            }

            ~InlineStack()
            {
                free( data );
            }

            void clear()
            {
                size = 4;
                pos = 0;

                // FIXME: handle failure
                data = static_cast<Type*>(realloc(data, size * sizeof(Type)));
            }

            Type getBelowTop( size_t index )
            {
                return data[pos - 1 - index];
            }

            unsigned getHeight() const
            {
                return pos;
            }

            bool isEmpty() const
            {
                return pos == 0;
            }

            void push( ValueRef val )
            {
                if ( pos >= size )
                {
                    size = pos * 2 + 1;
                    // FIXME: handle failure
                    data = static_cast<Type*>(realloc(data, size * sizeof(Type)));
                }

                data[pos++] = val.detach();
            }

            ValueRef pop()
            {
                helium_assert_userdata(pos > 0);

                return ValueRef{data[--pos]};
            }

            Type* popPtr()
            {
                helium_assert_userdata(pos > 0);

                return data + ( --pos );
            }

            Type top()
            {
                return data[pos - 1];
            }

            Type& topRef()
            {
                return data[pos - 1];
            }
    };
}

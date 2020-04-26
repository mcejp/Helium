#pragma once

#include <Helium/Config.hpp>
#include <Helium/Runtime/Hash.hpp>

#include <string_view>

namespace Helium
{
    typedef uint32_t    CodeAddr_t;
    typedef int64_t     Int_t;              // integer variables
    typedef double      Real_t;             // real variables
    typedef uint64_t    VarId_t;            // used for lifetime tracking

    enum class ValueType {
        undefined,                  // sentinel value type -- access causes an error
                                    // TODO: consider renaming to *invalid*

        // primitive types
        nul,                        // TODO: rename to *null*, *nil*, or *none*
        boolean,
        integer,
        real,                       // TODO: rename to *float* because that's what it really is

        // plain non-data types
        internal,
        nativeFunction,
        scriptFunction,

        // complex data types
        list,
        object,
        string,
    };

    struct Member;
    struct Value;
    class NativeFunctionContext;
	class VM;

    typedef void (*NativeFunction)(NativeFunctionContext& ctx);

    // A hashed string, used e.g. for object property names
    struct VMString
    {
        const char* text;
        uint32_t length;            // TODO: why limited? on purpose to reduce struct size?
        Hash_t hash;

        // TODO: make constexpr (and all uses too!)
        static VMString fromCString(const char* string);
    };

    struct GC
    {
        VM* vm;
        unsigned short flags;
        unsigned numReferences;
    };

    struct ListInfo : public GC
    {
        uint32_t capacity, length;
        Value* items;
    };

    struct ObjectInfo;

    // A dynamically allocated string (a type of Value)
    struct StringInfo
    {
        // 5 + length bytes
        unsigned numReferences;
        char text[1];
    };

    static const unsigned GC_colour_mask = 0x07, GC_black = 0x00, GC_grey = 0x01, GC_white = 0x02, GC_purple = 0x03, GC_registered = 0x10;
    static const unsigned Member_readOnly = 1;

    // TODO: the methods should be progressively migrated to RuntimeFunctions and make this a simple, opaque structure
    struct Value
    {
        enum class ObjectSetPropertyResult {
            success,
            memoryError,
            propertyReadOnlyError,
        };

#ifdef HELIUM_TRACE_VALUES
        VarId_t varId, refId;
#endif

        ValueType type;
        uint32_t length;    // applies only to strings (because those are immutable)
                            // Variable_funcPtr currently uses this to store the module index!

        union
        {
            Int_t integerValue;
            CodeAddr_t funcPointer;
            CodeAddr_t functionIndex;
            Real_t realValue;
            NativeFunction nativeFunction;
            void* pointer;
            bool booleanValue;

            StringInfo* string;

            GC* gc;
            ListInfo* list;
            ObjectInfo* object;
        };

        static void printStatistics();
        //static void resetStatistics();

        static Value undefined() {
            Value var;
            var.type = ValueType::undefined;
            return var;
        }

        static Value newNul()
        {
            Value var;
            var.type = ValueType::nul;
            var.register_();
            return var;
        }

        static Value newBoolean( bool value )
        {
            Value var;
            var.type = ValueType::boolean;
            var.booleanValue = value;
            var.register_();
            return var;
        }

        /*static Variable newFuncPointer( CodeAddr_t offset )
        {
            Variable var;
            var.type = Variable_funcPtr;
            var.funcPointer = offset;
            registerVariable( var );
            return var;
        }*/

        static Value newInteger( Int_t value )
        {
            Value var;
            var.type = ValueType::integer;
            var.integerValue = value;
            var.register_();
            return var;
        }

        static Value newInternal( void* pointer )
        {
            Value var;
            var.type = ValueType::internal;
            var.pointer = pointer;
            var.register_();
            return var;
        }

        static Value newReal( Real_t value )
        {
            Value var;
            var.type = ValueType::real;
            var.realValue = value;
            var.register_();
            return var;
        }

        static Value newScriptFunction( unsigned int moduleIndex, unsigned int functionIndex )
        {
            Value var;
            var.type = ValueType::scriptFunction;
            var.length = moduleIndex;
            var.funcPointer = functionIndex;
            var.register_();
            return var;
        }

        static Value newNativeFunction(NativeFunction nativeFunction)
        {
            Value var;
            var.type = ValueType::nativeFunction;
            var.nativeFunction = nativeFunction;
            var.register_();
            return var;
        }

        Value reference() const;
        void release();
        Value replicate() const;

        //bool isNul() const { return type == ValueType::nul; }
        bool isUndefined() const { return type == ValueType::undefined; }
        bool isList() const { return type == ValueType::list; }
        bool isObject() const { return type == ValueType::object; }

        void print( unsigned depth = 0 );
        //void printCompact( unsigned depth = 0 );

        /* LISTS */
        // If any of these returns false/Variable_undefined, an allocation error occured and must be handled
        static Value newList( VM* vm, size_t preallocSize );
        bool listAddItem(Value valueRef);
        bool listSetItem(size_t index, Value valueRef);
        void listRemoveItems( unsigned index, unsigned count );
        //void enumItems( void* user, void ( *enumCallback )( Variable var, unsigned index, void* user ) );

        // FUNCPOINTER
        unsigned int getFuncPointerModuleIndex() const { return length; }

        unsigned int getScriptFunctionModuleIndex() const { return length; }

        /* OBJECTS */
        // If any of these returns Variable_undefined, an allocation error occured and must be handled
        static Value newObject( VM* vm );

        [[deprecated]] Value replicateObject() const;        // TODO: needs to propagate errors like objectSetMember

        //void enumMembers( void* user, void ( *enumCallback )( Variable var, unsigned memberId, void* user ) );

        Value objectCloneProperty( const VMString& name );
        ObjectSetPropertyResult objectSetProperty(const VMString& name, Value valueRef, bool readOnly);

        /* STRINGS */
        // If any of these returns Variable_undefined, an allocation error occured and must be handled
        static Value newString( const char* string );
        static Value newStringWithLength( const char* string, size_t length );

        Value appendString( const char* text, long len );

        // Garbage collection
        bool gc_mark();
        void gc_mark_grey();
        void gc_mark_grey_sub();
        void gc_scan();
        void gc_scan_black();
        void gc_scan_black_sub();
        void gc_mark_not_registered();
        unsigned gc_collect_white();

    private:
        Value() = default;

        void register_();
        void unregister();

        bool listGrow( unsigned minLength );
        void listReleaseItems();
        void listDestroy();

        intptr_t objectFindProperty(const VMString& name);
        void objectReleaseMembers();
        void objectDestroy();
    };

    struct ObjectInfo : public GC
    {
        unsigned capacity, numMembers;
        Member* members;

        Value ( *clone )( Value obj );
        void ( *finalize )( Value obj );
    };

    struct Member
    {
        // 32 bytes
        Hash_t hash;
        char* key;
        unsigned flags;
        Value value;
    };

    class ValueRef
    {
    public:
        ValueRef() : value{Value::undefined()} {
        }

        ValueRef(ValueRef&& other) noexcept : value(other.detach()) {
        }

        explicit ValueRef(Value value) : value{value} {
        }

        ~ValueRef() {
            value.release();
        }

        ValueRef& operator =(ValueRef&& other) noexcept {
            value.release();
            value = other.detach();
            return *this;
        }

        Value* operator ->() {
            return &value;
        }

        operator Value() {
            return value;
        }

        Value detach() {
            Value value = this->value;
            this->value.type = ValueType::undefined;
            return value;
        }

        static ValueRef makeBoolean(bool value) {
            return ValueRef{Value::newBoolean(value)};
        }

        static ValueRef makeInteger(Int_t value) {
            return ValueRef{Value::newInteger(value)};
        }

        static ValueRef makeInternal(void* pointer) {
            return ValueRef{Value::newInternal(pointer)};
        }

        static ValueRef makeNativeFunction(NativeFunction nativeFunction) {
            return ValueRef{Value::newNativeFunction(nativeFunction)};
        }

        static ValueRef makeNul() {
            return ValueRef{Value::newNul()};
        }

        static ValueRef makeReal(Real_t value) {
            return ValueRef{Value::newReal(value)};
        }

        static ValueRef makeReference(Value value) {
            return ValueRef{value.reference()};
        }

        static ValueRef makeString(const char* string) {
            return ValueRef{Value::newString(string)};
        }

        static ValueRef makeStringWithLength(const char* string, size_t length) {
            return ValueRef{Value::newStringWithLength(string, length)};
        }

        ValueRef reference() {
            return ValueRef{value.reference()};
        }

        void reset() {
            value.release();
            value.type = ValueType::undefined;
        }

        void reset(Value valueRef) {
            value.release();
            value = valueRef;
        }

        ValueRef(const ValueRef& other) = delete;
        ValueRef& operator =(const ValueRef& other) = delete;

    private:
        Value value;
    };

    std::string_view to_string(ValueType t);
}

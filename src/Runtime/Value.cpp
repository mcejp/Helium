#include <Helium/Assert.hpp>
#include <Helium/Config.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <cinttypes>
#include <cmath>
#include <cstdlib>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

#include <cstring>

namespace Helium
{
    static VarId_t numAllocatedVars = 0, numExistingVars = 0, nextRefId = 0;

    VMString VMString::fromCString(const char* string) {
        auto len = strlen(string);
        helium_assert(len < std::numeric_limits<uint32_t>::max());
        return VMString {string, static_cast<uint32_t>(len), Hash::fromString(string, len)};
    }

    void Value::printStatistics()
    {
        if ( numExistingVars > 0 )
        {
            printf( "***************************************************************\n" );
            printf( "Helium: allocated %d variables, %d of them NEVER RELEASED\n", static_cast<int>( numAllocatedVars ),
                    static_cast<int>( numExistingVars ) );
            printf( "***************************************************************\n" );
        }
    }

    void Value::register_()
    {
#if HELIUM_TRACE_VALUES
        this->varId = numAllocatedVars++;
        this->refId = nextRefId++;
        ValueTrace::trackCreated(*this);
#else
        numAllocatedVars++;
#endif

        numExistingVars++;
    }

    /*void Variable::resetStatistics()
    {
        numAllocatedVars = 0;
        numExistingVars = 0;

#if HELIUM_TRACE_VALUES
        existingVars.clear();
#endif
    }*/

    Value Value::reference() const
    {
#if HELIUM_TRACE_VALUES
        if (type != ValueType::undefined) {
            ValueTrace::trackReferenced(*this, nextRefId);
        }
#endif

        switch ( type )
        {
            case ValueType::undefined:
                return undefined();

            case ValueType::nul:
                return newNul();

        case ValueType::boolean:
            return newBoolean(booleanValue);

            case ValueType::integer:
                return newInteger(integerValue);

            case ValueType::internal:
                return newInternal( pointer );

            case ValueType::real:
                return newReal(realValue);

            case ValueType::list:
            case ValueType::object: {
                gc->numReferences++;
                gc->flags &= ~GC_colour_mask;

#if HELIUM_TRACE_VALUES
                Value ref = *this;
                ref.refId = nextRefId++;
                return ref;
#else
                return *this;
#endif
            }

            case ValueType::string:
                string->numReferences++;
                return *this;

            case ValueType::nativeFunction:
                return newNativeFunction(this->nativeFunction);

            case ValueType::scriptFunction:
                return newScriptFunction( getScriptFunctionModuleIndex(), functionIndex );
        }
    }

    void Value::release()
    {
#if HELIUM_TRACE_VALUES
        if (type != ValueType::undefined) {
            ValueTrace::trackReleased(*this);
        }
#endif

        switch (type) {
        case ValueType::undefined:
            return;

        case ValueType::list:
        case ValueType::object:
            /*
            #ifdef info_variable
                        if ( varId == info_variable )
                            printf( "[%i] release\n", ( int ) varId );
            #endif
            */
            if (--gc->numReferences == 0)
            {
                // If known, mark as black
                // Otherwise destroy

                if (type == ValueType::list)
                    listReleaseItems();
                else
                    objectReleaseMembers();

                if (gc->flags & GC_registered)
                    gc->flags &= ~GC_colour_mask;
                else if (type == ValueType::list)
                    listDestroy();
                else
                    objectDestroy();
            }
            else if ((gc->flags & GC_colour_mask) != GC_purple && object->vm)
            {
                // Do not lose this object
                // Mark as purple and make known

                gc->flags = (gc->flags & ~GC_colour_mask) | GC_purple;

                if (!(gc->flags & GC_registered))
                {
                    gc->vm->addPossibleGarbage(*this);
                    gc->flags |= GC_registered;
                }
            }
            break;

        case ValueType::string:
            if (--string->numReferences == 0) {
                unregister();

                free(string);
            }
            break;

        case ValueType::nul:
        case ValueType::boolean:
        case ValueType::integer:
        case ValueType::internal:
        case ValueType::nativeFunction:
        case ValueType::real:
        case ValueType::scriptFunction:
            unregister();
            break;
        }

        type = ValueType::undefined;
    }

    Value Value::replicate() const
    {
        // TODO: same for list, vector, colour

        if ( type == ValueType::object )
        {
            if ( object->clone )
                return object->clone( *this );
            else
                return replicateObject();
        }
        else
            return reference();
    }

    void Value::print( unsigned depth )
    {
        if ( depth > 6 ) {
            printf("...");
            return;
        }

        switch ( type )
        {
        case ValueType::undefined:
            printf("undefined");
            break;

            case ValueType::boolean:
                printf("%s", booleanValue ? "true" : "false");
                break;

            case ValueType::integer:
                printf( "%" PRId64, static_cast<int64_t>(integerValue) );
                break;

            case ValueType::nul:
                printf( "nul" );
                break;

            case ValueType::real:
                printf( "%g", realValue);
                break;

            case ValueType::internal:
                printf( "[Internal %p]", pointer );
                break;

            case ValueType::list:
                printf( "( " );

                for ( unsigned i = 0; i < list->length; i++ )
                {
                    if ( list->items[i].type == ValueType::object )
                        putchar( '\n' );

                    list->items[i].print( depth + 1 );

                    if ( i + 1 < list->length )
                        putchar( ',' );

                    putchar( ' ' );
                }

                printf( ")" );
                break;

            case ValueType::object:
                for ( unsigned i = 0; i < depth; i++ )
                    printf( "  " );

                printf( "${\n" );

                for ( unsigned i = 0; i < object->numMembers; i++ )
                {
                    for ( unsigned j = 0; j < depth + 1; j++ )
                        printf( "  " );

                    printf( "%s: ", object->members[i].key );

                    if ( object->members[i].value.type == ValueType::object )
                        putchar( '\n' );

                    object->members[i].value.print( depth + 1 );

                    if ( i < object->numMembers - 1 )
                        putchar( ',' );

                    putchar( '\n' );
                }

                for ( unsigned i = 0; i < depth; i++ )
                    printf( "  " );

                printf( "}\n\n" );
                break;

            case ValueType::nativeFunction:
                printf( "[NativeFunction %p]", nativeFunction );
                break;

            case ValueType::scriptFunction:
                printf( "[ScriptFunction @ %d/%d]", getScriptFunctionModuleIndex(), static_cast<unsigned int>(functionIndex) );
                break;

            case ValueType::string:
                printf( "%s", string->text );
                break;
        }
    }

    /*void Variable::printCompact( unsigned depth )
    {
    }*/

    void Value::unregister()
    {
        helium_assert(this->type != ValueType::undefined);

#if HELIUM_TRACE_VALUES
        ValueTrace::trackDestroyed(*this);
#endif

        numExistingVars--;

        this->type = ValueType::undefined;
    }

    /* LISTS ******************************************************************
     **************************************************************************/

    Value Value::newList(VM* vm, size_t preallocSize) {
        // TODO: don't hardcode uint32 here
        if (preallocSize > std::numeric_limits<uint32_t>::max())
            preallocSize = std::numeric_limits<uint32_t>::max();

        ValueRef var;
        var->type = ValueType::list;
        var->list = new ListInfo;
        var->list->capacity = preallocSize > 0 ? static_cast<uint32_t>(preallocSize) : 1;
        var->list->length = 0;
        var->list->items = static_cast<Value*>(calloc(var->list->capacity, sizeof(Value)));

        var->list->vm = vm;
        var->list->flags = 0;
        var->list->numReferences = 1;

        if ( var->list->items == nullptr ) {
            return undefined();
        }

        var->register_();
        return var.detach();
	}

    bool Value::listGrow(unsigned minLength) {
        unsigned oldLength = list->capacity;

        list->capacity = minLength + minLength / 2 + 1;
        auto newItems = static_cast<Value*>(realloc(list->items, list->capacity * sizeof(Value)));

        if ( newItems == nullptr ) {
            return false;
        }

        list->items = newItems;
        memset( list->items + oldLength, 0, ( list->capacity - oldLength ) * sizeof( Value ) );
        return true;
    }

    /*void Variable::enumItems( void* user, void ( *enumCallback )( Variable var, unsigned index, void* user ) )
    {
        for ( unsigned i = 0; i < list->length; i++ )
            enumCallback( *this, i, user );
    }*/

    void Value::listReleaseItems()
    {
        for ( int index = list->length - 1; index >= 0; index-- )
            list->items[index].release();
    }

    void Value::listDestroy()
    {
        for ( int index = list->length - 1; index >= 0; index-- )
            if ( list->items[index].type != ValueType::list && list->items[index].type != ValueType::object )
                list->items[index].release();

        free( list->items );

        unregister();
        delete list;
    }

    bool Value::listAddItem(Value valueRef) {
        return listSetItem( list->length, valueRef );
    }

    bool Value::listSetItem(size_t index, Value valueRef) {
        helium_assert(type == ValueType::list);

        if ( index >= list->capacity ) {
            if (!listGrow(index + 1))
                return false;
        }
        else if ( index < list->length )
            list->items[index].release();

        if ( list->length <= index )
            list->length = index + 1;

        list->items[index] = valueRef;
        return true;
    }

    void Value::listRemoveItems( unsigned index, unsigned count )
    {
        if ( index >= list->length )
            return;

        if ( index + count > list->length )
            count = list->length - index;

        for ( unsigned i = index; i < index + count; i++ )
            list->items[i].release();

        memmove( list->items + index, list->items + index + count, ( list->length - index - count ) * sizeof( *list->items ) );
        list->length -= count;
    }

    /* OBJECTS ****************************************************************
     **************************************************************************/

    Value Value::newObject( VM* vm )
    {
        ValueRef var;
        var->type = ValueType::object;
        var->object = new ObjectInfo;
        ( var->object )->capacity = 4;
        ( var->object )->numMembers = 0;
        ( var->object )->members = static_cast<Member*>(calloc(var->object->capacity, sizeof(Member)));

        ( var->object )->clone = 0;
        ( var->object )->finalize = 0;

        var->object->vm = vm;
        var->object->flags = 0;
        var->object->numReferences = 1;

        if ( var->object->members == nullptr ) {
            return undefined();
        }

        var->register_();
        return var.detach();
    }

    Value Value::replicateObject() const
    {
        Value copy = newObject( object->vm );

        copy.object->clone = object->clone;
        copy.object->finalize = object->finalize;

        for ( unsigned i = 0; i < object->numMembers; i++ ) {
            // TODO: this will be eventually a VMString anyway
            helium_assert_debug(strlen(object->members[i].key) < UINT32_MAX);
            VMString vs {object->members[i].key, static_cast<uint32_t>(strlen(object->members[i].key)), object->members[i].hash};
            bool readOnly = (object->members[i].flags & Member_readOnly) != 0;
            copy.objectSetProperty(vs, object->members[i].value.reference(), readOnly);
        }

        return copy;
    }

    /*void Variable::enumMembers( void* user, void ( *enumCallback )( Variable var, unsigned i, void* user ) )
    {
        for ( unsigned i = 0; i < object->numMembers; i++ )
            enumCallback( *this, i, user );
    }*/

    void Value::objectReleaseMembers()
    {
        if ( object->finalize)
        {
            object->finalize( *this );
            object->finalize = 0;
        }

        for ( unsigned i = 0; i < object->numMembers; i++ )
            object->members[i].value.release();
    }

    void Value::objectDestroy()
    {
        for ( unsigned i = 0; i < object->numMembers; i++ )
            if ( object->members[i].value.type != ValueType::list && object->members[i].value.type != ValueType::object )
                object->members[i].value.release();

        for ( unsigned i = 0; i < object->numMembers; i++ )
            free( object->members[i].key );

        free( object->members );

        unregister();
        delete object;

        object = reinterpret_cast<ObjectInfo*>(0xcccccccc);
    }

    intptr_t Value::objectFindProperty(const VMString& name)
    {
        helium_assert(type == ValueType::object);

        for ( unsigned i = 0; i < object->numMembers; i++ )
            if ( object->members[i].hash == name.hash && strcmp( object->members[i].key, name.text ) == 0 )
                return i;

        return -1;
    }

    Value Value::objectCloneProperty( const VMString& name ) {
        helium_assert(type == ValueType::object);

        auto index = objectFindProperty(name);

        if (index >= 0)
            return object->members[index].value.reference();
        else
            return undefined();
    }

    Value::ObjectSetPropertyResult Value::objectSetProperty( const VMString& name, Value valueRef, bool readOnly )
    {
#ifdef info_variable
        if ( varId == info_variable )
            printf( "[%i] set `%s`\n", ( int ) varId, name );
#endif

        helium_assert(type == ValueType::object);

        auto index = objectFindProperty(name);

        if ( index == -1 )
        {
            // Seems it does not. Let's create it then!
            if ( object->numMembers + 1 > object->capacity )
            {
                // Oops, we need more memory!

                long oldLength = object->capacity;
                object->capacity *= 2;
                object->members = static_cast<Member*>(realloc(object->members, object->capacity * sizeof(Member)));

                if ( !object->members )
                {
                    valueRef.release();
                    return ObjectSetPropertyResult::memoryError;
                }

                memset( object->members + oldLength, 0, ( object->capacity - oldLength ) * sizeof( Member ) );
            }

            index = object->numMembers++;
            object->members[index].key = reinterpret_cast<char*>(malloc( name.length + 1 ));
            memcpy( object->members[index].key, name.text, name.length + 1 );
            object->members[index].hash = name.hash;
            object->members[index].flags = ( readOnly ? Member_readOnly : 0 );
        }
        else
        {
            if ( object->members[index].flags & Member_readOnly )
            {
                valueRef.release();
                return ObjectSetPropertyResult::propertyReadOnlyError;
            }

            object->members[index].value.release();
        }

        object->members[index].value = valueRef;
        return ObjectSetPropertyResult::success;
    }

    /* STRINGS ****************************************************************
     **************************************************************************/

    Value Value::newString(const char* string) {
        return newStringWithLength(string, strlen(string));
    }

    Value Value::newStringWithLength(const char* string, size_t length) {
        // TODO: don't hardcode uint32 here
        helium_assert(length < std::numeric_limits<uint32_t>::max());

        Value var;
        var.type = ValueType::string;
        var.length = length;
        var.string = reinterpret_cast<StringInfo*>( malloc( sizeof( unsigned ) + var.length + 1 ) );

        if ( string != nullptr )
            memcpy( var.string->text, string, var.length );
        else {
#if HELIUM_TRACE_VALUES
            // At least in this case we want to initialize the string buffer, because ValueTrace is going to print it
            // Alternative: indicate to ValueTrace that the string is uninitialized
            memset( var.string->text, 0, var.length );
#endif
        }

        var.string->numReferences = 1;
        var.string->text[var.length] = 0;

        var.register_();
        return var;
    }

    Value Value::appendString( const char* text, long otherLength )
    {
        Value appended = newStringWithLength( nullptr, length + otherLength );
        memcpy( ( appended.string )->text, string->text, length );
        memcpy( ( appended.string )->text + length, text, otherLength );
        ( appended.string )->text[length + otherLength] = 0;
        return appended;
    }

    bool Value::gc_mark()
    {
        if ( ( gc->flags & GC_colour_mask ) == GC_purple )
            gc_mark_grey();
        else
        {
            gc->flags &= ~GC_registered;

            if ( ( gc->flags & GC_colour_mask ) == GC_black && gc->numReferences == 0 )
            {
                if ( type == ValueType::list )
                    listDestroy();
                else
                    objectDestroy();
            }

            return true;
        }

        return false;
    }

    void Value::gc_mark_grey()
    {
        if ( ( gc->flags & GC_colour_mask ) != GC_grey )
        {
            gc->flags = ( gc->flags & ~GC_colour_mask ) | GC_grey;

            if ( type == ValueType::list )
            {
                for ( unsigned i = 0; i < list->length; i++ )
                    if ( list->items[i].type == ValueType::list || list->items[i].type == ValueType::object )
                        list->items[i].gc_mark_grey_sub();
            }
            else
            {
                for ( unsigned i = 0; i < object->numMembers; i++ )
                    if ( object->members[i].value.type == ValueType::list || object->members[i].value.type == ValueType::object )
                        object->members[i].value.gc_mark_grey_sub();
            }
        }
    }

    void Value::gc_mark_grey_sub()
    {
        gc->numReferences--;
        gc_mark_grey();
    }

    void Value::gc_scan()
    {
        if ( ( gc->flags & GC_colour_mask ) == GC_grey )
        {
            if ( gc->numReferences > 0 )
                gc_scan_black();
            else
            {
                gc->flags = ( gc->flags & ~GC_colour_mask ) | GC_white;

                if ( type == ValueType::list )
                {
                    for ( unsigned i = 0; i < list->length; i++ )
                        if ( list->items[i].type == ValueType::list || list->items[i].type == ValueType::object )
                            list->items[i].gc_scan();
                }
                else
                {
                    for ( unsigned i = 0; i < object->numMembers; i++ )
                        if ( object->members[i].value.type == ValueType::list || object->members[i].value.type == ValueType::object )
                            object->members[i].value.gc_scan();
                }
            }
        }
    }

    void Value::gc_scan_black()
    {
        gc->flags &= ~GC_colour_mask;

        if ( type == ValueType::list )
        {
            for ( unsigned i = 0; i < list->length; i++ )
                if ( list->items[i].type == ValueType::list || list->items[i].type == ValueType::object )
                    list->items[i].gc_scan_black_sub();
        }
        else
        {
            for ( unsigned i = 0; i < object->numMembers; i++ )
                if ( object->members[i].value.type == ValueType::list || object->members[i].value.type == ValueType::object )
                    object->members[i].value.gc_scan_black_sub();
        }
    }

    void Value::gc_scan_black_sub()
    {
        gc->numReferences++;

        if ( ( gc->flags & GC_colour_mask ) != GC_black )
            gc_scan_black();
    }

    void Value::gc_mark_not_registered()
    {
        gc->flags &= ~GC_registered;
    }

    unsigned Value::gc_collect_white()
    {
        if ( ( gc->flags & GC_colour_mask ) == GC_white && !( gc->flags & GC_registered ) )
        {
            unsigned count = 1;

            gc->flags &= ~GC_colour_mask;

            if ( type == ValueType::list )
            {
                for ( unsigned i = 0; i < list->length; i++ )
                    if ( list->items[i].type == ValueType::list || list->items[i].type == ValueType::object )
                        count += list->items[i].gc_collect_white();
            }
            else
            {
                if ( object->finalize)
                    object->finalize( *this );

                for ( unsigned i = 0; i < object->numMembers; i++ )
                    if ( object->members[i].value.type == ValueType::list || object->members[i].value.type == ValueType::object )
                        count += object->members[i].value.gc_collect_white();
            }

            if ( type == ValueType::list )
                listDestroy();
            else
                objectDestroy();

            return count;
        }
        else
            return 0;
    }

    std::string_view to_string(ValueType t) {
        switch (t) {
        case ValueType::boolean: return "boolean";
        case ValueType::integer: return "integer";
        case ValueType::internal: return "internal";
        case ValueType::list: return "list";
        case ValueType::nativeFunction: return "nativeFunction";
        case ValueType::nul: return "nul";
        case ValueType::object: return "object";
        case ValueType::real: return "real";
        case ValueType::scriptFunction: return "scriptFunction";
        case ValueType::string: return "string";
        case ValueType::undefined: return "undefined";
        }

        helium_abort_expression(t != t);
    }
}

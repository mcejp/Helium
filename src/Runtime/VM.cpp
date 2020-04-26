#include <Helium/Assert.hpp>
#include <Helium/Config.hpp>
#include <Helium/Runtime/NativeListFunctions.hpp>
#include <Helium/Runtime/RuntimeFunctions.hpp>
#include <Helium/Runtime/NativeStringFunctions.hpp>
#include <Helium/Runtime/VM.hpp>

#include <Helium/Runtime/Debug/GcTrace.hpp>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

#include <cmath>
#include <cstring>

#include <unordered_map>
#include <Helium/Runtime/NativeObjectFunctions.hpp>

namespace Helium
{

namespace {
    static std::unordered_map<std::string, NativeFunction> listMethods;
    static std::unordered_map<std::string, NativeFunction> stringMethods;

    // How many Possible Cycle Roots are needed to trigger a collect cycle
    inline auto GC_NUM_POSSIBLE_ROOTS_THRESHOLD = 1000;
}

    using std::move;

    std::optional<FunctionIndex_t> VMModule::findMainFunction() {
        for (size_t i = 0; i < functions.size(); i++) {
            if (functions[i].name == ScriptFunction::MAIN_FUNCTION_NAME) {
                return i;
            }
        }

        return {};
    }

    VM::VM()
    {
        global.reset(Value::newObject( this ));

        //registerStringFunctions( stringFunctions );

        // TODO: we can do better
        listMethods.emplace("add",          &NativeListFunctions::add);
        listMethods.emplace("remove",       &NativeListFunctions::remove);

        stringMethods.emplace("endsWith",       &NativeStringFunctions::endsWith);
        stringMethods.emplace("startsWith",     &NativeStringFunctions::startsWith);
    }

    VM::~VM()
    {
        ValueTraceCtx tracking_ctx("~VM");

        global.reset();
        loadedModules.clear();

        collectGarbage( GarbageCollectReason::vmShutdown );
    }

    /*void VM::addCode( size_t module, Instruction** code, size_t length )
    {
        if ( instructions == nullptr )
            instructions = Allocator<Instruction*>::allocate( length );
        else
            instructions = Allocator<Instruction*>::resize( instructions, numInstructions + length );

        on_debug_printf( "Helium VM: Module %" PRIuPTR " - adding %" PRIuPTR " instructions...\n", module, length );

        for ( size_t i = 0; i < length; i++ )
            //instructions[numInstructions + i] = new Instruction( code[i] );
            instructions[numInstructions + i] = code[i];

        numInstructions += length;
    }*/

    void VM::collectGarbage(GarbageCollectReason reason)
    {
        ValueTraceCtx tracking_ctx("VM::collectGarbage");

        GcTrace::beginCollectGarbage(reason, numInstructionsSinceLastCollect);

        for (size_t i = possibleRoots.size(); i > 0; )
        {
            --i;
            if (possibleRoots[i].gc_mark() )
                possibleRoots.erase(possibleRoots.begin() + i );
        }

        for ( auto& var : possibleRoots )
            var.gc_scan();

        int numValuesCollected = 0;

        for (size_t i = possibleRoots.size(); i > 0; )
        {
            --i;
            possibleRoots[i].gc_mark_not_registered();
            numValuesCollected += possibleRoots[i].gc_collect_white();
        }

        possibleRoots.clear();

        GcTrace::endCollectGarbage(numValuesCollected);
        numInstructionsSinceLastCollect = 0;
    }
}

#define STRING_OPERAND(next) (ctx.activeModule->strings[next->stringIndex])

namespace Helium
{
/*    ActivationContext::State VM::run( size_t entry, Variable* result_out )
    {
        ActivationContext ctx(this);
        ctx.callFunction(entry, 0);

        this->execute(ctx);

        if (ctx.state == ActivationContext::returnedValue)
            *result_out = ctx.stack.pop();
        else if (ctx.state == ActivationContext::raisedException)
            *result_out = ctx.exception.reference();

        return ctx.state;
    }*/

    void VM::execute( ActivationContext& ctx )
    {
        ActivationScope scope(ctx);

#if HELIUM_TRACE_VALUES
        ValueTraceCtx valueTraceContext("VM::execute", ctx);
#endif

        size_t numArgs = static_cast<size_t>(-1);

        while ( ctx.state == ActivationContext::ready )
        {
            // FIXME: do not reference ctx.module->*, cache locally!
            // This includes STRING_OPERAND
            helium_assert( ctx.pc < ctx.activeModule->instructions.size() );

            if (possibleRoots.size() > GC_NUM_POSSIBLE_ROOTS_THRESHOLD)
                collectGarbage( GarbageCollectReason::numPossibleRoots );

            auto next = &ctx.activeModule->instructions[ctx.pc++];

            switch ( next->opcode )
            {
                case Opcodes::op_add: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorAdd(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::args:
                    numArgs = next->integer;
                    break;

            case Opcodes::assert: {
                auto& expression = STRING_OPERAND(next);

                ValueRef value = ctx.stack.pop();
                bool boolValue;

                if (!RuntimeFunctions::asBoolean(value, &boolValue, true))
                    break;

                if (!boolValue) {
                    auto string = std::string("failed assertion `") + expression.text + "`";
                    RuntimeFunctions::raiseException(string.c_str());
                }
                break;
            }

                case Opcodes::call_func:
                    ctx.callScriptFunction(ctx.activeModuleIndex, next->functionIndex, numArgs, ValueRef());
                    break;

                case Opcodes::call_var:
                {
                    ValueRef callable = ctx.stack.pop();
                    ctx.invoke(callable, numArgs);
                    break;
                }

                // Call External
                case Opcodes::call_ext: {
                    ctx.callNativeFunction(externals[next->integer].callback, numArgs);
                    break;
                }

                case Opcodes::new_obj: {
                    ValueRef object;

                    if (!NativeObjectFunctions::newObject(&object))
                        break;

                    ctx.stack.push(move(object));
                    break;
                }

                case Opcodes::op_div: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorDiv(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::drop:
                    ctx.stack.pop();
                    break;

                case Opcodes::dup: {
                    Value val = ctx.stack.top();
                    ctx.stack.push(ValueRef::makeReference(val));
                    break;
                }

                case Opcodes::dup1: {
                    Value val = ctx.stack.getBelowTop(1);
                    ctx.stack.push(ValueRef::makeReference(val));
                    break;
                }

                case Opcodes::eq: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    if (RuntimeFunctions::operatorEquals(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(result));
                    break;
                }

                case Opcodes::grtr: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    if (RuntimeFunctions::operatorGreaterThan(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(result));
                    break;
                }

                case Opcodes::grtrEq: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    // implemented as not-less-than
                    if (RuntimeFunctions::operatorLessThan(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(!result));
                    break;
                }

                case Opcodes::invoke:
                {
                    ValueRef object = ctx.stack.pop();

                    auto& methodName = STRING_OPERAND(next);

                    switch ( object->type )
                    {
                        case ValueType::list: {
                            auto it = listMethods.find( methodName.text );

                            // FIXME: throw exception
                            helium_assert(it != listMethods.end());

                            ctx.callNativeFunctionWithSelf(it->second, numArgs, object);
                            break;
                        }

                        case ValueType::string: {
                            auto it = stringMethods.find( methodName.text );

                            // FIXME: throw exception
                            helium_assert(it != stringMethods.end());

                            ctx.callNativeFunctionWithSelf(it->second, numArgs, object);
                            break;
                        }

                        default: {
                            ValueRef method;

                            if (!RuntimeFunctions::getProperty(object, methodName, &method, true))
                                break;

                            ctx.invokeWithSelf(method, object, numArgs);
                        }
                    }
                    break;
                }

                case Opcodes::jmp:
                    ctx.pc = next->codeAddr;
                    break;

                case Opcodes::jmp_true: {
                    ValueRef value = ctx.stack.pop();
                    bool boolValue;

                    if (!RuntimeFunctions::asBoolean(value, &boolValue, true))
                        break;

                    if (boolValue)
                        ctx.pc = next->codeAddr;

                    break;
                }

                case Opcodes::jmp_false: {
                    ValueRef value = ctx.stack.pop();
                    bool boolValue;

                    if (!RuntimeFunctions::asBoolean(value, &boolValue, true))
                        break;

                    if (!boolValue)
                        ctx.pc = next->codeAddr;

                    break;
                }

                case Opcodes::land: {
                    ValueRef left = ctx.stack.pop();
                    ValueRef right = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorLogAnd(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::less: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    if (RuntimeFunctions::operatorLessThan(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(result));
                    break;
                }

                case Opcodes::lessEq: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    // implemented as not-greater-than
                    if (RuntimeFunctions::operatorGreaterThan(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(!result));
                    break;
                }

                case Opcodes::new_list:
                {
                    // TODO: integer needs to be verfified
                    size_t count = next->integer;

                    ValueRef list;

                    if (!NativeListFunctions::newList(count, &list))
                        break;

                    for ( long i = count - 1; i >= 0; i-- ) {
                        if (!NativeListFunctions::setItem(list, i, ctx.stack.pop()))
                            // FIXME: needs double-break
                            break;
                    }

                    ctx.stack.push( move(list) );
                    break;
                }

                case Opcodes::lnot: {
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorLogNot(left);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::lor: {
                    ValueRef left = ctx.stack.pop();
                    ValueRef right = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorLogOr(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                //logical( Opcodes::lxor, xor );

                case Opcodes::op_mod: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorMod(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::op_mul: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorMul(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::neg: {
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorNeg(left);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::neq: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();
                    bool result;

                    if (RuntimeFunctions::operatorEquals(left, right, &result))
                        ctx.stack.push(ValueRef::makeBoolean(!result));
                    break;
                }

                case Opcodes::nop:
                    break;

                case Opcodes::setIndexed:
				{
					ValueRef list, index;

                    index = ctx.stack.pop();
                    list = ctx.stack.pop();

                    RuntimeFunctions::setIndexed(list, index, ctx.stack.pop());
                    break;
				}

                // Pop into Local
                case Opcodes::setLocal:
                    ctx.frame->setLocal( next->integer, ctx.stack.pop() );
                    break;

                case Opcodes::setMember:
                {
                    ValueRef object = ctx.stack.pop();
                    auto memberName = STRING_OPERAND(next);

                    RuntimeFunctions::setMember(object, memberName, ctx.stack.pop());
                    break;
                }

                case Opcodes::pushc_b:
                    if (next->integer == 1)
                        ctx.stack.push(ValueRef::makeBoolean(true));
                    else if (next->integer == 0)
                        ctx.stack.push(ValueRef::makeBoolean(false));
                    else
                        helium_assert(next->integer == 0 || next->integer == 1);
                    break;

                case Opcodes::pushc_f:
                    ctx.stack.push( ValueRef::makeReal( next->realValue ) );
                    break;

                case Opcodes::pushc_func:
                    ctx.stack.push( ValueRef{Value::newScriptFunction( ctx.activeModuleIndex, next->functionIndex )} );
                    break;

                case Opcodes::pushc_i: {
                    ctx.stack.push(ValueRef::makeInteger(next->integer));
                    break;
                }

                case Opcodes::pushc_s: {
                    auto str = STRING_OPERAND(next);
                    // TODO: preload or cache this
                    ctx.stack.push(ValueRef::makeStringWithLength(str.text, str.length));
                    break;
                }

                case Opcodes::getIndexed:
                {
                    ValueRef index = ctx.stack.pop();
                    ValueRef range = ctx.stack.pop();

                    ValueRef item;

                    if (RuntimeFunctions::getIndexed(range, index, &item))
                        ctx.stack.push(move(item));
                    break;
                }

                // Push the Global Object
                case Opcodes::pushglobal:
                    ctx.stack.push(global.reference());
                    break;

                case Opcodes::getLocal:
                    ctx.stack.push( ValueRef::makeReference(ctx.frame->getLocal( next->integer )) );
                    break;

                case Opcodes::getProperty:
                {
                    ValueRef object = ctx.stack.pop();

                    ValueRef member;

                    if (!RuntimeFunctions::getProperty(object, STRING_OPERAND(next), &member, true))
                        break;

                    ctx.stack.push( move(member) );
                    break;
                }

                case Opcodes::pushnil:
                    ctx.stack.push(ValueRef::makeNil() );
                    break;

                case Opcodes::ret: {
                    ctx.frames.pop_back();

                    if ( ctx.frames.empty() ) {
                        ctx.state = ActivationContext::returnedValue;
                        break;
                    }

                    ctx.frame = &ctx.frames.back();
                    ctx.activeModule = ctx.frame->module;
                    ctx.activeModuleIndex = ctx.frame->moduleIndex;
                    ctx.pc = ctx.frame->pc;
                    break;
                }

                case Opcodes::op_sub: {
                    ValueRef right = ctx.stack.pop();
                    ValueRef left = ctx.stack.pop();

                    ValueRef result = RuntimeFunctions::operatorSub(left, right);

                    if (!result->isUndefined())
                        ctx.stack.push( move(result) );
                    break;
                }

                case Opcodes::op_switch:
                {
                    ValueRef value = ctx.stack.pop();
                    size_t i = 0;

                    auto switchTable = ctx.activeModule->switchTables[next->switchTableIndex];

                    //* Iterate throught the cases, break if we have a match
                    //* If no corresponding handler is foud, we automatically fall back to the 'else' handler
                    for ( ; i < switchTable->cases.size(); i++ ) {
                        bool equals;
                        helium_assert(RuntimeFunctions::operatorEquals(value, switchTable->cases[i], &equals));

                        if (equals)
                            break;
                    }

                    ctx.pc = switchTable->handlers[i];
                    break;
                }

                case Opcodes::throw_var: {
                    // FIXME: unchecked stack underflow
                    ctx.raiseException(ctx.stack.pop());
                    break;
                }

#ifdef HELIUM_MIXED_MODE
                case Opcodes::add_i:
                    // FIXME: detect integer overflow
                    ctx.frame->setRegisterInt(next->reg.r0, ctx.frame->getRegisterInt(next->reg.r1) + ctx.frame->getRegisterInt(next->reg.r2));
                    break;

                case Opcodes::mov_i:
                    ctx.frame->setRegisterInt(next->reg.r0, ctx.frame->getRegisterInt(next->reg.r1));
                    break;

                case Opcodes::pop_i: {
                    ValueRef value = ctx.stack.pop();
                    Int_t intValue;

                    if (RuntimeFunctions::asInteger(value, &intValue, true))
                        ctx.frame->setRegisterInt(next->reg.r0, intValue);
                    break;
                }

                case Opcodes::push_i:
                    ctx.stack.push(ValueRef::makeInteger(ctx.frame->getRegisterInt(next->reg.r0)));
                    break;
#endif

                default:
                    helium_assert(next->opcode != next->opcode);
            }

            numInstructionsSinceLastCollect++;

            if (ctx.state == ActivationContext::raisedException) {
                // Pop frames until we find a handler

                bool found = false;
                bool frameSwitch = false;

                while (!ctx.frames.empty()) {
                    // Scan the next frame
                    auto& frame = ctx.frames.back();

                    // Is a handler active in this frame?
                    // TODO: optimized lookup

                    for (const auto& eh : frame.scriptFunction->exceptionHandlers) {
                        auto pc = ctx.pc - 1;       // TODO: ok?

                        if (pc >= eh.start && pc < eh.start + eh.length) {
                            ctx.pc = eh.handler;
                            found = true;
                            break;
                        }
                    }

                    if (found)
                        break;

                    // No active handler in this frame - pop it and continue the search
                    ctx.frames.pop_back();
                    frameSwitch = true;
                }

                if (found) {
                    if (frameSwitch) {
                        ctx.frame = &ctx.frames.back();
                        ctx.activeModule = ctx.frame->module;
                        ctx.activeModuleIndex = ctx.frame->moduleIndex;
                    }

                    while ( ctx.stack.getHeight() > ctx.frame->stackBase )
                        ctx.stack.pop();

                    ctx.stack.push( move(ctx.exception) );
                    ctx.resume();
                }
            }
        }
    }

    /*void VM::invoke( Variable me, unsigned target )
    {
        if ( !frameBottom.isEmpty() )
            frameBottom.push( frameBottom.top() + frameTop );
        else
            frameBottom.push( 0 );

        frameTop = 1;
        setLocal( 0, me );

        unsigned tmp = callStack.pop();
        callStack.push( pc );
        callStack.push( tmp );

        pc = target;
    }

    Variable VM::invoke( Variable callable, List<Variable>& arguments, bool blindCall, bool keepArguments )
    {
        if ( callable.type != ValueType::funcPointer && callable.type != Variable_methocator )
        {
            callable.release();

            if ( !keepArguments )
            {
                iterate2 ( i, arguments )
                    (*i).release();

                arguments.clear();
            }

            if ( blindCall )
                return Variable();
            else
                throw Variable::newException( "ObjectNotCallable", "Object passed to Helium::VM::invoke() is not invokable." );
        }

        callStack.push( pc );
        callStack.push( -1 ); // to make VM::execute() return to native instead of the code running in the VM before this invocation
        callStack.push( arguments.getLength() );

        if ( keepArguments )
        {
            reverse_iterate2 ( i, arguments )
                ctx.stack.push( (*i).reference() );
        }
        else
        {
            reverse_iterate2 ( i, arguments )
                ctx.stack.push( (*i) );
            arguments.clear();
        }

        if ( !frameBottom.isEmpty() )
            frameBottom.push( frameBottom.top() + frameTop );
        else
            frameBottom.push( 0 );

        frameTop = 0;

        if ( callable.type == Variable_methocator )
        {
            setLocal( 0, callable.metho->object.reference() );
            pc = callable.metho->method.funcPointer;
        }
        else
            pc = callable.funcPointer;

        isRunning = true;

        execute();

        if ( !stack.isEmpty() )
            return ctx.stack.pop();
        else
            return Variable();
    }*/

    /*long VM::lateAddCode( Instruction** code, unsigned length )
    {
        unsigned oldLength = numInstructions;

        on_debug_printf( "Growing code to %i...\n", oldLength + length );
        instructions = ( Instruction** )realloc( instructions,
                ( oldLength + length ) * sizeof( Instruction* ) );

        for ( unsigned i = 0; i < length; i++ )
            instructions[oldLength + i] = code[i];

        numInstructions += length;
        return oldLength;
    }*/

    ModuleIndex_t VM::loadModule(Module* script )
    {
        std::vector<size_t> externalIndices;
        externalIndices.resize( script->dependencies.size() );

        // Check and satisfy any external dependencies.
        for ( size_t i = 0; i < script->dependencies.size(); i++ )
        {
            const auto& name = script->dependencies[i];

            size_t index;

            for ( index = 0; index < externals.size(); index++ )
                if ( externals[index].name == name )
                    break;

            if ( index >= externals.size() )
                // FIXME: how to handle this?
                throw ( "Failed to link external " + name ).c_str();

            externalIndices[i] = index;
        }

        auto module = std::make_unique<VMModule>();

        size_t stringPoolSize = script->stringPool.size() * sizeof(VMString);

        for (size_t i = 0; i < script->stringPool.size(); i++) {
            const auto& s = script->stringPool[i];

            module->stringMemory.resize(stringPoolSize + s.size() + 1);

            auto strings = reinterpret_cast<VMString*>(&module->stringMemory[0]);
            strings[i].text = reinterpret_cast<const char*>(stringPoolSize);
            strings[i].length = s.size();
            strings[i].hash = Hash::fromString(reinterpret_cast<const char*>(s.data()), s.size());

            memcpy(&module->stringMemory[stringPoolSize], s.data(), s.size());
            module->stringMemory[stringPoolSize + s.size()] = 0;
            stringPoolSize += s.size() + 1;
        }

        module->strings = script->stringPool.size() != 0 ? reinterpret_cast<VMString*>(&module->stringMemory[0]) : nullptr;

        for (size_t i = 0; i < script->stringPool.size(); i++) {
            module->strings[i].text = reinterpret_cast<const char*>(&module->stringMemory[0]) + reinterpret_cast<size_t>(module->strings[i].text);
        }

        // Use this once Instruction is POD
        //module->instructions.assign(script->code.begin(), script->code.end());

        module->instructions.resize(script->code.size());

        for ( size_t i = 0; i < module->instructions.size(); i++ )
        {
            auto current = &module->instructions[i];
            new(current) Instruction(*script->code[i]);

            auto op = current->opcode;

            if (op == Opcodes::call_ext)
                current->integer = externalIndices[current->integer];
            // TODO: set pointer to VMString
            //else if (InstructionDesc::getByOpcode(op)->operandType == OperandType::string)
            //    current->string

            // FIXME: verify stringIndex
            // FIXME: verify switchTableIndex
        }

        module->functions = script->functions;
        module->switchTables = script->switchTables;

        loadedModules.emplace_back(std::move(module));
        return loadedModules.size() - 1;
    }

    int16_t VM::registerCallback( const char* name, NativeFunction callback )
    {
        helium_assert( externals.size() < EXTERNALS_MAX );

        externals.emplace_back(ExternalFunc{ name, callback });
        return static_cast<int16_t>( externals.size() - 1 );
    }

    std::string_view to_string(GarbageCollectReason gcReason) {
        switch (gcReason) {
            case GarbageCollectReason::numInstructionsSinceLastCollect:
                return "numInstructionsSinceLastCollect";
            case GarbageCollectReason::numPossibleRoots:
                return "numPossibleRoots";
            case GarbageCollectReason::vmShutdown:
                return "vmShutdown";
        }

        helium_abort_expression(gcReason != gcReason);
    }
}

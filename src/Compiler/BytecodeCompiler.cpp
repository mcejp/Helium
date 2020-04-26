#include <Helium/Config.hpp>
#include <Helium/Compiler/Ast.hpp>
#include <Helium/Compiler/BytecodeCompiler.hpp>
#include <Helium/Compiler/Type.hpp>
#include <Helium/Memory/LinearAllocator.hpp>

#include <cstring>
#include <unordered_map>
#include <vector>

#if HELIUM_TRACE_VALUES
#include <Helium/Runtime/Debug/ValueTrace.hpp>
#endif

#include <stx/optional.hpp>

namespace Helium
{
    using std::string;
    using stx::optional;

    struct ValueHandle {
        Type* maybeType;

        int maybeLocalIndex;

#ifdef HELIUM_MIXED_MODE
        uint16_t reg;
#endif

        static ValueHandle stackTop() { return ValueHandle{ nullptr, -1 }; }

#ifdef HELIUM_MIXED_MODE
        static ValueHandle inReg(Type* type, uint16_t reg) { return ValueHandle{ type, -1, reg }; }
#endif
    };

    static bool isStackTop(ValueHandle vh) { return vh.maybeType == nullptr && vh.maybeLocalIndex == -1; }
    static bool isInt(ValueHandle vh) { return vh.maybeType && vh.maybeType->isInt(); }

    static ValueRef getConstant(const AstNodeLiteral* literal)
    {
        switch ( literal->literalType )
        {
            case AstNodeLiteral::Type::boolean: {
                auto boolean = static_cast<const AstNodeLiteralBoolean*>(literal);
                return ValueRef::makeBoolean(boolean->value);
            }

            case AstNodeLiteral::Type::integer: {
                auto integer = static_cast<const AstNodeLiteralInteger*>(literal);
                return ValueRef::makeInteger(integer->value);
            }

            case AstNodeLiteral::Type::nil:
                return ValueRef::makeNil();

            case AstNodeLiteral::Type::object:
                helium_assert(literal->literalType != AstNodeLiteral::Type::object);
                break;

            case AstNodeLiteral::Type::real: {
                auto real = static_cast<const AstNodeLiteralReal*>(literal);
                return ValueRef::makeReal(real->value);
            }

            case AstNodeLiteral::Type::string: {
                auto string = static_cast<const AstNodeLiteralString*>(literal);
                return ValueRef::makeStringWithLength(string->text.c_str(), string->text.size());
            }
        }
    }

    struct Function
    {
        struct Local {
            const char* name;
            Type* maybeType;

#ifdef HELIUM_MIXED_MODE
            int16_t reg = -1;
#endif

            Local(const char* name, Type* maybeType) : name(name), maybeType(maybeType) {}
        };

        std::string name;
        optional<FunctionIndex_t> scriptFunctionIndex;  // Don't expect this to be set before ::compile finishes

        const AstNodeFunction* function;
        const AstNodeClass* ownerClass;

		int offset;
        bool toBeExported;              // TODO: deprecate? it was at one point, but what's the alternative
                                        // do we want to re-do intermodule altogether?

        std::vector<std::string> arguments;
        std::vector<Local> locals;

        pool_ptr<AstNodeFunction> functionOwning;

#ifdef HELIUM_MIXED_MODE
        uint16_t registersInUse = 0;
#endif

        Function();

        LocalIndex_t createLocal(const char* name, Type* maybeType);
        LocalIndex_t getOrAllocLocalIndex(const char* name);
        optional<LocalIndex_t> tryGetLocalIndex(const char* name);

        void addArgument( const char* name );
        bool isArgument( const char* name );

#ifdef HELIUM_MIXED_MODE
        uint16_t allocRegister() { return registersInUse++; }
        uint16_t getRegisterForLocal(LocalIndex_t index);
#endif
    };

    struct AssemblerState
    {
        std::unique_ptr<Module> script;

        std::vector<Function*> functions;
        std::vector<Function*> unknownPushFuncPtrs;     // TODO: what does this serve
        std::vector<std::string> temporaryStrings;
        Function* currentFunction;
        ScriptFunction* currentScriptFunction = nullptr;
        bool generateDebug;

        std::shared_ptr<std::string> currentFunctionName, unitNameString;

        std::unordered_map<std::string, size_t> stringPoolIndices;

        bool failed = false;

        AssemblerState( bool withDebugInformation, std::shared_ptr<std::string> unitNameString )
            : generateDebug( withDebugInformation ), unitNameString( unitNameString )
        {
            script = std::make_unique<Module>();
        }

        ~AssemblerState()
        {
            functions.clear();
        }

        Instruction* add( Opcodes::Opcode opcode, const SourceSpan& span ) {
            Instruction* added = new Instruction;
            added->opcode = opcode;

            if ( generateDebug )
            {
                added->origin = new InstructionOrigin;
                added->origin->unit = unitNameString;
                added->origin->function = currentFunctionName;
                added->origin->line = span.start.line;
            }

            script->code.push_back( added );
            return added;
        }

        Function* addFunctionForProcessing(const AstNodeFunction* function) {
            Function* func = new Function;
            func->name = function->getName();       // TODO: if we really need a unique name for every function,
                                                    //       it should be done here
            func->offset = -1;
            func->function = function;
            func->toBeExported = false;
            func->ownerClass = 0;
            functions.push_back( func );
            return func;
        }

        Instruction* emit(Opcodes::Opcode opcode, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::none);

            return add(opcode, span);
        }

        Instruction* emitInteger(Opcodes::Opcode opcode, int64_t integerValue, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::integer);

            Instruction* instr = add(opcode, span);
            instr->integer = integerValue;
            return instr;
        }

        Instruction* emitLocal(Opcodes::Opcode opcode, LocalIndex_t local, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::localIndex);

            Instruction* instr = add(opcode, span);
            instr->integer = local;
            return instr;
        }

        Instruction* emitReal(Opcodes::Opcode opcode, Real_t realValue, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::real);

            Instruction* instr = add(opcode, span);
            instr->realValue = realValue;
            return instr;
        }

#ifdef HELIUM_MIXED_MODE
        void emitReg1(Opcodes::Opcode opcode, int r0, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::reg1);

            auto instr = add(opcode, span);
            instr->reg.r0 = r0;
        }

        void emitReg2(Opcodes::Opcode opcode, int r0, int r1, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::reg2);

            auto instr = add(opcode, span);
            instr->reg.r0 = r0;
            instr->reg.r1 = r1;
        }

        void emitReg3(Opcodes::Opcode opcode, int r0, int r1, int r2, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::reg3);

            auto instr = add(opcode, span);
            instr->reg.r0 = r0;
            instr->reg.r1 = r1;
            instr->reg.r2 = r2;
        }
#endif

        Instruction* emitString(Opcodes::Opcode opcode, const char* string, const SourceSpan& span)
        {
            helium_assert_debug(InstructionDesc::getByOpcode(opcode)->operandType == OperandType::string);

            Instruction* instr = add(opcode, span);
            instr->stringIndex = getStringIndex(string);
            return instr;
        }

        CodeAddr_t currentOffset()
        {
            auto pos = script->code.size();

            helium_assert(pos < std::numeric_limits<CodeAddr_t>::max());

            return static_cast<CodeAddr_t>(pos);
        }

        size_t getStringIndex( const char* str )
        {
            std::string s(str);

            auto it = stringPoolIndices.find(s);

            if (it != stringPoolIndices.end())
                return it->second;

            size_t index = script->stringPool.size();
            script->stringPool.emplace_back(s.begin(), s.end());
            stringPoolIndices.emplace(std::move(s), index);
            return index;
        }

        size_t getSwitchTableIndex(std::shared_ptr<SwitchTable>&& switchTable)
        {
            script->switchTables.emplace_back(std::move(switchTable));
            return script->switchTables.size() - 1;
        }

        size_t getTemporaryStringIndex( const char* str )
        {
            std::string s(str);

            auto it = std::find(temporaryStrings.begin(), temporaryStrings.end(), s);

            if (it != temporaryStrings.end())
                return it - temporaryStrings.begin();

            temporaryStrings.emplace_back(std::move(s));
            return temporaryStrings.size() - 1;
        }

        static bool isGlobal( const AstNodeExpression* node )
        {
            return node->type == AstNodeExpression::Type::identifier && static_cast<const AstNodeIdent*>( node )->name == "_global";
        }

        bool isMember( const char* name )
        {
            const AstNodeClass* currentClass = currentFunction ? currentFunction->ownerClass : 0;

            if ( !currentClass )
                return false;

            for (auto& member : currentClass->memberVariables) {
                if (member.name == name)
                    return true;
            }

            return false;
        }

        bool isMethod( const char* name )
        {
            auto currentClass = currentFunction ? currentFunction->ownerClass : 0;

            if ( !currentClass )
                return false;

            for (auto& method : currentClass->methods) {
                if (method->getName() == name)
                    return true;
            }

            return false;
        }

        size_t getExternal( const char* name )
        {
            //* All functions beginning with '~' are implicitly prefixed with "Helium::"
            //* ('~.IO::serialize' => 'Helium::IO::Serialize')

            // FIXME: Not UTF-8 safe, but will be probably removed anyways
            auto fullName = name[0] != '~' ? std::string( name ) : "Helium::" + std::string( name + 1 );

            for ( size_t i = 0; i < script->dependencies.size(); i++ )
            {
                if ( script->dependencies[i] == fullName )
                    return i;
            }

            script->dependencies.push_back( fullName );
            return script->dependencies.size() - 1;
        }

        Function* findFunction( const char* name )
        {
            for ( auto function : functions )
                if ( function->name == name )
                    return function;

            return 0;
        }

        void binaryOperator( Opcodes::Opcode opcode, const AstNodeBinaryExpr* expr );
        void compileClass(AstNodeClass* classDecl);
        void cook();
        void linearize( AstNodeScript& tree );
        Type* maybeGetType(AstNodeTypeName* maybeTypeName);
        void unaryOperator( Opcodes::Opcode opcode, const AstNodeUnaryExpr* expr );

        // New-style stuff

        /**
         * Throw away the value referred to by \p vh. This removes the value from stack and/or calls releaseExpression
         * @param vh
         * @param span
         * @return true on success, false on error
         */
        bool dropValue(ValueHandle vh, const SourceSpan& span);

        /**
         * Evaluate an Expression node and put the result in its natural location
         * (for general case, falls back to evaluateExpressionBasic and evaluating on stack)
         * @param node
         * @return ValueHandle on success, empty on error
         */
        optional<ValueHandle> evaluateExpression(const AstNodeExpression* node);

        /**
         * Evaluate an Expression using stack operations
         * @param node
         * @return true on success, false on error
         */
        bool evaluateExpressionBasic(const AstNodeExpression* node);

        /**
         * Store the stack top into an Expression node
         * @param node
         * @param keepOnStack if false, the value will be popped off the stack
         * @return true on success, false on error
         */
        bool popExpression(const AstNodeExpression* node, bool keepOnStack);

        /**
         * Pop a stack value into a local variable
         * @param index
         * @param span
         */
        void popLocal(LocalIndex_t index, const SourceSpan& span);

        /**
         * Evaluate an Expression and make sure the final result is on top of the stack
         * @param node
         * @return true on success, false on error
         */
        bool pushExpression(const AstNodeExpression* node);

        /**
         * Push a literal value to the stack
         * @param node
         * @return true on success, false on error
         */
        bool pushLiteral(const AstNodeLiteral* node);

        /**
         * Push the value referred to by \p vh to the stack
         * @param vh
         * @param span
         * @return true on success, false on error
         */
        bool pushValue(ValueHandle vh, const SourceSpan& span);

        /**
         * Release the memory location occupied by \p vh
         * @param vh
         * @return true on success, false on error
         */
        bool releaseExpression(ValueHandle vh) { return true; }

        /**
         * Find out if the specified expression is a local variable and if it is, return the relevant information
         * @param expression
         * @param index_out
         * @param maybeType_out
         * @return
         */
        bool tryResolveLocalVariable(const AstNodeExpression* expression, LocalIndex_t* index_out, Type** maybeType_out);

        bool compileStatement(const AstNodeStatement* node);

        // Errors
        void raiseError(const char* message, const SourceSpan& span);
        void operandTypeError(const AstNodeBinaryExpr* operation, Type* maybeType1, Type* maybeType2);
        void typeError1() { abort(); }

    private:
        // TODO: temporary, will be removed once we stop messing with the AST
        LinearAllocator allocatorTmp{LinearAllocator::kDefaultBlockSize};
    };

    Function::Function() {
        locals.emplace_back(Local{ "this", nullptr });
    }

    void Function::addArgument(const char* name)
    {
        arguments.push_back(name);
    }

    LocalIndex_t Function::createLocal(const char* name, Type* maybeType)
    {
        locals.emplace_back(Local{ name, maybeType });

        helium_assert(locals.size() < LOCALS_MAX);

        return static_cast<LocalIndex_t>(locals.size()) - 1;
    }

    LocalIndex_t Function::getOrAllocLocalIndex(const char* name) {
        auto local = tryGetLocalIndex(name);

        if (local) {
            return *local;
        }
        else {
            locals.emplace_back(Local{name, nullptr});

            return locals.size() - 1;
        }
    }

    optional<LocalIndex_t> Function::tryGetLocalIndex(const char* name)
    {
        for (size_t i = 0; i < locals.size(); i++)
            if (strcmp(locals[i].name, name) == 0)
                return i;

        return {};
    }

#ifdef HELIUM_MIXED_MODE
    uint16_t Function::getRegisterForLocal(LocalIndex_t index) {
        auto& local = locals[index];

        // FIXME: check type

        if (local.reg < 0) {
            // FIXME: check required size
            local.reg = allocRegister();
        }

        return local.reg;
    }
#endif

    bool Function::isArgument( const char* name )
    {
        return std::find( arguments.begin(), arguments.end(), name ) != arguments.end();
    }

    void AssemblerState::binaryOperator( Opcodes::Opcode opcode, const AstNodeBinaryExpr* expr )
    {
        pushExpression(expr->getLeft());
        pushExpression(expr->getRight());
        emit(opcode, expr->span);
    }

    bool AssemblerState::compileStatement(const AstNodeStatement* node) {
        switch (node->type) {
            case AstNodeStatement::Type::assert: {
                auto assert = static_cast<const AstNodeAssert*>(node);

                pushExpression(assert->getExpression());

                emitString( Opcodes::assert, assert->getExpressionString().c_str(), node->span );
                break;
            }

            case AstNodeStatement::Type::assignment: {
                auto assignment = static_cast<const AstNodeAssignment*>(node);
                auto target = assignment->getTarget();
                auto expr = assignment->getExpression();
                auto location = assignment->span;

                //* Only local variables, list items and variable members can be stored to!
                if ( target->type != AstNodeExpression::Type::identifier
                     && target->type != AstNodeExpression::Type::indexed
                     && target->type != AstNodeExpression::Type::property ) {
                    raiseError("Expected local variable, list item or variable member at left side of assignment", assignment->span);
                    return false;
                }

#ifdef HELIUM_MIXED_MODE
                LocalIndex_t index;
                Type* maybeType;

                if (tryResolveLocalVariable(target, &index, &maybeType)) {
                    optional<ValueHandle> vh = evaluateExpression(expr);

                    // Evaluate the expression to assign
                    if (!vh)
                        return false;

                    if (maybeType == vh->maybeType && isInt(maybeType)) {
                        auto srcRegister = vh->reg;      // FIXME: can we assume this?
                        auto destRegister = currentFunction->getRegisterForLocal(index);

                        emitReg2(Opcodes::mov_i, destRegister, srcRegister, expr->span);

                        releaseExpression(*vh);
                        return true;
                    }
                    else {
                        // Generic assignment -- go through the stack

                        if (!pushValue(*vh, location))
                            return false;

                        popLocal(index, location);
                        releaseExpression(*vh);
                        return true;
                    }
                }
#endif

                //* Build the expression and store it.
                pushExpression(assignment->getExpression());
                popExpression(target, false);
                break;
            }

            case AstNodeStatement::Type::block: {
                auto block = static_cast<const AstNodeBlock*>(node);

                for (const auto& statement : block->getStatements())
                    compileStatement(statement.get());

                break;
            }

            case AstNodeStatement::Type::expression: {
                auto expression = static_cast<const AstNodeStatementExpression*>(node);
                auto expr = expression->getExpression();

                pushExpression(expr);
                emit(Opcodes::drop, expression->span);
                break;
            }

            case AstNodeStatement::Type::for_: {
                auto for_ = static_cast<const AstNodeForClassic*>(node);

                Instruction* jumpToEnd;
                /* The FOR statement, when compiled, consists of:
                    -[1] init-statement
                    -[2] loop expression
                    -[3] JZ to [7] (executed if !Expr)
                    -[4] The FOR-branch
                    -[5] update-statement
                    -[6] JMP to [2]
                    -[7] the end
                */

                compileStatement(for_->getInitStatement());                   //* init

                //* [2]
                unsigned begin = currentOffset();
                pushExpression(for_->getExpression());

                jumpToEnd = add( Opcodes::jmp_false, node->span );       //* jz end

                compileStatement(for_->getBody());
                compileStatement(for_->getUpdateStatement());

                add( Opcodes::jmp, node->span )->codeAddr = begin; //* jmp begin
                jumpToEnd->codeAddr = currentOffset();
                break;
            }

            case AstNodeStatement::Type::forRange: {
                auto forRange = static_cast<const AstNodeForRange*>(node);

                auto iteratorVar = currentFunction->createLocal("(iterator)", nullptr);
                auto itemVar = currentFunction->getOrAllocLocalIndex(forRange->getVariableName().c_str() );

                // [1] Initialize
                // Store zero in the iterator
                emitInteger(Opcodes::pushc_i, 0, node->span);
                emitLocal(Opcodes::setLocal, iteratorVar, node->span);

                // [2] Compare the iterator and the final value
                unsigned begin = currentOffset();

                // if !(iterator < range.length)
                emitLocal(Opcodes::getLocal, iteratorVar, node->span);
                pushExpression(forRange->getRange());
                emitString(Opcodes::getProperty, "length", node->span);
                emit(Opcodes::less, node->span);

                //   break
                Instruction* jumpToEnd = add( Opcodes::jmp_false, node->span );

                // else
                //   item = list[iterator]
                pushExpression(forRange->getRange());
                emitLocal(Opcodes::getLocal, iteratorVar, node->span);
                emit(Opcodes::getIndexed, node->span);
                emitLocal(Opcodes::setLocal, itemVar, node->span);

                compileStatement(forRange->getBlock());

                // [3] Post-cycle statement

                // iterator += 1
                emitLocal(Opcodes::getLocal, iteratorVar, node->span);
                emitInteger(Opcodes::pushc_i, 1, node->span);
                emit(Opcodes::op_add, node->span);
                emitLocal(Opcodes::setLocal, iteratorVar, node->span);

                // [4] Jump to [2]
                add( Opcodes::jmp, node->span )->codeAddr = begin;

                // [5] Statement end.
                jumpToEnd->codeAddr = currentOffset();
                break;
            }

            case AstNodeStatement::Type::if_: {
                auto if_ = static_cast<const AstNodeIf*>(node);
                auto expr = if_->getExpression();
                auto elseBlock = if_->getElseBlock();

                Instruction* jumpToElse, * jumpToEnd = 0;
                /* The IF statement, when compiled, consists of:
                    -[1] The Expression
                    -[2] JZ to [5] (executed if !Expr)
                    -[3] The IF-branch
                    -[4] JMP to [6], if [5] exists
                    -[5] Optional ELSE-branch
                    -[6] the end
                */

                if ( expr->type == AstNodeExpression::Type::unaryExpr
                     && static_cast<const AstNodeUnaryExpr*>(expr)->type == AstNodeUnaryExpr::Type::not_ )
                {
                    auto notExpr = static_cast<const AstNodeUnaryExpr*>(expr);
                    pushExpression(notExpr->right.get());
                    jumpToElse = add( Opcodes::jmp_true, node->span );
                }
                else
                {
                    pushExpression(expr);
                    jumpToElse = add( Opcodes::jmp_false, node->span );
                }

                // the body
                compileStatement(if_->getBlock());

                //* [4] (if it has got an ELSE)
                if ( elseBlock )
                    jumpToEnd = add( Opcodes::jmp, node->span );

                //* [5]
                unsigned ifBlockEnd = currentOffset();

                if ( elseBlock )
                    compileStatement(elseBlock);

                //* [6]
                unsigned elseBlockEnd = currentOffset();

                jumpToElse->codeAddr = ifBlockEnd;
                if ( elseBlock )
                    jumpToEnd->codeAddr = elseBlockEnd;
                break;
            }

            case AstNodeStatement::Type::infer:
                break;

            case AstNodeStatement::Type::return_: {
                auto& return_ = *static_cast<const AstNodeReturn*>(node);

                pushExpression(return_.getExpression());
                emit( Opcodes::ret, node->span);
                break;
            }

            case AstNodeStatement::Type::switch_: {
                auto switch_ = static_cast<const AstNodeSwitch*>(node);

                //* Compile the expression
                pushExpression(switch_->getExpression());

                //* First of all: caluclate the total number of cases (expanding multiple cases)
                size_t numCases = 0;

                for (const auto& case_ : switch_->getCases()) {
                    auto valueList = case_.first.get();

                    numCases += valueList->getItems().size();
                }

                // Built the switch jump table
                auto switchTable = std::make_shared<SwitchTable>();
                switchTable->cases.resize(numCases);
                switchTable->handlers.resize(numCases + 1);

                // We only need to add this instruction before compiling the cases
                auto switchInstruction = add( Opcodes::op_switch, switch_->span );

                //* Temporary table of jumps to the end
                std::vector<Instruction*> endJumps;

                //* Compile the cases
                size_t currentEntry = 0;

                for (const auto& case_ : switch_->getCases()) {
                    auto valueList = case_.first.get();

                    for (const auto& caseValue : valueList->getItems()) {
                        // FIXME: nobody guarantees this!
                        helium_assert(caseValue->type == AstNodeExpression::Type::literal);

                        switchTable->cases[currentEntry] = getConstant(static_cast<const AstNodeLiteral*>(caseValue.get()));
                        switchTable->handlers[currentEntry++] = currentOffset();
                    }

                    // Compile the block
                    compileStatement(case_.second.get());

                    endJumps.push_back( add( Opcodes::jmp, case_.first->span ) );
                }

                helium_assert_debug(currentEntry == numCases);

                //* The default handler pointer (if not present, this will point to the statement end)
                switchTable->handlers[numCases] = currentOffset();

                switchInstruction->switchTableIndex = getSwitchTableIndex(std::move(switchTable));

                //* Default handler code. Optional.
                if ( switch_->hasDefaultHandler() )
                    compileStatement( switch_->getDefaultHandler() );

                //* Finally, fill the jumps to the end
                for (size_t i = 0; i < switch_->getCases().size(); i++) {
                    endJumps[i]->codeAddr = currentOffset();
                }
                break;
            }

            case AstNodeStatement::Type::throw_: {
                auto throw_ = static_cast<const AstNodeThrow*>(node);

                pushExpression(throw_->getExpression());
                add(Opcodes::throw_var, node->span);
                break;
            }

            case AstNodeStatement::Type::tryCatch: {
                auto try_catch = static_cast<const AstNodeTryCatch*>(node);

                Instruction * exit;

                /* 'try-catch' code structure:
                    try X
                    block (b->left)
                    exit end
                    X:
                    store exception to b->children[0]
                    handler (b->right)
                    end:
                */

                /* 'try-drop' code structure:
                    try X:
                    block (b->left)
                    exit end
                    X:
                    drop
                    end:
                */

                auto start = currentOffset();

                // try body
                compileStatement(try_catch->getTryBlock());

                auto length = currentOffset() - start;

                exit = add( Opcodes::jmp, try_catch->span );                          //* exit end
                auto handler = currentOffset();

                // store the exception
                add(Opcodes::setLocal, try_catch->span)->integer = currentFunction->getOrAllocLocalIndex(try_catch->getCaughtVariableName().c_str() );

                compileStatement(try_catch->getCatchBlock());

                exit->codeAddr = currentOffset();

                // Nested try-catch blocks will get pushed before this, which is exactly what we need
                currentScriptFunction->exceptionHandlers.push_back(Eh {start, length, handler});
                break;
            }

            case AstNodeStatement::Type::while_: {
                auto while_ = static_cast<const AstNodeWhile*>(node);

                Instruction* jumpToEnd;
                /* The IF statement, when compiled, consists of:
                    -[1] The Expression
                    -[2] JZ to [5] (executed if !Expr)
                    -[3] The WHILE-branch
                    -[4] JMP to [1]
                    -[5] the end
                */

                unsigned begin = currentOffset();

                // Evaluate the expression
                pushExpression(while_->getExpression());
                jumpToEnd = add( Opcodes::jmp_false, node->span );

                // Loop body
                compileStatement(while_->getBlock());

                // Jump to start
                add( Opcodes::jmp, node->span )->codeAddr = begin;
                jumpToEnd->codeAddr = currentOffset();
                break;
            }
        }

        return true;
    }

    bool AssemblerState::dropValue(ValueHandle vh, const SourceSpan& span) {
        if (vh.maybeType == nullptr) {
            // value is dynamic, and it's necessarily already on the stack
            // FIXME: this shouldn't be an equivalency

            emit(Opcodes::drop, span);
            return true;
        }

        releaseExpression(vh);
        return true;
    }

    optional<ValueHandle> AssemblerState::evaluateExpression(const AstNodeExpression* node) {
        switch (node->type) {
#ifdef HELIUM_MIXED_MODE
        case AstNodeExpression::Type::binaryExpr: {
            auto binaryExpr = static_cast<const AstNodeBinaryExpr*>(node);

            // Only 'add' is currently supported -- bail out for others
            if (binaryExpr->binaryExprType != AstNodeBinaryExpr::Type::add)
                break;

            optional<ValueHandle> left = evaluateExpression(binaryExpr->getLeft());

            if (!left)
                return {};

            // left is dynamic
            if (isStackTop(*left)) {
                pushExpression(binaryExpr->getRight());
                emit(Opcodes::op_add, binaryExpr->span);
                return ValueHandle::stackTop();
            }

            // left is Int
            if (isInt(*left)) {
                optional<ValueHandle> right = evaluateExpression(binaryExpr->getRight());

                if (!right)
                    return {};

                if (isInt(*right)) {
                    auto vh = ValueHandle::inReg(Type::builtinInt(), currentFunction->allocRegister());
                    emitReg3(Opcodes::add_i, vh.reg, left->reg, right->reg, node->span);
                    releaseExpression(*left);
                    releaseExpression(*right);
                    return vh;
                }
                else {
                    operandTypeError(binaryExpr, left->maybeType, right->maybeType);
                    return {};
                }
            }

            helium_assert(false);

            //if (left->maybeType != right.maybeType)
            //    typeError1();

            return {};
        }

        case AstNodeExpression::Type::literal: {
            auto literal = static_cast<const AstNodeLiteral*>(node);

            // Only integer literals are currently supported -- bail out for others
            if (literal->literalType != AstNodeLiteral::Type::integer)
                break;

            auto integer = static_cast<const AstNodeLiteralInteger*>(literal);
            emitInteger(Opcodes::pushc_i, integer->value, node->span);

            //vh_out = ValueHandle::inReg(Type::builtinInt(), currentFunction->allocRegister());
            //emitReg1(Opcodes::pop_i, vh_out.reg, node->line);
            return ValueHandle::stackTop();
        }

        case AstNodeExpression::Type::identifier: {
            auto identifier = static_cast<const AstNodeIdent*>(node);

            if (isGlobal(identifier)) {
                break;
            }

            LocalIndex_t index;
            Type* maybeType;

            if (tryResolveLocalVariable(identifier, &index, &maybeType)) {
                auto location = node->span;     // FIXME

                if (maybeType != nullptr) {
                    auto type = maybeType;

                    if (isInt(type)) {
                        return ValueHandle::inReg(type, currentFunction->getRegisterForLocal(index));
                    }
                }
            }

            break;
        }
#endif

        default:
            ;
        }

        evaluateExpressionBasic(node);
        return ValueHandle::stackTop();
    }

    void AssemblerState::operandTypeError(const AstNodeBinaryExpr* operation, Type* maybeType1, Type* maybeType2) {
        auto message = string("Invalid operands to '") + string(operation->getSymbol()) + "' - " + Type::toString(maybeType1) + ", " + Type::toString(maybeType2);

        raiseError(message.c_str(), operation->span);
    }

    bool AssemblerState::popExpression(const AstNodeExpression* node, bool keepOnStack) {
        switch (node->type) {
            case AstNodeExpression::Type::identifier: {
                auto identifier = static_cast<const AstNodeIdent*>(node);

                helium_assert(!isGlobal(identifier));

                if (keepOnStack) {
                    emit(Opcodes::dup, node->span);
                }

                // FIXME: DRY
                bool forceLocal = (identifier->ns == AstNodeIdent::Namespace::local) || currentFunction->isArgument( identifier->name.c_str() );

                if ( !forceLocal && isMember( identifier->name.c_str() ) ) {
                    emitLocal(Opcodes::getLocal, LOCAL_THIS, node->span);
                    emitString(Opcodes::setMember, identifier->name.c_str(), node->span);
                }
                else {
                    emitLocal(Opcodes::setLocal, currentFunction->getOrAllocLocalIndex(identifier->name.c_str()), node->span);
                }

                break;
            }

            // Indexed store
            case AstNodeExpression::Type::indexed: {
                auto indexed = static_cast<const AstNodeExprIndexed*>(node);
                auto range = indexed->range.get();
                auto index = indexed->index.get();

                if (keepOnStack) {
                    emit(Opcodes::dup, node->span);
                }

                // Stack parameters are: the list (lower) & index
                pushExpression(range);
                pushExpression(index);

                emit(Opcodes::setIndexed, node->span);
                break;
            }

            case AstNodeExpression::Type::property: {
                auto property = static_cast<const AstNodeProperty*>(node);
                auto object = property->object.get();
                auto& propertyName = property->propertyName->name;

                if (keepOnStack) {
                    emit(Opcodes::dup, node->span);
                }

                if ( object ) {
                    pushExpression(object);
                }
                else {
                    emitLocal(Opcodes::getLocal, LOCAL_THIS, node->span);
                }

                emitString(Opcodes::setMember, propertyName.c_str(), node->span );
                break;
            }

            default:
                helium_assert(node->type != node->type);
        }

        return true;
    }

    void AssemblerState::popLocal(LocalIndex_t index, const SourceSpan& span) {
        helium_assert_debug(currentFunction != nullptr);

        auto& local = currentFunction->locals[index];

        if (local.maybeType == nullptr) {
            emitLocal(Opcodes::setLocal, index, span);
        }
#ifdef HELIUM_MIXED_MODE
        else if (isInt(local.maybeType)) {
            emitReg1(Opcodes::pop_i, currentFunction->getRegisterForLocal(index), span);
        }
#endif
        else
            helium_assert(false);
    }

    // Compute excepression and push it to the top of the stack
    bool AssemblerState::evaluateExpressionBasic(const AstNodeExpression* node) {
        switch (node->type) {
            case AstNodeExpression::Type::binaryExpr: {
                auto binaryExpr = static_cast<const AstNodeBinaryExpr*>(node);

                switch (binaryExpr->binaryExprType) {
                case AstNodeBinaryExpr::Type::add: binaryOperator(Opcodes::op_add, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::divide: binaryOperator(Opcodes::op_div, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::modulo: binaryOperator(Opcodes::op_mod, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::multiply: binaryOperator(Opcodes::op_mul, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::subtract: binaryOperator(Opcodes::op_sub, binaryExpr); return true;

                case AstNodeBinaryExpr::Type::and_: binaryOperator(Opcodes::land, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::or_: binaryOperator(Opcodes::lor, binaryExpr); return true;

                case AstNodeBinaryExpr::Type::greater: binaryOperator(Opcodes::grtr, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::greaterEq: binaryOperator(Opcodes::grtrEq, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::less: binaryOperator(Opcodes::less, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::lessEq: binaryOperator(Opcodes::lessEq, binaryExpr); return true;

                case AstNodeBinaryExpr::Type::equals: binaryOperator(Opcodes::eq, binaryExpr); return true;
                case AstNodeBinaryExpr::Type::notEquals: binaryOperator(Opcodes::neq, binaryExpr); return true;
                }

                break;
            }

            case AstNodeExpression::Type::call:
            {
                auto call = static_cast<const AstNodeCall*>(node);
                bool isTopLevel = false;
                auto callable = call->getCallable();
                auto argList = call->getArguments();

                bool blind = false;
                Instruction* skip = 0;

                if ( call->isBlindCall() && !isTopLevel ) {
                    raiseError("Blind-call is not a statement", call->span);
                    return false;
                }

                if ( call->isBlindCall() )
                    blind = true;

                if ( blind )
                {
                    // Blind-calls: test the function first and skip the whole call if not valid

                    pushExpression(callable);
                    skip = add( Opcodes::jmp_false, callable->span );
                }

                //* Branch->right shall be the parameter list
                for (auto iter = argList->getItems().rbegin(); iter != argList->getItems().rend(); iter++) {
                    pushExpression(iter->get());
                }

                //* Push the parameter count
                // TODO: why so early?
                add( Opcodes::args, node->span )->integer = argList->getItems().size();

                if ( callable->type == AstNodeExpression::Type::property )
                    //* A direct member invocation
                {
                    auto& property = *static_cast<const AstNodeProperty*>(callable);
                    const auto& methodName = property.propertyName->name;

                    pushExpression(property.object.get());
                    emitString(Opcodes::invoke, methodName.c_str(), property.span);
                }
                else if ( callable->type == AstNodeExpression::Type::identifier
                          && isMethod( static_cast<const AstNodeIdent*>(callable)->name.c_str() ) )
                {
                    // Calling own method

                    const auto& className = currentFunction->ownerClass->name;
                    const auto& methodName = static_cast<const AstNodeIdent*>(callable)->name;

                    if ( className != methodName )
                        add( Opcodes::unknownCall, node->span )->stringIndex = getTemporaryStringIndex((className + "|" + methodName).c_str());
                    else
                        add( Opcodes::unknownCall, node->span )->stringIndex = getTemporaryStringIndex(className.c_str());
                }
                else if ( callable->type == AstNodeExpression::Type::identifier
                          && !currentFunction->tryGetLocalIndex( static_cast<const AstNodeIdent*>(callable)->name.c_str() )
                          && static_cast<const AstNodeIdent*>(callable)->ns != AstNodeIdent::Namespace::local
                          && !isMember( static_cast<const AstNodeIdent*>(callable)->name.c_str() ) )
                    //* A plain by-symbol call (but we can't be always sure whether it's a subroutine or an external)
                {
                    const auto& funcName = static_cast<const AstNodeIdent*>(callable)->name;
                    //on_debug_printf( "~$ subr/callex %s\n", funcName.c_str() );
                    add( Opcodes::unknownCall, node->span )->stringIndex = getTemporaryStringIndex(funcName.c_str());
                }
                else
                    // A General function call
                {
                    // Get the function pointer on the stack
                    pushExpression(callable);

                    emit(Opcodes::call_var, node->span);
                }

                if ( isTopLevel )
                    add( Opcodes::drop, call->span );

                if ( blind )
                    skip->codeAddr = currentOffset();
                break;
            }

            case AstNodeExpression::Type::function: {
                auto function = static_cast<const AstNodeFunction*>(node);

                auto func = addFunctionForProcessing(function);

                add( Opcodes::unknownPush, function->span )->stringIndex = getTemporaryStringIndex( func->name.c_str() );
                unknownPushFuncPtrs.push_back( currentFunction );
                break;
            }

            case AstNodeExpression::Type::identifier: {
                auto identifier = static_cast<const AstNodeIdent*>(node);

                if (isGlobal(identifier)) {
                    emit(Opcodes::pushglobal, node->span);
                    break;
                }

                // FIXME: DRY
                bool forceLocal = (identifier->ns == AstNodeIdent::Namespace::local) || currentFunction->isArgument( identifier->name.c_str() );

                if ( !forceLocal && isMember( identifier->name.c_str() ) )
                {
                    // Get value of class member
                    emitLocal(Opcodes::getLocal, LOCAL_THIS, node->span);
                    emitString(Opcodes::getProperty, identifier->name.c_str(), node->span);
                }
                else
                {
                    add( Opcodes::unknownPush, node->span )->stringIndex = getTemporaryStringIndex(identifier->name.c_str());
                    unknownPushFuncPtrs.push_back( currentFunction );
                }

                break;
            }

            // Indexed load
            case AstNodeExpression::Type::indexed: {
                auto indexed = static_cast<const AstNodeExprIndexed*>(node);
                auto range = indexed->range.get();
                auto index = indexed->index.get();

                //* Stack parameters are: the list (lower) & index
                pushExpression(range);
                pushExpression(index);

                // No instruction parameters
                add(Opcodes::getIndexed, node->span );
                break;
            }

            case AstNodeExpression::Type::list: {
                auto list = static_cast<const AstNodeList*>(node);

                if ( list->getItems().size() == 1 )
                    //* One-item list is considered a plain expression
                    pushExpression( list->getItems()[0].get() );
                else
                    //* But this is a multi-item one.
                {
                    //* Make them one-by-one
                    for (const auto& expression : list->getItems())
                        pushExpression( expression.get() );

                    //* And create a list out of them (if the expression is used anywhere)
                    add( Opcodes::new_list, node->span )->integer = list->getItems().size();
                }
                break;
            }

            case AstNodeExpression::Type::literal:
                return pushLiteral(static_cast<const AstNodeLiteral*>(node));

            case AstNodeExpression::Type::property: {
                auto property = static_cast<const AstNodeProperty*>(node);
                auto object = property->object.get();
                auto& propertyName = property->propertyName->name;

                pushExpression(object);
                emitString(Opcodes::getProperty, propertyName.c_str(), node->span);
                break;
            }

            case AstNodeExpression::Type::unaryExpr: {
                auto unaryExpr = static_cast<const AstNodeUnaryExpr*>(node);

                switch (unaryExpr->type) {
                case AstNodeUnaryExpr::Type::negation: unaryOperator(Opcodes::neg, unaryExpr); return true;
                case AstNodeUnaryExpr::Type::not_: unaryOperator(Opcodes::lnot, unaryExpr); return true;
                }

                break;
            }
        }

        return true;
    }

    bool AssemblerState::pushExpression(const AstNodeExpression* node) {
        auto vh = evaluateExpression(node);

        if (!vh || !pushValue(*vh, node->span) || !releaseExpression(*vh))
            return false;

        return true;
    }

    // Compute excepression and push it to the top of the stack
    bool AssemblerState::pushLiteral(const AstNodeLiteral* node) {
        switch (node->literalType) {
            case AstNodeLiteral::Type::boolean:
                emitInteger(Opcodes::pushc_b, static_cast<const AstNodeLiteralBoolean*>(node)->value, node->span);
                return true;

            case AstNodeLiteral::Type::integer:
                emitInteger(Opcodes::pushc_i, static_cast<const AstNodeLiteralInteger*>(node)->value, node->span);
                return true;

            case AstNodeLiteral::Type::nil:
                emit(Opcodes::pushnil, node->span);
                return true;

            case AstNodeLiteral::Type::object: {
                auto objectLiteral = static_cast<const AstNodeLiteralObject*>(node);
                emit(Opcodes::new_obj, node->span);

                for (auto& pair : objectLiteral->getProperties()) {
                    pushExpression(pair.second.get());

                    emit(Opcodes::dup1, pair.second->span);
                    emitString( Opcodes::setMember, pair.first.c_str(), pair.second->span );
                }
                return true;
            }

            case AstNodeLiteral::Type::real: {
                auto real = static_cast<const AstNodeLiteralReal*>(node);
                emitReal(Opcodes::pushc_f, real->value, node->span);
                return true;
            }

            case AstNodeLiteral::Type::string: {
                auto string = static_cast<const AstNodeLiteralString*>(node);
                emitString(Opcodes::pushc_s, string->text.c_str(), node->span);
                return true;
            }
        }
    }

    bool AssemblerState::pushValue(ValueHandle vh, const SourceSpan& span) {
        // value is already on the stack
        if (isStackTop(vh))
            return true;

#ifdef HELIUM_MIXED_MODE
        if (isInt(vh.maybeType)) {
            emitReg1(Opcodes::push_i, vh.reg, span);
            return true;
        }
#endif

        helium_assert(false);
    }

    void AssemblerState::raiseError(const char* message, const SourceSpan& span) {
        printf("%s:%i: error: %s\n", this->unitNameString->c_str(), span.start.line, message);

        failed = true;
    }

    bool AssemblerState::tryResolveLocalVariable(const AstNodeExpression* expression, LocalIndex_t* index_out, Type** maybeType_out) {
        helium_assert_debug(currentFunction != nullptr);

        if (expression->type == AstNodeExpression::Type::identifier) {
            auto identifier = static_cast<const AstNodeIdent*>(expression);

            // FIXME: DRY
            bool forceLocal = (identifier->ns == AstNodeIdent::Namespace::local) ||
                              currentFunction->isArgument(identifier->name.c_str());

            if (forceLocal) {
                auto index = currentFunction->getOrAllocLocalIndex(identifier->name.c_str());
                *index_out = index;
                *maybeType_out = currentFunction->locals[index].maybeType;
                return true;
            }
        }

        return false;
    }

    void AssemblerState::unaryOperator( Opcodes::Opcode opcode, const AstNodeUnaryExpr* expr )
    {
        pushExpression(expr->right.get());
        emit(opcode, expr->span);
    }

    void AssemblerState::linearize( AstNodeScript& tree )
    {
        // Create an entry for .main function
        Function* main = new Function;
        main->name = ScriptFunction::MAIN_FUNCTION_NAME;
        main->offset = 0;
        main->function = tree.mainFunction.get();
        main->toBeExported = false;
        main->ownerClass = 0;
        functions.push_back( main );

        for (auto& class_ : tree.classes) {
            compileClass(class_.get());
        }

        // TODO: Is this good enough?
        // Handling of anonymous & named functions should be probably unified
        for (auto& function : tree.functions) {
            addFunctionForProcessing(function.get());
        }

        //* Iterate through the function list (which is incrementally built during the compilation!)
        for ( size_t i = 0; i < functions.size(); i++ )
        {
            //* Register this function as being compiled
            currentFunction = functions[i];
            currentFunction->offset = currentOffset();
            currentFunctionName = std::make_shared<std::string>( currentFunction->name );
            //on_debug_printf( "%s:\t\t-- @ %i\n", currentFunctionName->c_str(), currentFunction->offset );

            // Add function header + parameter loader

            auto arguments = currentFunction->function->getArguments();
            helium_assert_debug(arguments);

            auto startInstr = currentOffset();

            //if ( parameters->type == AstNodeType::list )
            //{
                // Ordinary fixed-argument function

                size_t numArguments = arguments->decls.size();

                ScriptFunction scriptFunction {
                        currentFunction->name,
                        currentFunction->toBeExported,
                        ScriptFunction::ArgumentListType::explicit_,
                        numArguments,
                        startInstr,
                        0,
                        {},
                };

                this->currentScriptFunction = &scriptFunction;

                for (auto& decl : arguments->decls) {
                    auto argName = decl.name.c_str();

                    currentFunction->addArgument(argName);
                    auto index = currentFunction->createLocal(argName, maybeGetType(decl.type.get()));

#ifdef HELIUM_MIXED_MODE
                    // If needed, convert arg to native type
                    if (currentFunction->locals[index].maybeType != nullptr) {
                        auto line = currentFunction->function->span;
                        emitLocal(Opcodes::getLocal, index, line);
                        popLocal(index, line);
                    }
#else
                    // TODO: argument type checking
#endif
                }
            /*}
			else if ( parameters->type == AstNodeType::symbol )
            {
                // Variadic
                auto argName = parameters->textValue;

                add( Opcodes::func_var, parameters->line )->mode = currentFunction->getLocal( parameters->textValue );
                currentFunction->addArgument( argName );
            }*/

            // Now compile the function body
            compileStatement( currentFunction->function->getBody() );

            // Implicit "return this" at the end of class methods
            emitLocal(Opcodes::getLocal, LOCAL_THIS, currentFunction->function->span);
            emit(Opcodes::ret, currentFunction->function->span);

            scriptFunction.length = script->code.size() - startInstr;
            this->currentScriptFunction = nullptr;
            script->functions.emplace_back(std::move(scriptFunction));
            currentFunction->scriptFunctionIndex = script->functions.size() - 1;

            currentFunctionName.reset();
        }
    }

    static bool isBuiltinInt(AstNodeTypeName* typeName) {
        return typeName->name == "Int";
    }

    Type* AssemblerState::maybeGetType(AstNodeTypeName* maybeTypeName) {
        if (!maybeTypeName)
            return nullptr;

        if (isBuiltinInt(maybeTypeName))
            return Type::builtinInt();

        typeError1();
        return nullptr;
    }

    void AssemblerState::compileClass(AstNodeClass* classDecl) {
        // Iterate class functions and try to find the constructor
        AstNodeFunction* constructor = nullptr;
        std::vector<AstNodeFunction*> nonConstructorMethods;
        nonConstructorMethods.reserve(classDecl->methods.size());

        for (auto& method : classDecl->methods)
        {
            if (method->getName() == "constructor") {
                if (constructor)
                    helium_assert(false);   // FIXME: error

                constructor = method.get();
            }
            else
                nonConstructorMethods.push_back(method.get());
        }

        // If the constructor was not found, we need to create one
        pool_ptr<AstNodeFunction> constructorOwning;

        if (!constructor) {
            // TODO: check if span makes sense
            constructorOwning = allocatorTmp.make_pooled<AstNodeFunction>(std::string(classDecl->name), classDecl->span,
                                                                          allocatorTmp.make_pooled<AstNodeDeclList>(classDecl->span),
                                                                          nullptr);

            constructor = constructorOwning.get();
        }

        // Constructor

        // Function definition
        Function* func = new Function;
        func->name = classDecl->name;
        func->offset = -1;
        func->function = constructor;
        func->functionOwning = std::move(constructorOwning);

        // We'll need to do some operations with the original function body
        // FIXME: shouldn't have to do this. AST must be immutable!
        auto originalBody = const_cast<AstNodeFunction*>(func->function)->moveBody();
        const_cast<AstNodeFunction*>(func->function)->setBody(allocatorTmp.make_pooled<AstNodeBlock>(func->function->span));

        // First of all, we need to add a command to lock the declared class members (if any)
        // They make no sense anyway as long as the object is not created/initialized.
        // See the example:

        /*
        class ExtendingClass
        member alpha, beta;

        ExtendingClass( alpha, beta ) : BaseClass( alpha )
        beta = local beta;
        */

        // Without this tweak, BaseClass would be initialized with "this.alpha"
        // which makes no sense and thus is not desirable.

        /*Branch* lockMembers = new Branch(AstNodeType::lockClassMembers);
        lockMembers->booleanValue = true;
        func->function->body->addChild(lockMembers);*/

        // Build the object construction
        // This creates the object and fills it with function pointers to the class's methods
        // TODO: this is obviously all wrong and needs to be done in concrete types
        // TODO: check if this span makes sense (below as well)
        auto construction = allocatorTmp.make_pooled<AstNodeLiteralObject>(func->function->span);

        for (auto& classMethod : nonConstructorMethods) {
            // "method->left" specifies the method (member) name
            // while "method->right" specifies the global function name
            // See "case AstNodeType::construction:" for more information on how this works

            auto expression = allocatorTmp.make_pooled<AstNodeIdent>(classDecl->name + "|" + classMethod->getName(), AstNodeIdent::Namespace::unknown, func->function->span);
            construction->addProperty(std::string(classMethod->getName()), std::move(expression));
        }

        // Also add pre-initialized member variables
        for (auto& member : classDecl->memberVariables) {
            if (member.initialValue) {
                construction->addProperty(std::string(member.name), std::move(member.initialValue));
            }
        }

        pool_ptr<AstNodeExpression> selfInitValue;
        pool_ptr<AstNodeExpression> base = constructor->moveParentConstructor();

        if (base)
        {
            // Extending something (usually a base class but it can be actually anything summable with an object)
            selfInitValue = allocatorTmp.make_pooled<AstNodeBinaryExpr>(AstNodeBinaryExpr::Type::add,
                                                                        std::move(base), std::move(construction),
                                                                        func->function->span);
        }
        else
            selfInitValue = std::move(construction);

        // this = ...
        auto selfAssignment = allocatorTmp.make_pooled<AstNodeAssignment>(allocatorTmp.make_pooled<AstNodeIdent>("this", AstNodeIdent::Namespace::local, func->function->span),
                                                                          std::move(selfInitValue),
                                                                          func->function->span);

        // this = our_new_object
        const_cast<AstNodeBlock*>(func->function->getBody())->emplace_back(std::move(selfAssignment));

        // "this" is defined now.
        // Time to unlock declared class members.
        /*Branch* unlockMembers = new Branch(AstNodeType::lockClassMembers);
        unlockMembers->booleanValue = false;
        func->function->body->addChild(unlockMembers);*/

        // Finally add the constuctor body defined in the script (will be null if the constructor was manufactured here)
        if (originalBody)
            const_cast<AstNodeBlock*>(func->function->getBody())->emplace_back(std::move(originalBody));

        // If a class is to be private, this must be declared at the constructor
        // TODO: Makes no sense, move it to class, will be fairly trivial.
        func->toBeExported = !constructor->isPrivate;

        func->ownerClass = classDecl;
        functions.push_back(func);

        // Compile other methods
        for (auto& method : nonConstructorMethods)
        {
            Function* func = new Function;
            func->name = classDecl->name + "|" + method->getName();
            func->offset = -1;
            func->function = method;
            func->toBeExported = !method->isPrivate;
            func->ownerClass = classDecl;
            functions.push_back(func);
        }
    }

    void AssemblerState::cook()
    {
        for (Instruction* current: script->code) {
            switch ( current->opcode )
            {
                case Opcodes::unknownCall:
                {
                    const auto& functionName = temporaryStrings[current->stringIndex];
                    Function* function = findFunction( functionName.c_str() );

                    if ( !function )
                    // Not found, compiling as external...
                    {
                        current->opcode = Opcodes::call_ext;
                        current->integer = getExternal( functionName.c_str() );
                    }
                    else
                    // Found it, use a fixed call
                    {
                        current->opcode = Opcodes::call_func;
                        helium_assert(function->scriptFunctionIndex);
                        current->functionIndex = *function->scriptFunctionIndex;
                    }
                    break;
                }

                case Opcodes::unknownPush:
                {
                    // Try to find the id of the referenced subroutine (if it /is/ one)
                    const auto& functionName = temporaryStrings[current->stringIndex];
                    Function* function = findFunction( functionName.c_str() );

                    // Pointer to the function this instruction comes from
                    Function* parentFunction = unknownPushFuncPtrs[0];
                    unknownPushFuncPtrs.erase( unknownPushFuncPtrs.begin() );

                    if ( !function )
                    {
                        // If it's not a subroutine, then it is a local variable!

                        current->opcode = Opcodes::getLocal;
                        auto index = parentFunction->getOrAllocLocalIndex( functionName.c_str() );
                        current->integer = index;
                    }
                    else
                    {
                        // It actually is a function, so store a pointer to it.

                        current->opcode = Opcodes::pushc_func;
                        helium_assert(function->scriptFunctionIndex);
                        current->functionIndex = *function->scriptFunctionIndex;
                    }
                    break;
                }
            }
        }
    }

    std::unique_ptr<Module> BytecodeCompiler::compile(AstNodeScript& tree, bool withDebugInformation, std::shared_ptr<std::string> unitNameString )
    {
#if HELIUM_TRACE_VALUES
        auto frame = string("compile: ") + *unitNameString;
        ValueTraceCtx tracking_ctx(frame.c_str());
#endif

        AssemblerState state{ withDebugInformation, unitNameString };

        state.linearize( tree );
        state.cook();

        // Cut-out the generated script from the Assembler State
        std::unique_ptr<Module> script = std::move(state.script);

        if (state.failed)
            return nullptr;

        return script;
    }
}

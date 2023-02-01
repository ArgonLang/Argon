// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/boolean.h>
#include <vm/datatype/bounds.h>
#include <vm/datatype/dict.h>
#include <vm/datatype/error.h>
#include <vm/datatype/function.h>
#include <vm/datatype/future.h>
#include <vm/datatype/nil.h>
#include <vm/datatype/module.h>
#include <vm/datatype/set.h>
#include <vm/datatype/struct.h>

#include <vm/importer/import.h>

#include "opcode.h"
#include "runtime.h"
#include "areval.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

ArObject *Binary(ArObject *l, ArObject *r, int offset) {
#define GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) (struct)) + (offset)))
    BinaryOp lop = nullptr;
    BinaryOp rop = nullptr;
    ArObject *result = nullptr;

    if (AR_GET_TYPE(l)->ops != nullptr)
        lop = GET_BINARY_OP(AR_GET_TYPE(l)->ops, offset);

    if (AR_GET_TYPE(r)->ops != nullptr)
        rop = GET_BINARY_OP(AR_GET_TYPE(r)->ops, offset);

    if (lop != nullptr)
        result = lop(l, r);

    if (rop != nullptr && result == nullptr && !IsPanicking())
        result = rop(l, r);

    return result;
#undef GET_BINARY_OP
}

ArObject *GetCallableFromType(ArObject **type) {
    String *key;
    ArObject *ret;

    if (AR_TYPEOF(*type, type_type_)) {
        if ((key = StringNew(((TypeInfo *) *type)->name)) == nullptr)
            argon::vm::DiscardLastPanic();

        if ((ret = AttributeLoad(*type, (ArObject *) key, true)) == nullptr) {
            argon::vm::DiscardLastPanic();
            Release(key);
            return *type;
        }

        Release(key);

        Release(type);
        *type = ret;
    }

    return *type;
}

ArObject *Subscribe(ArObject *subscr, ArObject *index) {
    if (!AR_ISSUBSCRIPTABLE(subscr)) {
        ErrorFormat(kTypeError[0], "'%s' not subscriptable", AR_TYPE_NAME(subscr));
        return nullptr;
    }

    if (AR_TYPEOF(index, type_bounds_)) {
        if (AR_SLOT_SUBSCRIPTABLE(subscr)->get_slice == nullptr) {
            ErrorFormat(kTypeError[0], "'%s' does not support slice operations", AR_TYPE_NAME(subscr));
            return nullptr;
        }

        return AR_SLOT_SUBSCRIPTABLE(subscr)->get_slice(subscr, index);
    }

    if (AR_SLOT_SUBSCRIPTABLE(subscr)->get_item == nullptr) {
        ErrorFormat(kTypeError[0], "'%s' does not support index operations", AR_TYPE_NAME(subscr));
        return nullptr;
    }

    return AR_GET_TYPE(subscr)->subscriptable->get_item(subscr, index);
}

bool STSubscribe(ArObject *subscr, ArObject *index, ArObject *value) {
    if (!AR_ISSUBSCRIPTABLE(subscr)) {
        ErrorFormat(kTypeError[0], "'%s' not subscriptable", AR_TYPE_NAME(subscr));
        return false;
    }

    if (AR_TYPEOF(index, type_bounds_)) {
        if (AR_SLOT_SUBSCRIPTABLE(subscr)->set_slice == nullptr) {
            ErrorFormat(kTypeError[0], "'%s' does not support slice operations", AR_TYPE_NAME(subscr));
            return false;
        }

        return AR_SLOT_SUBSCRIPTABLE(subscr)->set_slice(subscr, index, value);
    }

    if (AR_SLOT_SUBSCRIPTABLE(subscr)->set_item == nullptr) {
        ErrorFormat(kTypeError[0], "'%s' does not support index operations", AR_TYPE_NAME(subscr));
        return false;
    }

    return AR_SLOT_SUBSCRIPTABLE(subscr)->set_item(subscr, index, value);
}

bool CallDefer(Fiber *fiber, Frame **cu_frame, const Code **cu_code) {
    ArObject *ret;
    Defer *defer;
    Frame *frame;

    defer = (*cu_frame)->defer;

    while (defer != nullptr && defer->function->IsNative()) {
        ret = FunctionInvokeNative(defer->function, nullptr, 0,
                                   ENUMBITMASK_ISTRUE(defer->mode, OpCodeCallMode::KW_PARAMS));
        Release(ret);

        defer = DeferPop(&(*cu_frame)->defer);
    }

    if (defer == nullptr)
        return false;

    if ((frame = FrameNew(fiber, defer->function, defer->args, defer->count, defer->mode)) == nullptr)
        return false;

    DeferPop(&(*cu_frame)->defer);

    *cu_frame = frame;
    *cu_code = frame->code;

    FiberPushFrame(fiber, frame);

    return true;
}

bool CallFunction(Fiber *fiber, Frame **cu_frame, const Code **cu_code, bool validate_only) {
    ArObject **eval_stack;
    ArObject **args;
    ArObject *ret;
    Frame *old_frame;
    Function *func;

    ArSize stack_size;
    ArSize args_length;
    ArSize positional_args;
    OpCodeCallMode mode;

    old_frame = *cu_frame;
    stack_size = I16Arg((*cu_frame)->instr_ptr);
    mode = I32Flag<OpCodeCallMode>((*cu_frame)->instr_ptr);

    eval_stack = (*cu_frame)->eval_stack - stack_size;
    func = (Function *) GetCallableFromType((*cu_frame)->eval_stack - (stack_size + 1));

    args = eval_stack;
    args_length = stack_size;

    if (stack_size > 0 && *args == nullptr) {
        args++;
        args_length--;
    }

    if (!AR_TYPEOF(func, type_function_)) {
        ErrorFormat(kTypeError[0], kTypeError[9], AR_TYPE_NAME(func));
        return false;
    }

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::REST_PARAMS)) {
        args = ((List *) *eval_stack)->objects;
        args_length = ((List *) *eval_stack)->length;
    }

    positional_args = args_length;

    if (func->currying != nullptr)
        positional_args += func->currying->length;

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS)) {
        if (!func->IsKWArgs()) {
            ErrorFormat(kTypeError[0], kTypeError[4], ARGON_RAW_STRING(func->qname));
            return false;
        }

        positional_args--;
    }

    ret = nullptr;

    if (positional_args < func->arity) {
        if (positional_args == 0 || validate_only) {
            ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, positional_args);
            return false;
        }

        ret = (ArObject *) FunctionNew(func, args, args_length);
        goto CLEANUP;
    }

    if (positional_args > func->arity && !func->IsVariadic()) {
        ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, positional_args);
        return false;
    }

    if (validate_only)
        return true;

    if (func->IsNative()) {
        ret = FunctionInvokeNative(func, args, args_length, ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS));
        if (ret == nullptr)
            return false;

        goto CLEANUP;
    }

    if (!func->IsAsync()) {
        auto *frame = FrameNew(fiber, func, args, args_length, mode);
        if (frame == nullptr)
            return false;

        *cu_frame = frame;
        *cu_code = frame->code;

        FiberPushFrame(fiber, frame);
    } else {
        if ((ret = (ArObject *) EvalAsync(func, args, args_length, mode)) == nullptr)
            return false;
    }

    CLEANUP:
    for (ArSize i = 0; i < stack_size; i++)
        Release(eval_stack[i]);

    old_frame->eval_stack -= (stack_size + 1);
    Release(old_frame->eval_stack); // Release function

    if (ret != nullptr)
        *(old_frame->eval_stack++) = ret;

    old_frame->instr_ptr += OpCodeOffset[*(old_frame->instr_ptr)];

    return true;
}

bool PopExecutedFrame(Fiber *fiber, const Code **out_code, Frame **out_frame, ArObject **ret) {
    auto *cu_frame = *out_frame;

    do {
        for (ArSize i = cu_frame->eval_stack - cu_frame->extra; i > 0; i--)
            Release(*(--cu_frame->eval_stack));

        cu_frame->eval_stack = nullptr;

        if (cu_frame->defer != nullptr && CallDefer(fiber, out_frame, out_code))
            return true; // Continue execution on new frame

        *ret = IncRef(cu_frame->return_value);

        FrameDel(FiberPopFrame(fiber));

        if (fiber->frame == nullptr) {
            if (IsPanicking())
                Release(ret);

            return false;
        }

        *out_frame = fiber->frame;
        *out_code = (*out_frame)->code;

        cu_frame = *out_frame;

        if (cu_frame->eval_stack == nullptr) {
            // Deferred call
            Release(ret);
            continue;
        }

        *cu_frame->eval_stack = *ret;
        cu_frame->eval_stack++;
    } while (IsPanicking());

    return true;
}

bool Spawn(Frame *cu_frame) {
    ArObject **eval_stack;
    ArObject **args;
    Function *func;

    ArSize stack_size;
    ArSize args_length;
    ArSize positional_args;
    OpCodeCallMode mode;

    stack_size = I16Arg(cu_frame->instr_ptr);
    mode = I32Flag<OpCodeCallMode>(cu_frame->instr_ptr);

    eval_stack = cu_frame->eval_stack - stack_size;
    func = (Function *) *(cu_frame->eval_stack - (stack_size + 1));

    args = eval_stack;
    args_length = stack_size;

    if (stack_size > 0 && *args == nullptr) {
        args++;
        args_length--;
    }

    if (!AR_TYPEOF(func, type_function_)) {
        ErrorFormat(kTypeError[0], kTypeError[9], AR_TYPE_NAME(func));
        return false;
    }

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::REST_PARAMS)) {
        args = ((List *) *eval_stack)->objects;
        args_length = ((List *) *eval_stack)->length;
    }

    positional_args = args_length;

    if (func->currying != nullptr)
        positional_args += func->currying->length;

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS)) {
        if (!func->IsKWArgs()) {
            ErrorFormat(kTypeError[0], kTypeError[4], ARGON_RAW_STRING(func->qname));
            return false;
        }

        positional_args--;
    }

    if (positional_args < func->arity) {
        ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, positional_args);
        return false;
    }

    if (positional_args > func->arity && !func->IsVariadic()) {
        ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, positional_args);
        return false;
    }

    if (func->IsGenerator()) {
        ErrorFormat(kTypeError[0], kTypeError[6], "spawn", ARGON_RAW_STRING(func->qname));
        return false;
    }

    bool ok = Spawn(func, args, args_length, mode);

    for (ArSize i = 0; i < stack_size; i++)
        Release(eval_stack[i]);

    cu_frame->eval_stack -= (stack_size + 1);
    Release(cu_frame->eval_stack); // Release function

    return ok;
}

int Unpack(ArObject *iterable, ArObject **eval_stack, int len) {
    ArObject *iter;
    ArObject *item;

    if (!AR_ISITERABLE(iterable)) {
        ErrorFormat(kTypeError[0], "unpacking expression was expecting an iterable not a '%s'", AR_TYPE_NAME(iterable));
        return false;
    }

    if ((iter = IteratorGet(iterable, false)) == nullptr)
        return false;

    eval_stack += len;

    auto count = 0;
    while ((item = IteratorNext(iter)) != nullptr && count < len)
        *(eval_stack - ++count) = item;

    Release(iter);

    if (count != len) {
        // Revert partial changes
        for (auto i = 0; i < count; i++)
            Release(*(eval_stack - i));

        ErrorFormat(kTypeError[0], "incompatible number of value to unpack (expected '%d' got '%d')", len, count);

        return -1;
    }

    return count;
}

ArObject *argon::vm::Eval(Fiber *fiber) {
#ifndef ARGON_FF_COMPUTED_GOTO
#define TARGET_OP(op) case OpCode::op:

#define CGOTO   continue
#endif

// STACK MANIPULATION MACRO
#define DISPATCH()                                              \
    cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];  \
    CGOTO

#define DISPATCH_YIELD()                                        \
    cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];  \
    if(fiber->status != FiberStatus::RUNNING) return nullptr;   \
    CGOTO

#define JUMPTO(offset)                                                  \
    cu_frame->instr_ptr = (unsigned char *)cu_code->instr + (offset);   \
    continue

#define PEEK1() (*(cu_frame->eval_stack - 2))
#define PEEK2() (*(cu_frame->eval_stack - 3))
#define PEEK3() (*(cu_frame->eval_stack - 4))
#define POP()   Release(*(--cu_frame->eval_stack))

#define PUSH(obj) do {              \
    *cu_frame->eval_stack = obj;    \
    cu_frame->eval_stack++;         \
    } while(0)

#define STACK_REWIND(offset)    for(ArSize i = offset; i>0; POP(), i--)

#define TOP()   (*(cu_frame->eval_stack - 1))

#define TOP_REPLACE(obj) do {               \
    Release(*(cu_frame->eval_stack - 1));   \
    *(cu_frame->eval_stack - 1) = obj;      \
    } while(0)

#define BINARY_OP(op, opchar)                                                                                       \
    if ((ret = Binary(PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr) {                                         \
        if (!IsPanicking()) {                                                                                       \
            ErrorFormat(kRuntimeError[0], kRuntimeError[2], #opchar, AR_TYPE_NAME(PEEK1()), AR_TYPE_NAME(TOP()));   \
        }                                                                                                           \
        break; }                                                                                            \
    POP();                                                                                                          \
    TOP_REPLACE(ret);                                                                                               \
    DISPATCH()

#define UNARY_OP(op, opchar)                                                                                    \
    ret = TOP();                                                                                                \
    if(AR_GET_TYPE(ret)->ops == nullptr || AR_GET_TYPE(ret)->ops->op == nullptr) {                              \
        ErrorFormat(kRuntimeError[0], kRuntimeError[1], #opchar, AR_TYPE_NAME(ret));                            \
        break; }                                                                                        \
    if((ret = AR_GET_TYPE(ret)->ops->op(ret)) == nullptr)                                                       \
        break;                                                                                          \
    TOP_REPLACE(ret);                                                                                           \
    DISPATCH()

    Frame *cu_frame = fiber->frame;
    const Code *cu_code = cu_frame->code;

    ArObject *ret = nullptr;

    if (IsPanicking() && !PopExecutedFrame(fiber, &cu_code, &cu_frame, &ret))
        return ret;

    while (cu_frame->instr_ptr < cu_code->instr_end) {
        switch (*((OpCode *) cu_frame->instr_ptr)) {
            TARGET_OP(ADD) {
                BINARY_OP(add, +);
            }
            TARGET_OP(AWAIT) {
                auto *future = (Future *) TOP();

                if (!AR_TYPEOF(future, type_future_)) {
                    ErrorFormat(kTypeError[0], kTypeError[2], type_future_->name, AR_TYPE_NAME(TOP()));
                    break;
                }

                if (!FutureAWait(future))
                    return nullptr;

                if ((ret = (ArObject *) FutureResult(future)) == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(CALL) {
                if (!CallFunction(fiber, &cu_frame, &cu_code, false))
                    break;

                continue;
            }
            TARGET_OP(CMP) {
                if ((ret = Compare(PEEK1(), TOP(), (CompareMode) I16Arg(cu_frame->instr_ptr))) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(CNT) {
                ret = TOP();

                if (!AR_ISSUBSCRIPTABLE(ret) || AR_GET_TYPE(ret)->subscriptable->item_in == nullptr) {
                    ErrorFormat(kRuntimeError[0], kRuntimeError[1], "in", AR_TYPE_NAME(ret));
                    break;
                }

                if ((ret = AR_GET_TYPE(ret)->subscriptable->item_in(TOP(), PEEK1())) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(DEC) {
                UNARY_OP(dec, --);
            }
            TARGET_OP(DFR) {
                auto mode = I32Flag<OpCodeCallMode>(cu_frame->instr_ptr);
                auto count = I16Arg(cu_frame->instr_ptr);
                auto *func = (Function *) *(cu_frame->eval_stack - (count + 1));

                if (!AR_TYPEOF(func, type_function_)) {
                    ErrorFormat(kTypeError[0], kTypeError[9], AR_TYPE_NAME(func));
                    break;
                }

                if (func->IsAsync()) {
                    ErrorFormat(kTypeError[0], kTypeError[6], "defer", ARGON_RAW_STRING(func->qname));
                    break;
                }

                if (func->IsGenerator()) {
                    ErrorFormat(kTypeError[0], kTypeError[7], "defer", ARGON_RAW_STRING(func->qname));
                    break;
                }

                if (!CallFunction(fiber, &cu_frame, &cu_code, true))
                    break;

                if (!DeferPush(&(cu_frame->defer), func, cu_frame->eval_stack - count, count, mode))
                    break;

                STACK_REWIND(count + 1);
                DISPATCH();
            }
            TARGET_OP(DIV) {
                BINARY_OP(div, /);
            }
            TARGET_OP(DUP) {
                auto items = I16Arg(cu_frame->instr_ptr);
                auto **cursor = cu_frame->eval_stack - items;

                while (items--)
                    PUSH(IncRef(*(cursor++)));

                DISPATCH();
            }
            TARGET_OP(EQST) {
                auto mode = (CompareMode) I16Arg(cu_frame->instr_ptr);

                if (!AR_SAME_TYPE(PEEK1(), TOP())) {
                    ret = BoolToArBool(mode == CompareMode::NE);

                    POP();
                    TOP_REPLACE(ret);
                    DISPATCH();
                }

                if ((ret = Compare(PEEK1(), TOP(), (CompareMode) I16Arg(cu_frame->instr_ptr))) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(EXTD) {
                if (!AR_TYPEOF(PEEK1(), type_list_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during EXTD execution");
                    break;
                }

                if (!ListExtend((List *) PEEK1(), TOP()))
                    break;

                POP();
                DISPATCH();
            }
            TARGET_OP(IDIV) {
                BINARY_OP(idiv, '//');
            }
            TARGET_OP(IMPALL) {
                if (!NamespaceMergePublic(cu_frame->globals, ((Module *) TOP())->ns))
                    break;

                POP();
                DISPATCH();
            }
            TARGET_OP(IMPFRM) {
                auto attribute = TupleGet(cu_code->statics, I32Arg(cu_frame->instr_ptr));

                ret = AttributeLoad(TOP(), attribute, false);

                Release(attribute);

                if (ret == nullptr)
                    break;

                PUSH(ret);
                DISPATCH();
            }
            TARGET_OP(IMPMOD) {
                auto *mod_name = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));

                ret = (ArObject *) importer::LoadModule(fiber->context->imp, (String *) mod_name, nullptr);

                Release(mod_name);

                if (ret == nullptr) {
                    if (fiber->status != FiberStatus::RUNNING)
                        return nullptr;

                    break;
                }

                PUSH(ret);
                DISPATCH_YIELD();
            }
            TARGET_OP(INC) {
                UNARY_OP(inc, ++);
            }
            TARGET_OP(INIT) {
                auto args = I16Arg(cu_frame->instr_ptr);
                auto mode = I32Flag<OpCodeInitMode>(cu_frame->instr_ptr);

                ret = (ArObject *) StructNew((TypeInfo *) *(cu_frame->eval_stack - (args + 1)),
                                             cu_frame->eval_stack - args, args, mode);
                if (ret == nullptr)
                    break;

                STACK_REWIND(args);
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(INV) {
                UNARY_OP(invert, ~);
            }
            TARGET_OP(IPADD) {
                BINARY_OP(inp_add, +=);
            }
            TARGET_OP(IPSUB) {
                BINARY_OP(inp_sub, -=);
            }
            TARGET_OP(JF) {
                // JUMP IF FALSE
                if (!IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                POP();
                DISPATCH();
            }
            TARGET_OP(JFOP) {
                // JUMP FALSE OR POP
                if (IsTrue(TOP())) {
                    POP();
                    DISPATCH();
                }

                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(JMP) {
                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(JNIL) {
                // JUMP IF NIL
                if (TOP() == (ArObject *) Nil) {
                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                DISPATCH();
            }
            TARGET_OP(JNN) {
                // JUMP IF NOT NIL
                if (TOP() != (ArObject *) Nil) {
                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                DISPATCH();
            }
            TARGET_OP(JT) {
                // JUMP IF TRUE
                if (IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                POP();
                DISPATCH();
            }
            TARGET_OP(JTOP) {
                // JUMP TRUE OR POP
                if (!IsTrue(TOP())) {
                    POP();
                    DISPATCH();
                }

                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(LAND) {
                BINARY_OP(l_and, &);
            }
            TARGET_OP(LDATTR) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[3], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    break;
                }

                ret = AttributeLoad(TOP(), key, false);

                Release(key);

                if (ret == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(LDENC) {
                PUSH(ListGet(cu_frame->enclosed, I16Arg(cu_frame->instr_ptr)));
                DISPATCH();
            }
            TARGET_OP(LDGBL) {
                auto *key = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));

                if ((ret = NamespaceLookup(cu_frame->globals, key, nullptr)) != nullptr) {
                    Release(key);

                    PUSH(ret);
                    DISPATCH();
                }

                if ((ret = NamespaceLookup(fiber->context->builtins->ns, key, nullptr)) != nullptr) {
                    Release(key);

                    PUSH(ret);
                    DISPATCH();
                }

                ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[1], ARGON_RAW_STRING((String *) key));
                Release(key);

                break;
            }
            TARGET_OP(LDITER) {
                if ((ret = IteratorGet(TOP(), false)) == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(LDLC) {
                PUSH(IncRef(cu_frame->locals[I16Arg(cu_frame->instr_ptr)]));
                DISPATCH();
            }
            TARGET_OP(LDMETH) {
                auto *instance = TOP();
                ArObject *key;

                if ((key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr))) == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[3], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    break;
                }

                bool is_method;
                ret = AttributeLoadMethod(instance, key, &is_method);

                Release(key);

                if (ret == nullptr)
                    break;

                if (is_method) {
                    *(cu_frame->eval_stack - 1) = ret;
                    PUSH(instance);
                } else {
                    TOP_REPLACE(ret);
                    PUSH(nullptr);
                }

                DISPATCH();
            }
            TARGET_OP(LDSCOPE) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    break;
                }

                ret = AttributeLoad(TOP(), key, true);
                Release(key);

                if (ret == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(LOR) {
                BINARY_OP(l_or, |);
            }
            TARGET_OP(LSTATIC) {
                PUSH(TupleGet(cu_code->statics, I32Arg(cu_frame->instr_ptr)));
                DISPATCH();
            }
            TARGET_OP(LXOR) {
                BINARY_OP(l_xor, ^);
            }
            TARGET_OP(MKBND) {
                ArObject *stop = TOP();
                ArObject *start = PEEK1();

                ret = (ArObject *) BoundsNew(start, stop, (ArObject *) Nil);
                if (ret == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MKDT) {
                auto args = I32Arg(cu_frame->instr_ptr);
                auto dict = DictNew();
                bool ok = true;
                int idx = 0;

                if (dict == nullptr)
                    break;

                // Fill dict
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor += 2) {
                    if (!DictInsert(dict, *cursor, *(cursor + 1))) {
                        ok = false;
                        break;
                    }

                    Release(*cursor);
                    Release(*(cursor + 1));

                    idx++;
                }

                if (!ok) {
                    STACK_REWIND(args - idx);
                    cu_frame->eval_stack -= args;

                    Release(dict);

                    break;
                }

                cu_frame->eval_stack -= args;
                PUSH((ArObject *) dict);
                DISPATCH();
            }
            TARGET_OP(MKFN) {
                auto flags = I32Flag<FunctionFlags>(cu_frame->instr_ptr);
                auto *name = (String *) PEEK2();
                auto *qname = (String *) PEEK1();
                List *enclosed = nullptr;

                ret = TOP();

                if (ENUMBITMASK_ISTRUE(flags, FunctionFlags::CLOSURE)) {
                    enclosed = (List *) TOP();
                    ret = PEEK1();
                    qname = (String *) PEEK2();
                    name = (String *) PEEK3();
                }

                ret = (ArObject *) FunctionNew((Code *) ret, name, qname, cu_frame->globals,
                                               enclosed, I16Arg(cu_frame->instr_ptr), flags);
                if (ret == nullptr)
                    break;

                if (ENUMBITMASK_ISTRUE(flags, FunctionFlags::CLOSURE))
                    POP();

                POP(); // Name
                POP(); // QName
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MKLT) {
                auto args = I32Arg(cu_frame->instr_ptr);
                auto list = ListNew(args);

                if (list == nullptr)
                    break;

                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    ListAppend(list, *cursor);
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH((ArObject *) list);
                DISPATCH();
            }
            TARGET_OP(MKNS) {
                if ((ret = (ArObject *) NamespaceNew()) == nullptr)
                    break;

                PUSH(ret);
                DISPATCH();
            }
            TARGET_OP(MKST) {
                auto args = I32Arg(cu_frame->instr_ptr);
                bool ok = true;
                int idx = 0;

                if ((ret = (ArObject *) SetNew()) == nullptr)
                    break;

                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    if (!SetAdd((Set *) ret, *cursor)) {
                        ok = false;
                        break;
                    }

                    Release(*cursor);

                    idx++;
                }

                if (!ok) {
                    STACK_REWIND(args - idx);
                    cu_frame->eval_stack -= args;

                    Release(ret);
                    break;
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH();
            }
            TARGET_OP(MKSTRUCT) {
                auto trait_count = I32Arg(cu_frame->instr_ptr);
                auto *stack_base = cu_frame->eval_stack - trait_count;

                ret = StructTypeNew((String *) *(stack_base - 4),
                                    (String *) *(stack_base - 3),
                                    (String *) *(stack_base - 2),
                                    (Namespace *) *(stack_base - 1),
                                    (TypeInfo **) stack_base,
                                    trait_count);

                if (ret == nullptr)
                    break;

                STACK_REWIND(trait_count);

                POP(); // namespace
                POP(); // doc
                POP(); // qname

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MKTP) {
                auto args = I32Arg(cu_frame->instr_ptr);
                auto tuple = TupleNew(args);

                if (tuple == nullptr)
                    break;

                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    TupleInsert(tuple, *cursor, args - (cu_frame->eval_stack - cursor));
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH((ArObject *) tuple);
                DISPATCH();
            }
            TARGET_OP(MKTRAIT) {
                auto trait_count = I32Arg(cu_frame->instr_ptr);
                auto *stack_base = cu_frame->eval_stack - trait_count;

                ret = TraitNew((const char *) ARGON_RAW_STRING((String *) *(stack_base - 4)),
                               (const char *) ARGON_RAW_STRING((String *) *(stack_base - 3)),
                               (const char *) ARGON_RAW_STRING((String *) *(stack_base - 2)),
                               *(stack_base - 1),
                               (TypeInfo **) stack_base,
                               trait_count);

                if (ret == nullptr)
                    break;

                STACK_REWIND(trait_count);

                POP(); // namespace
                POP(); // doc
                POP(); // qname

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MOD) {
                BINARY_OP(mod, %);
            }
            TARGET_OP(MUL) {
                BINARY_OP(mul, *);
            }
            TARGET_OP(NEG) {
                UNARY_OP(neg, -);
            }
            TARGET_OP(NGV) {
                ret = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));

                if (!NamespaceNewSymbol(cu_frame->globals, ret, TOP(),
                                        (AttributeFlag) (I32Arg(cu_frame->instr_ptr) >> 16u)))
                    break;

                Release(ret);
                POP();
                DISPATCH();
            }
            TARGET_OP(NJE) {
                if ((ret = IteratorNext(TOP())) != nullptr) {
                    PUSH(ret);
                    DISPATCH();
                }

                if (IsPanicking())
                    break;

                POP();
                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(NOT) {
                TOP_REPLACE(BoolToArBool(!IsTrue(TOP())));
                DISPATCH();
            }
            TARGET_OP(NSTORE) {
                if (!AR_TYPEOF(PEEK2(), type_namespace_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during NSTORE execution");
                    break;
                }

                if (!NamespaceNewSymbol((Namespace *) PEEK2(), TOP(), PEEK1(),
                                        (AttributeFlag) I16Arg(cu_frame->instr_ptr)))
                    break;

                STACK_REWIND(2);
                DISPATCH();
            }
            TARGET_OP(PANIC) {
                Panic(TOP());
                POP();

                break;
            }
            TARGET_OP(PBHEAD) {
                auto len = I16Arg(cu_frame->instr_ptr);

                ret = TOP(); // Save TOP

                for (ArSize i = 0; i < len; i++)
                    *(cu_frame->eval_stack - i - 1) = *(cu_frame->eval_stack - i - 2);

                *(cu_frame->eval_stack - (len + 1)) = ret;

                DISPATCH();
            }
            TARGET_OP(PLT) {
                if (!AR_TYPEOF(PEEK1(), type_list_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during PLT execution");
                    break;
                }

                if (!ListAppend((List *) PEEK1(), TOP()))
                    break;

                POP();
                DISPATCH();
            }
            TARGET_OP(POP) {
                POP();
                DISPATCH();
            }
            TARGET_OP(POS) {
                UNARY_OP(pos, +);
            }
            TARGET_OP(RET) {
                cu_frame->return_value = TOP();

                cu_frame->eval_stack--;

                cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];
                break;
            }
            TARGET_OP(SHL) {
                BINARY_OP(shl, >>);
            }
            TARGET_OP(SHR) {
                BINARY_OP(shr, <<);
            }
            TARGET_OP(SPW) {
                if (!::Spawn(cu_frame))
                    break;

                DISPATCH();
            }
            TARGET_OP(STATTR) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    break;
                }

                if (!AttributeSet(TOP(), key, PEEK1(), false)) {
                    Release(key);
                    break;
                }

                Release(key);

                POP(); // Instance
                POP(); // Value
                DISPATCH();
            }
            TARGET_OP(STENC) {
                ListInsert(cu_frame->enclosed, TOP(), I16Arg(cu_frame->instr_ptr));
                POP();
                DISPATCH();
            }
            TARGET_OP(STGBL) {
                ret = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));
                AttributeProperty aprop{};

                if (!NamespaceContains(cu_frame->globals, ret, &aprop)) {
                    Release(ret);
                    ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[1], ARGON_RAW_STRING((String *) ret));
                    break;
                }

                if (aprop.IsConstant()) {
                    ErrorFormat(kUnassignableError[0], kUnassignableError[1], ARGON_RAW_STRING((String *) ret));
                    break;
                }

                NamespaceSet(cu_frame->globals, ret, TOP());

                Release(ret);
                POP();
                DISPATCH();
            }
            TARGET_OP(STLC) {
                auto idx = I16Arg(cu_frame->instr_ptr);

                Release(cu_frame->locals[idx]);
                cu_frame->locals[idx] = TOP();
                cu_frame->eval_stack--;
                DISPATCH();
            }
            TARGET_OP(STSCOPE) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    break;
                }

                if (!AttributeSet(TOP(), key, PEEK1(), true)) {
                    Release(key);
                    break;
                }

                Release(key);

                POP(); // Instance
                POP(); // Value
                DISPATCH();
            }
            TARGET_OP(STSUBSCR) {
                if (!STSubscribe(PEEK1(), TOP(), PEEK2()))
                    break;

                STACK_REWIND(3);
                DISPATCH();
            }
            TARGET_OP(SUB) {
                BINARY_OP(sub, -);
            }
            TARGET_OP(SUBSCR) {
                if ((ret = Subscribe(PEEK1(), TOP())) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(TEST) {
                if (Equal(PEEK1(), TOP())) {
                    POP();
                    TOP_REPLACE(BoolToArBool(true));
                    DISPATCH();
                }

                TOP_REPLACE(BoolToArBool(false));
                DISPATCH();
            }
            TARGET_OP(UNPACK) {
                ret = TOP();

                auto inc = Unpack(ret, cu_frame->eval_stack - 1, I16Arg(cu_frame->instr_ptr));
                if (inc < 0) {
                    Release(ret);
                    break;
                }

                cu_frame->eval_stack += inc - 1;

                Release(ret);
                DISPATCH();
            }
            default:
                ErrorFormat(kRuntimeError[0], "unknown opcode: 0x%X", *cu_frame->instr_ptr);
        }

        if (!PopExecutedFrame(fiber, &cu_code, &cu_frame, &ret))
            break;
    }

    return ret;
}

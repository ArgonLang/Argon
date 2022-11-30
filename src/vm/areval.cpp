// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/boolean.h>
#include <vm/datatype/bounds.h>
#include <vm/datatype/error.h>
#include <vm/datatype/function.h>
#include <vm/datatype/future.h>

#include "opcode.h"
#include "runtime.h"
#include "areval.h"
#include "vm/datatype/dict.h"
#include "vm/datatype/nil.h"

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

bool CallFunction(Fiber *fiber, Frame **cu_frame, const Code **cu_code, bool validate_only) {
    ArObject **eval_stack;
    ArObject **args;
    ArObject *ret;
    Frame *old_frame;
    Function *func;

    ArSize stack_size;
    ArSize total_args;
    ArSize positional_args;
    OpCodeCallMode mode;

    bool ok = false;

    old_frame = *cu_frame;
    stack_size = I16Arg((*cu_frame)->instr_ptr);
    mode = I32Flag<OpCodeCallMode>((*cu_frame)->instr_ptr);

    eval_stack = (*cu_frame)->eval_stack - stack_size;
    args = eval_stack;

    func = (Function *) *((*cu_frame)->eval_stack - (stack_size + 1));

    total_args = stack_size;

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::REST_PARAMS)) {
        args = ((List *) *eval_stack)->objects;
        total_args = ((List *) *eval_stack)->length;
    }

    if (func->currying != nullptr)
        total_args += func->currying->length;

    positional_args = total_args;

    if (ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS)) {
        if (!func->IsKWArgs()) {
            ErrorFormat(kTypeError[0], kTypeError[4], ARGON_RAW_STRING(func->qname));
            goto CLEANUP_ERROR;
        }

        positional_args--;
    }

    ret = nullptr;

    if (positional_args < func->arity) {
        if (positional_args == 0 || validate_only) {
            ErrorFormat(kTypeError[0], kTypeError[4], ARGON_RAW_STRING(func->qname));
            goto CLEANUP_ERROR;
        }

        ret = (ArObject *) FunctionNew(func, args, positional_args);
        goto CLEANUP;
    }

    if (positional_args > func->arity && !func->IsVariadic()) {
        ErrorFormat(kTypeError[0], kTypeError[3], ARGON_RAW_STRING(func->qname), func->arity, total_args);
        goto CLEANUP_ERROR;
    }

    if (validate_only)
        return true;

    if (func->IsNative()) {
        ret = FunctionInvokeNative(func, args, total_args, ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS));
        goto CLEANUP;
    }

    (*cu_frame)->instr_ptr += OpCodeOffset[*(*cu_frame)->instr_ptr];

    if (!func->IsAsync()) {
        auto *frame = FrameNew(fiber, func, args, total_args, mode);
        if (frame == nullptr)
            goto CLEANUP_ERROR;

        *cu_frame = frame;
        *cu_code = frame->code;

        FiberPushFrame(fiber, frame);
    } else {
        if ((ret = (ArObject *) EvalAsync(func, args, total_args, mode)) == nullptr)
            goto CLEANUP_ERROR;
    }

    CLEANUP:
    ok = true;

    CLEANUP_ERROR:
    for (ArSize i = 0; i < stack_size; i++)
        Release(eval_stack[i]);

    old_frame->eval_stack -= (stack_size + 1);
    Release(old_frame->eval_stack); // Release function

    if (ok && ret != nullptr)
        *(old_frame->eval_stack++) = ret;

    return ok;
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

ArObject *argon::vm::Eval(Fiber *fiber) {
#ifndef ARGON_FF_COMPUTED_GOTO
#define TARGET_OP(op) case OpCode::op:

#define CGOTO   continue
#endif

// STACK MANIPULATION MACRO
#define DISPATCH()                                              \
    cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];  \
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
        goto END_LOOP; }                                                                                            \
    POP();                                                                                                          \
    TOP_REPLACE(ret);                                                                                               \
    DISPATCH()

#define UNARY_OP(op, opchar)                                                                                    \
    ret = TOP();                                                                                                \
    if(AR_GET_TYPE(ret)->ops == nullptr || AR_GET_TYPE(ret)->ops->op == nullptr) {                              \
        ErrorFormat(kRuntimeError[0], kRuntimeError[1], #opchar, AR_TYPE_NAME(ret));                            \
        goto END_LOOP; }                                                                                        \
    if((ret = AR_GET_TYPE(ret)->ops->op(ret)) == nullptr)                                                       \
        goto END_LOOP;                                                                                          \
    TOP_REPLACE(ret);                                                                                           \
    DISPATCH()

    Frame *cu_frame = fiber->frame;
    const Code *cu_code = cu_frame->code;

    ArObject *ret = nullptr;
    ArObject *return_value = nullptr;

    while (cu_frame->instr_ptr < cu_code->instr_end) {
        switch (*((OpCode *) cu_frame->instr_ptr)) {
            TARGET_OP(ADD) {
                BINARY_OP(add, +);
            }
            TARGET_OP(AWAIT) {
                auto *future = (Future *) TOP();

                if (!AR_TYPEOF(future, type_future_)) {
                    ErrorFormat(kTypeError[0], kTypeError[2], type_future_->name, AR_TYPE_NAME(TOP()));
                    goto END_LOOP;
                }

                if (!FutureAWait(future))
                    return nullptr;

                if ((ret = FutureResult(future)) == nullptr)
                    goto END_LOOP;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(CALL) {
                if (!CallFunction(fiber, &cu_frame, &cu_code, false))
                    goto END_LOOP;

                continue;
            }
            TARGET_OP(CMP) {
                if ((ret = Compare(PEEK1(), TOP(), (CompareMode) I16Arg(cu_frame->instr_ptr))) == nullptr)
                    goto END_LOOP;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(CNT) {
                ret = TOP();

                if (!AR_ISSUBSCRIPTABLE(ret) || AR_GET_TYPE(ret)->subscriptable->item_in == nullptr) {
                    ErrorFormat(kRuntimeError[0], kRuntimeError[1], "in", AR_TYPE_NAME(ret));
                    goto END_LOOP;
                }

                if ((ret = AR_GET_TYPE(ret)->subscriptable->item_in(TOP(), PEEK1())) == nullptr)
                    goto END_LOOP;

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

                if (func->IsAsync()) {
                    ErrorFormat(kTypeError[0], kTypeError[6], ARGON_RAW_STRING(func->qname));
                    goto END_LOOP;
                }

                if (func->IsGenerator()) {
                    ErrorFormat(kTypeError[0], kTypeError[7], ARGON_RAW_STRING(func->qname));
                    goto END_LOOP;
                }

                if (!CallFunction(fiber, &cu_frame, &cu_code, true))
                    goto END_LOOP;

                if (!DeferPush(&(cu_frame->defer), func, cu_frame->eval_stack - count, count, mode))
                    goto END_LOOP;

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
                    goto END_LOOP;

                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(EXTD) {
                if (!AR_TYPEOF(PEEK1(), type_list_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during EXTD execution");
                    goto END_LOOP;
                }

                if (!ListExtend((List *) PEEK1(), TOP()))
                    goto END_LOOP;

                POP();
                DISPATCH();
            }
            TARGET_OP(IDIV) {
                BINARY_OP(idiv, '//');
            }
            TARGET_OP(INC) {
                UNARY_OP(inc, ++);
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

                    goto END_LOOP;
                }

                ret = AttributeLoad(TOP(), key, false);

                Release(key);

                if (ret == nullptr)
                    goto END_LOOP;

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

                // TODO: check builtins
                //ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[1], ARGON_RAW_STRING((String *) ret));
                Release(key);
                goto END_LOOP;

                PUSH(ret);
                DISPATCH();
            }
            TARGET_OP(LDITER) {
                if ((ret = IteratorGet(TOP(), false)) == nullptr)
                    goto END_LOOP;

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

                    goto END_LOOP;
                }

                bool is_method;
                ret = AttributeLoadMethod(instance, key, &is_method);

                Release(key);

                if (ret == nullptr)
                    goto END_LOOP;

                if (is_method) {
                    *(cu_frame->eval_stack - 1) = ret;
                    PUSH(instance);
                } else
                    TOP_REPLACE(ret);

                DISPATCH();
            }
            TARGET_OP(LDSCOPE) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    goto END_LOOP;
                }

                ret = AttributeLoad(TOP(), key, true);
                Release(key);

                if (ret == nullptr)
                    goto END_LOOP;

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

                cu_frame->eval_stack--;

                ret = (ArObject *) BoundsNew(start, stop, (ArObject *) Nil);
                Release(stop);
                Release(start);

                if (ret == nullptr)
                    goto END_LOOP;

                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MKDT) {
                auto args = I32Arg(cu_frame->instr_ptr);
                auto dict = DictNew();

                if (dict == nullptr)
                    goto END_LOOP;

                // Fill dict
                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor += 2) {
                    if (!DictInsert(dict, *cursor, *(cursor + 1))) {
                        Release(dict);
                        goto END_LOOP;
                    }

                    Release(*cursor);
                    Release(*(cursor + 1));
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
                    goto END_LOOP;

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
                    goto END_LOOP;

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
                    goto END_LOOP;

                PUSH(ret);
                DISPATCH();
            }
            TARGET_OP(MKTP) {
                auto args = I32Arg(cu_frame->instr_ptr);
                auto tuple = TupleNew(args);

                if (tuple == nullptr)
                    goto END_LOOP;

                for (ArObject **cursor = cu_frame->eval_stack - args; cursor != cu_frame->eval_stack; cursor++) {
                    TupleInsert(tuple, *cursor, args - (cu_frame->eval_stack - cursor));
                    Release(*cursor);
                }

                cu_frame->eval_stack -= args;
                PUSH((ArObject *) tuple);
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
                    goto END_LOOP;

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
                    goto END_LOOP;

                POP();
                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(NOT) {
                TOP_REPLACE(BoolToArBool(!IsTrue(TOP())));
                DISPATCH();
            }
            TARGET_OP(NSTORE) {
                if (!AR_TYPEOF(PEEK1(), type_namespace_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during NSTORE execution");
                    goto END_LOOP;
                }

                if (!NamespaceNewSymbol((Namespace *) PEEK2(), PEEK1(), TOP(),
                                        (AttributeFlag) I16Arg(cu_frame->instr_ptr)))
                    goto END_LOOP;

                STACK_REWIND(3);
                DISPATCH();
            }
            TARGET_OP(PANIC) {
                Panic(TOP());
                POP();

                goto END_LOOP;
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
                    goto END_LOOP;
                }

                if (!ListAppend((List *) PEEK1(), TOP()))
                    goto END_LOOP;

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
            TARGET_OP(STATTR) {
                auto *key = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
                                cu_code->statics->length);

                    goto END_LOOP;
                }

                if (!AttributeSet(TOP(), key, PEEK1(), false)) {
                    Release(key);
                    goto END_LOOP;
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
                    goto END_LOOP;
                }

                if (aprop.IsConstant()) {
                    ErrorFormat(kUnassignableError[0], kUnassignableError[1], ARGON_RAW_STRING((String *) ret));
                    goto END_LOOP;
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

                    goto END_LOOP;
                }

                if (!AttributeSet(TOP(), key, PEEK1(), true)) {
                    Release(key);
                    goto END_LOOP;
                }

                Release(key);

                POP(); // Instance
                POP(); // Value
                DISPATCH();
            }
            TARGET_OP(STSUBSCR) {
                auto *subscr = PEEK1();

                if (!AR_ISSUBSCRIPTABLE(subscr) || AR_GET_TYPE(subscr)->subscriptable->set_item == nullptr) {
                    ErrorFormat(kTypeError[0], "'%s' not subscriptable", AR_TYPE_NAME(subscr));
                    return nullptr;
                }

                if (!AR_GET_TYPE(subscr)->subscriptable->set_item(subscr, TOP(), PEEK2()))
                    goto END_LOOP;

                STACK_REWIND(3);
                DISPATCH();
            }
            TARGET_OP(SUB) {
                BINARY_OP(sub, -);
            }
            TARGET_OP(SUBSCR) {
                if ((ret = Subscribe(PEEK1(), TOP())) == nullptr)
                    goto END_LOOP;

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
                    goto END_LOOP;
                }

                cu_frame->eval_stack += inc - 1;

                Release(ret);
                DISPATCH();
            }
            default:
                ErrorFormat(kRuntimeError[0], "unknown opcode: 0x%X", *cu_frame->instr_ptr);
                goto END_LOOP;
        }

        END_LOOP:

        do {
            STACK_REWIND(cu_frame->eval_stack - cu_frame->extra);

            cu_frame->eval_stack = nullptr;

            if (cu_frame->defer != nullptr &&
                CallDefer(fiber, &cu_frame, &cu_code))
                break;

            ret = IncRef(cu_frame->return_value);

            FrameDel(FiberPopFrame(fiber));

            if (fiber->frame == nullptr)
                goto END;

            cu_frame = fiber->frame;
            cu_code = cu_frame->code;

            if (cu_frame->eval_stack == nullptr) {
                // Deferred call
                Release(ret);
                continue;
            }

            PUSH(ret);
        } while (IsPanicking());
    }

    END:

    if (IsPanicking()) {
        Release(ret);
        return nullptr;
    }

    return ret;
}

// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <vm/datatype/boolean.h>
#include <vm/datatype/bounds.h>
#include <vm/datatype/error.h>
#include <vm/datatype/function.h>

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

    while (cu_frame->instr_ptr < cu_code->instr_end) {
        switch (*((OpCode *) cu_frame->instr_ptr)) {
            TARGET_OP(ADD) {
                BINARY_OP(add, +);
            }
            TARGET_OP(CALL) {
                auto args = I16Arg(cu_frame->instr_ptr);
                assert(false);
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

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], I32Arg(cu_frame->instr_ptr),
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
                auto flags = (FunctionFlags) I32Flag(cu_frame->instr_ptr);
                auto *name = (String *) PEEK2();
                auto *qname = (String *) PEEK1();
                Tuple *enclosed = nullptr;

                ret = TOP();

                if (ENUMBITMASK_ISTRUE(flags, FunctionFlags::CLOSURE)) {
                    enclosed = (Tuple *) TOP();
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
    }

    END_LOOP:

    return nullptr;
}

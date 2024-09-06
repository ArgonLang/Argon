// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bounds.h>
#include <argon/vm/datatype/chan.h>
#include <argon/vm/datatype/dict.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/future.h>
#include <argon/vm/datatype/nil.h>
#include <argon/vm/datatype/module.h>
#include <argon/vm/datatype/set.h>
#include <argon/vm/datatype/struct.h>

#include <argon/vm/importer/import.h>

#include <argon/vm/defer.h>
#include <argon/vm/opcode.h>
#include <argon/vm/runtime.h>
#include <argon/vm/areval.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

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
    Frame *new_frame;
    Function *func;

    ArSize stack_size;
    ArSize args_length;
    ArSize positional_args;
    OpCodeCallMode mode;

    bool exit_ok = true;

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
        if (!func->IsKWArgs() && !func->HaveDefaults()) {
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

    new_frame = nullptr;

    if (func->IsNative()) {
        ret = FunctionInvokeNative(func, args, args_length, ENUMBITMASK_ISTRUE(mode, OpCodeCallMode::KW_PARAMS));
        if (ret == nullptr) {
            auto f_status = GetFiberStatus();
            if (f_status == FiberStatus::SUSPENDED || f_status == FiberStatus::BLOCKED_SUSPENDED)
                return false;

            exit_ok = false;
            ret = (ArObject *) IncRef(Nil);
        }

        goto CLEANUP;
    }

    if (func->IsAsync()) {
        if ((ret = (ArObject *) EvalAsync(func, args, args_length, mode)) == nullptr)
            return false;

        goto CLEANUP;
    }

    if (!func->IsRecoverable()) {
        new_frame = FrameNew(fiber, func, args, args_length, mode);
        if (new_frame == nullptr)
            return false;
    }

    if (func->IsGenerator()) {
        if (!func->IsRecoverable()) {
            if ((ret = (ArObject *) FunctionInitGenerator(func, new_frame)) == nullptr) {
                FrameDel(new_frame);
                return false;
            }

            goto CLEANUP;
        }

        if ((new_frame = (Frame *) func->LockAndGetStatus(fiber)) == nullptr)
            return false;
    }

    assert(new_frame != nullptr);

    *cu_frame = new_frame;
    *cu_code = new_frame->code;

    FiberPushFrame(fiber, new_frame);

    CLEANUP:
    for (ArSize i = 0; i < stack_size; i++)
        Release(eval_stack[i]);

    old_frame->eval_stack -= stack_size;

    if (ret != nullptr)
        Replace(old_frame->eval_stack - 1, ret);

    old_frame->instr_ptr += 4;

    return exit_ok;
}

bool PopExecutedFrame(Fiber *fiber, const Code **out_code, Frame **out_frame, ArObject **ret) {
    auto *cu_frame = *out_frame;
    auto is_panicking = IsPanickingFrame();
    auto panicking = false;

    do {
        if (cu_frame->eval_stack != nullptr) {
            for (ArSize i = cu_frame->eval_stack - cu_frame->extra; i > 0; i--)
                Release(*(--cu_frame->eval_stack));
        } else
            is_panicking = IsPanicking();

        cu_frame->eval_stack = nullptr;

        if (cu_frame->defer != nullptr && CallDefer(fiber, out_frame, out_code))
            return true; // Continue execution on new frame

        *ret = IncRef(cu_frame->return_value);

        if (!panicking && is_panicking)
            panicking = true;

        FrameDel(FiberPopFrame(fiber));

        if (fiber->frame == nullptr || fiber->unwind_limit == cu_frame) {
            if (IsPanicking())
                Release(ret);

            return false;
        }

        cu_frame = fiber->frame;

        *out_frame = fiber->frame;
        *out_code = fiber->frame->code;

        if (cu_frame->eval_stack == nullptr) {
            // Deferred call
            Release(ret);
            continue;
        }

        // It is only needed if the function is an initialized generator
        // and allows other threads to execute the frame.
        ((Function *) *(cu_frame->eval_stack - 1))->Unlock(fiber);

        Replace(cu_frame->eval_stack - 1,
                *ret != nullptr ? *ret : (ArObject *) IncRef(Nil));
    } while (cu_frame->eval_stack == nullptr || (panicking && cu_frame->trap_ptr == nullptr));

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
        if (!func->IsKWArgs() && !func->HaveDefaults()) {
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
        return -1;
    }

    if ((iter = IteratorGet(iterable, false)) == nullptr)
        return -1;

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

#define CGOTO continue
#else
#define TARGET_OP(op)   \
      case OpCode::op:  \
      LBL_##op:

#define CGOTO \
    goto *LBL_OPCODES[*(cu_frame->instr_ptr)]

    static const void *LBL_OPCODES[] = {
            &&LBL_ADD,
            &&LBL_AWAIT,
            &&LBL_CALL,
            &&LBL_CMP,
            &&LBL_CNT,
            &&LBL_DEC,
            &&LBL_DFR,
            &&LBL_DIV,
            &&LBL_DTMERGE,
            &&LBL_DUP,
            &&LBL_EQST,
            &&LBL_EXTD,
            &&LBL_IDIV,
            &&LBL_IMPALL,
            &&LBL_IMPFRM,
            &&LBL_IMPMOD,
            &&LBL_INC,
            &&LBL_INIT,
            &&LBL_INV,
            &&LBL_IPADD,
            &&LBL_IPSUB,
            &&LBL_JEX,
            &&LBL_JF,
            &&LBL_JFOP,
            &&LBL_JMP,
            &&LBL_JNIL,
            &&LBL_JNN,
            &&LBL_JT,
            &&LBL_JTOP,
            &&LBL_LAND,
            &&LBL_LDATTR,
            &&LBL_LDENC,
            &&LBL_LDGBL,
            &&LBL_LDITER,
            &&LBL_LDLC,
            &&LBL_LDMETH,
            &&LBL_LDSCOPE,
            &&LBL_LOR,
            &&LBL_LSTATIC,
            &&LBL_LXOR,
            &&LBL_MKBND,
            &&LBL_MKDT,
            &&LBL_MKFN,
            &&LBL_MKLT,
            &&LBL_MKST,
            &&LBL_MKSTRUCT,
            &&LBL_MKTP,
            &&LBL_MKTRAIT,
            &&LBL_MOD,
            &&LBL_MTH,
            &&LBL_MUL,
            &&LBL_NEG,
            &&LBL_NGV,
            &&LBL_NOT,
            &&LBL_NXT,
            &&LBL_PANIC,
            &&LBL_PLT,
            &&LBL_POP,
            &&LBL_POPC,
            &&LBL_POPGT,
            &&LBL_POS,
            &&LBL_PSHC,
            &&LBL_PSHN,
            &&LBL_RET,
            &&LBL_SHL,
            &&LBL_SHR,
            &&LBL_SPW,
            &&LBL_ST,
            &&LBL_STATTR,
            &&LBL_STENC,
            &&LBL_STGBL,
            &&LBL_STLC,
            &&LBL_STSCOPE,
            &&LBL_STSUBSCR,
            &&LBL_SUB,
            &&LBL_SUBSCR,
            &&LBL_SYNC,
            &&LBL_TEST,
            &&LBL_TRAP,
            &&LBL_TSTORE,
            &&LBL_UNPACK,
            &&LBL_UNSYNC,
            &&LBL_YLD
    };
#endif

// STACK MANIPULATION MACRO
#define DISPATCH()                                                  \
    cu_frame->instr_ptr += OpCodeOffset[*cu_frame->instr_ptr];      \
    CGOTO

#define DISPATCH1()                                                 \
    cu_frame->instr_ptr += 1;                                       \
    CGOTO

#define DISPATCH2()                                                 \
    cu_frame->instr_ptr += 2;                                       \
    CGOTO

#define DISPATCH4()                                                 \
    cu_frame->instr_ptr += 4;                                       \
    CGOTO

#define DISPATCH_YIELD(op_size)                                     \
    cu_frame->instr_ptr += op_size;                                 \
    if(GetFiberStatus() != FiberStatus::RUNNING) return nullptr;    \
    CGOTO

#define JUMPADDR(offset) (unsigned char *) (cu_code->instr + (offset))

#define JUMPTO(offset)                          \
    cu_frame->instr_ptr = JUMPADDR(offset);     \
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

#define BINARY_OP4(first, second, op, opchar)                                                                       \
        if ((ret = ExecBinaryOpOriented(first, second, offsetof(OpSlots, op))) == nullptr) {                        \
        if (!IsPanickingFrame()) {                                                                                  \
            ErrorFormat(kRuntimeError[0], kRuntimeError[2], #opchar, AR_TYPE_NAME(first), AR_TYPE_NAME(second));    \
        }                                                                                                           \
        break; }                                                                                                    \

#define BINARY_OP(op, opchar)                                                                                       \
    if ((ret = ExecBinaryOp(PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr) {                                   \
        if (!IsPanickingFrame()) {                                                                                  \
            ErrorFormat(kRuntimeError[0], kRuntimeError[2], #opchar, AR_TYPE_NAME(PEEK1()), AR_TYPE_NAME(TOP()));   \
        }                                                                                                           \
        break; }                                                                                                    \
    POP();                                                                                                          \
    TOP_REPLACE(ret);                                                                                               \
    DISPATCH1()

#define UNARY_OP(op, opchar)                                                                                        \
    ret = TOP();                                                                                                    \
    if(AR_GET_TYPE(ret)->ops == nullptr || AR_GET_TYPE(ret)->ops->op == nullptr) {                                  \
        ErrorFormat(kRuntimeError[0], kRuntimeError[1], #opchar, AR_TYPE_NAME(ret));                                \
        break; }                                                                                                    \
    if((ret = AR_GET_TYPE(ret)->ops->op(ret)) == nullptr)                                                           \
        break;                                                                                                      \
    TOP_REPLACE(ret);                                                                                               \
    DISPATCH1()

    Frame *cu_frame = fiber->frame;
    const Code *cu_code = cu_frame->code;

    ArObject *ret = nullptr;

    if (IsPanickingFrame()) {
        if ((uintptr_t) cu_frame->trap_ptr > 0)
            cu_frame->instr_ptr = cu_frame->trap_ptr;
        else if (!PopExecutedFrame(fiber, &cu_code, &cu_frame, &ret))
            return nullptr;
    }

    while (cu_frame->instr_ptr < cu_code->instr_end) {
        switch (*((OpCode *) cu_frame->instr_ptr)) {
            TARGET_OP(ADD) {
                BINARY_OP(add, +);
            }
            TARGET_OP(AWAIT) {
                auto *future = (Future *) TOP();
                if (!AR_TYPEOF(future, type_future_)) {
                    ErrorFormat(kTypeError[0], kTypeError[2], type_future_->name, AR_TYPE_NAME(future));
                    break;
                }

                if (!FutureAWait(future))
                    return nullptr;

                if ((ret = (ArObject *) FutureResult(future)) == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(CALL) {
                bool call_ok = CallFunction(fiber, &cu_frame, &cu_code, false);

                if (GetFiberStatus() != FiberStatus::RUNNING)
                    return nullptr;

                if (!call_ok)
                    break;

                continue;
            }
            TARGET_OP(CMP) {
                if ((ret = Compare(PEEK1(), TOP(), (CompareMode) I16Arg(cu_frame->instr_ptr))) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(CNT) {
                auto mode = (vm::OpCodeContainsMode) I16Arg(cu_frame->instr_ptr);
                ret = TOP();

                if (!AR_ISSUBSCRIPTABLE(ret) || AR_SLOT_SUBSCRIPTABLE(ret)->item_in == nullptr) {
                    ErrorFormat(kRuntimeError[0],
                                kRuntimeError[1],
                                mode == OpCodeContainsMode::IN ? "in" : "not in",
                                AR_TYPE_NAME(ret));

                    break;
                }

                if ((ret = AR_SLOT_SUBSCRIPTABLE(ret)->item_in(ret, PEEK1())) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);

                if (mode == OpCodeContainsMode::NOT_IN)
                    TOP_REPLACE(BoolToArBool(!IsTrue(ret)));

                DISPATCH2();
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
                DISPATCH4();
            }
            TARGET_OP(DIV) {
                BINARY_OP(div, /);
            }
            TARGET_OP(DTMERGE) {
                ret = (ArObject *) DictMerge((Dict *) PEEK1(), (Dict *) TOP(), false);
                if (ret == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(DUP) {
                auto items = I16Arg(cu_frame->instr_ptr);
                auto **cursor = cu_frame->eval_stack - items;

                while (items--)
                    PUSH(IncRef(*(cursor++)));

                DISPATCH2();
            }
            TARGET_OP(EQST) {
                auto mode = (CompareMode) I16Arg(cu_frame->instr_ptr);
                const auto *self = PEEK1();
                const auto *other = TOP();

                if (!AR_SAME_TYPE(self, other)) {
                    ret = BoolToArBool(mode == CompareMode::NE);

                    POP();
                    TOP_REPLACE(ret);
                    DISPATCH2();
                }

                if ((ret = Compare(self, other, mode)) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(EXTD) {
                ret = PEEK1();

                if (!AR_TYPEOF(ret, type_list_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during EXTD execution");
                    break;
                }

                if (!ListExtend((List *) ret, TOP()))
                    break;

                POP();
                DISPATCH1();
            }
            TARGET_OP(IDIV) {
                BINARY_OP(idiv, '//');
            }
            TARGET_OP(IMPALL) {
                if (!NamespaceMergePublic(cu_frame->globals, ((Module *) TOP())->ns))
                    break;

                POP();
                DISPATCH1();
            }
            TARGET_OP(IMPFRM) {
                auto attribute = TupleGet(cu_code->statics, I32Arg(cu_frame->instr_ptr));

                ret = AttributeLoad(TOP(), attribute, false);

                Release(attribute);

                if (ret == nullptr)
                    break;

                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(IMPMOD) {
                auto *mod_name = TupleGet(cu_code->statics, (ArSSize) I32Arg(cu_frame->instr_ptr));

                ret = (ArObject *) importer::LoadModule(fiber->context->imp, (String *) mod_name, nullptr);

                Release(mod_name);

                if (ret != nullptr) {
                    PUSH(ret);
                    DISPATCH_YIELD(4);
                }

                if (GetFiberStatus() != FiberStatus::RUNNING)
                    return nullptr;

                break;
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
                DISPATCH4();
            }
            TARGET_OP(INV) {
                UNARY_OP(invert, ~);
            }
            TARGET_OP(IPADD) {
                auto *actual = PEEK1();

                BINARY_OP4(actual, TOP(), inp_add, +=)

                POP();

                if (actual != ret) {
                    TOP_REPLACE(ret);
                    DISPATCH1();
                }

                // Skip next STORE operation

                cu_frame->instr_ptr++;

                auto nextI = (OpCode) *cu_frame->instr_ptr;

                POP();

                if (nextI == OpCode::STSUBSCR) {
                    POP();
                    POP();
                } else if (nextI == OpCode::STSCOPE || nextI == OpCode::STATTR)
                    POP();

                DISPATCH();
            }
            TARGET_OP(IPSUB) {
                auto *actual = PEEK1();

                BINARY_OP4(actual, TOP(), inp_sub, -=)

                POP();

                if (actual != ret) {
                    TOP_REPLACE(ret);
                    DISPATCH1();
                }

                // Skip next STORE operation

                cu_frame->instr_ptr++;

                auto nextI = (OpCode) *cu_frame->instr_ptr;

                POP();

                if (nextI == OpCode::STSUBSCR) {
                    POP();
                    POP();
                } else if (nextI == OpCode::STSCOPE || nextI == OpCode::STATTR)
                    POP();

                DISPATCH();
            }
            TARGET_OP(JEX) {
                const auto *peek = (Function *) PEEK1();

                if (AR_TYPEOF(peek, type_function_) && peek->IsExhausted() || TOP() == nullptr) {
                    POP(); // POP nullptr value of an exhausted iterator, or the return value of a generator
                    POP(); // POP iterator/generator

                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                DISPATCH4();
            }
            TARGET_OP(JF) {
                // JUMP IF FALSE
                if (!IsTrue(TOP())) {
                    POP();

                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                POP();
                DISPATCH4();
            }
            TARGET_OP(JFOP) {
                // JUMP FALSE OR POP
                if (IsTrue(TOP())) {
                    POP();

                    DISPATCH4();
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

                DISPATCH4();
            }
            TARGET_OP(JNN) {
                // JUMP IF NOT NIL
                if (TOP() != (ArObject *) Nil) {
                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                DISPATCH4();
            }
            TARGET_OP(JT) {
                // JUMP IF TRUE
                if (IsTrue(TOP())) {
                    POP();

                    JUMPTO(I32Arg(cu_frame->instr_ptr));
                }

                POP();
                DISPATCH4();
            }
            TARGET_OP(JTOP) {
                // JUMP TRUE OR POP
                if (!IsTrue(TOP())) {
                    POP();

                    DISPATCH4();
                }

                JUMPTO(I32Arg(cu_frame->instr_ptr));
            }
            TARGET_OP(LAND) {
                BINARY_OP(l_and, &);
            }
            TARGET_OP(LDATTR) {
                auto index = (ArSSize) I32Arg(cu_frame->instr_ptr);

                auto *key = TupleGet(cu_code->statics, index);
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[3], index, cu_code->statics->length);

                    break;
                }

                ret = AttributeLoad(TOP(), key, false);

                Release(key);

                if (ret == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(LDENC) {
                PUSH(ListGet(cu_frame->enclosed, I16Arg(cu_frame->instr_ptr)));
                DISPATCH2();
            }
            TARGET_OP(LDGBL) {
                auto *key = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));

                if ((ret = NamespaceLookup(cu_frame->globals, key, nullptr)) != nullptr) {
                    Release(key);

                    PUSH(ret);
                    DISPATCH4();
                }

                if ((ret = NamespaceLookup(fiber->context->builtins->ns, key, nullptr)) != nullptr) {
                    Release(key);

                    PUSH(ret);
                    DISPATCH4();
                }

                ErrorFormat(kUndeclaredeError[0], kUndeclaredeError[1], ARGON_RAW_STRING((String *) key));

                Release(key);

                // Prevent crash when using 'trap' keyword with non-existent variables
                PUSH(nullptr);

                break;
            }
            TARGET_OP(LDITER) {
                ret = TOP();

                if (AR_TYPEOF(ret, type_function_)) {
                    if (((Function *) ret)->IsRecoverable()) {
                        DISPATCH1();
                    }

                    ErrorFormat(kTypeError[0], "'%s' is not an instance of a generator",
                                ARGON_RAW_STRING(((Function *) ret)->qname));
                    break;
                }

                if ((ret = IteratorGet(ret, false)) == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(LDLC) {
                PUSH(IncRef(cu_frame->locals[I16Arg(cu_frame->instr_ptr)]));
                DISPATCH2();
            }
            TARGET_OP(LDMETH) {
                auto index = (ArSSize) I32Arg(cu_frame->instr_ptr);
                auto *instance = TOP();

                ArObject *key;
                if ((key = TupleGet(cu_code->statics, index)) == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[3], index, cu_code->statics->length);

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

                DISPATCH4();
            }
            TARGET_OP(LDSCOPE) {
                auto index = (ArSSize) I32Arg(cu_frame->instr_ptr);
                auto *key = TupleGet(cu_code->statics, index);
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], index, cu_code->statics->length);

                    break;
                }

                ret = AttributeLoad(TOP(), key, true);
                Release(key);

                if (ret == nullptr)
                    break;

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(LOR) {
                BINARY_OP(l_or, |);
            }
            TARGET_OP(LSTATIC) {
                PUSH(TupleGet(cu_code->statics, I32Arg(cu_frame->instr_ptr)));
                DISPATCH4();
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
                DISPATCH1();
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
                    cu_frame->eval_stack -= idx;

                    Release(dict);

                    break;
                }

                cu_frame->eval_stack -= args;
                PUSH((ArObject *) dict);
                DISPATCH4();
            }
            TARGET_OP(MKFN) {
                auto flags = I32Flag<FunctionFlags>(cu_frame->instr_ptr);
                TypeInfo *base = nullptr;

                if (ENUMBITMASK_ISTRUE(flags, FunctionFlags::METHOD) ||
                    ENUMBITMASK_ISTRUE(flags, FunctionFlags::STATIC))
                    base = (TypeInfo *) PEEK3();

                ret = (ArObject *) FunctionNew((Code *) TOP(), base, cu_frame->globals, (Tuple *) PEEK1(),
                                               (List *) PEEK2(), I16Arg(cu_frame->instr_ptr), flags);
                if (ret == nullptr)
                    break;

                POP(); // defargs
                POP(); // enclosed
                TOP_REPLACE(ret);
                DISPATCH4();
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
                DISPATCH4();
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
                    cu_frame->eval_stack -= idx;

                    Release(ret);
                    break;
                }

                cu_frame->eval_stack -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MKSTRUCT) {
                auto trait_count = I32Arg(cu_frame->instr_ptr);
                auto *stack_base = cu_frame->eval_stack - trait_count;

                ret = StructTypeNew((String *) *(stack_base - 3),
                                    (String *) *(stack_base - 2),
                                    (String *) *(stack_base - 1),
                                    nullptr,
                                    (TypeInfo **) stack_base,
                                    trait_count);

                if (ret == nullptr)
                    break;

                STACK_REWIND(trait_count);

                POP(); // doc
                POP(); // qname

                TOP_REPLACE(ret);
                DISPATCH4();
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
                DISPATCH4();
            }
            TARGET_OP(MKTRAIT) {
                auto trait_count = I32Arg(cu_frame->instr_ptr);
                auto *stack_base = cu_frame->eval_stack - trait_count;

                ret = TraitNew((const char *) ARGON_RAW_STRING((String *) *(stack_base - 3)),
                               (const char *) ARGON_RAW_STRING((String *) *(stack_base - 2)),
                               (const char *) ARGON_RAW_STRING((String *) *(stack_base - 1)),
                               nullptr,
                               (TypeInfo **) stack_base,
                               trait_count);

                if (ret == nullptr)
                    break;

                STACK_REWIND(trait_count);

                POP(); // doc
                POP(); // qname

                TOP_REPLACE(ret);
                DISPATCH4();
            }
            TARGET_OP(MOD) {
                BINARY_OP(mod, %);
            }
            TARGET_OP(MTH) {
                auto len = I16Arg(cu_frame->instr_ptr);

                ret = *(cu_frame->eval_stack - len - 1);

                while (len > 0) {
                    *(cu_frame->eval_stack - (len + 1)) = *(cu_frame->eval_stack - len);
                    len--;
                }

                *(cu_frame->eval_stack - 1) = ret;

                DISPATCH2();
            }
            TARGET_OP(MUL) {
                BINARY_OP(mul, *);
            }
            TARGET_OP(NEG) {
                UNARY_OP(neg, -);
            }
            TARGET_OP(NGV) {
                ret = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));

                if (!NamespaceNewSymbol(cu_frame->globals, ret, TOP(), (I32Flag<AttributeFlag>(cu_frame->instr_ptr))))
                    break;

                Release(ret);
                POP();
                DISPATCH4();
            }
            TARGET_OP(NOT) {
                ret = BoolToArBool(!IsTrue(TOP()));

                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(NXT) {
                ret = TOP();

                if (!AR_TYPEOF(ret, type_function_)) {
                    ret = IteratorNext(ret);

                    if (IsPanickingFrame())
                        break;

                    PUSH(ret);
                    DISPATCH1();
                }

                auto *g_frame = (Frame *) ((Function *) ret)->LockAndGetStatus(fiber);
                if (g_frame == nullptr) {
                    if (GetFiberStatus() != FiberStatus::RUNNING)
                        return nullptr;

                    break;
                }

                PUSH(IncRef(ret));

                cu_frame->instr_ptr++;

                cu_frame = g_frame;
                cu_code = g_frame->code;

                FiberPushFrame(fiber, g_frame);
                continue;
            }
            TARGET_OP(PANIC) {
                Panic(TOP());
                POP();

                break;
            }
            TARGET_OP(PLT) {
                ret = PEEK1();

                if (!AR_TYPEOF(ret, type_list_)) {
                    ErrorFormat(kRuntimeError[0], "unexpected type in evaluation stack during PLT execution");
                    break;
                }

                if (!ListAppend((List *) ret, TOP()))
                    break;

                POP();
                DISPATCH1();
            }
            TARGET_OP(POP) {
                POP();
                DISPATCH1();
            }
            TARGET_OP(POPC) {
                ret = TOP();

                if (!AR_TYPEOF(ret, type_chan_)) {
                    ErrorFormat(kTypeError[0], kTypeError[2], type_chan_->name, AR_TYPE_QNAME(ret));
                    break;
                }

                if (!ChanRead((Chan *) ret, &ret)) {
                    if (GetFiberStatus() != FiberStatus::RUNNING)
                        return nullptr;

                    break;
                }

                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(POPGT) {
                auto arg = I16Arg(cu_frame->instr_ptr);

                while (cu_frame->eval_stack - cu_frame->extra > arg)
                    POP();

                DISPATCH2();
            }
            TARGET_OP(POS) {
                UNARY_OP(pos, +);
            }
            TARGET_OP(PSHC) {
                ret = TOP();

                if (!AR_TYPEOF(ret, type_chan_)) {
                    ErrorFormat(kTypeError[0], kTypeError[2], type_chan_->name, AR_TYPE_QNAME(ret));
                    break;
                }

                if (!ChanWrite((Chan *) ret, PEEK1())) {
                    if (GetFiberStatus() != FiberStatus::RUNNING)
                        return nullptr;

                    break;
                }

                // POP only Chan, leave value on stack!
                POP();
                DISPATCH1();
            }
            TARGET_OP(PSHN) {
                PUSH(nullptr);
                DISPATCH1();
            }
            TARGET_OP(RET) {
                cu_frame->return_value = TOP();

                cu_frame->eval_stack--;

                cu_frame->instr_ptr++;
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

                DISPATCH4();
            }
            TARGET_OP(ST) {
                cu_frame->trap_ptr = JUMPADDR(I32Arg(cu_frame->instr_ptr));

                cu_frame->panic_baseline = (void *) fiber->panic;

                DISPATCH4();
            }
            TARGET_OP(STATTR) {
                auto index = (ArSSize) I32Arg(cu_frame->instr_ptr);
                auto *key = TupleGet(cu_code->statics, index);
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], index, cu_code->statics->length);

                    break;
                }

                if (!AttributeSet(PEEK1(), key, TOP(), false)) {
                    Release(key);
                    break;
                }

                Release(key);

                POP(); // Instance
                POP(); // Value
                DISPATCH4();
            }
            TARGET_OP(STENC) {
                ListInsert(cu_frame->enclosed, TOP(), I16Arg(cu_frame->instr_ptr));

                POP();
                DISPATCH2();
            }
            TARGET_OP(STGBL) {
                AttributeProperty aprop{};

                ret = TupleGet(cu_code->names, I16Arg(cu_frame->instr_ptr));

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
                DISPATCH4();
            }
            TARGET_OP(STLC) {
                auto idx = I16Arg(cu_frame->instr_ptr);

                Release(cu_frame->locals[idx]);
                cu_frame->locals[idx] = TOP();
                cu_frame->eval_stack--;

                DISPATCH2();
            }
            TARGET_OP(STSCOPE) {
                auto index = (ArSSize) I32Arg(cu_frame->instr_ptr);
                auto *key = TupleGet(cu_code->statics, index);
                if (key == nullptr) {
                    DiscardLastPanic();

                    ErrorFormat(kRuntimeError[0], kRuntimeError[2], index, cu_code->statics->length);

                    break;
                }

                if (!AttributeSet(PEEK1(), key, TOP(), true)) {
                    Release(key);
                    break;
                }

                Release(key);

                POP(); // Instance
                POP(); // Value
                DISPATCH4();
            }
            TARGET_OP(STSUBSCR) {
                if (!STSubscribe(PEEK2(), PEEK1(), TOP()))
                    break;

                STACK_REWIND(3);
                DISPATCH1();
            }
            TARGET_OP(SUB) {
                BINARY_OP(sub, -);
            }
            TARGET_OP(SUBSCR) {
                if ((ret = Subscribe(PEEK1(), TOP())) == nullptr)
                    break;

                POP();
                TOP_REPLACE(ret);
                DISPATCH1();
            }
            TARGET_OP(SYNC) {
                ret = TOP();

                auto err = MonitorAcquire(ret);
                if (err < 0)
                    break;

                if (err == 0)
                    return nullptr;

                *cu_frame->sync_keys++ = IncRef(ret);

                POP();
                DISPATCH1();
            }
            TARGET_OP(TEST) {
                if (Equal(PEEK1(), TOP())) {
                    POP();
                    TOP_REPLACE(BoolToArBool(true));
                    DISPATCH();
                }

                TOP_REPLACE(BoolToArBool(false));
                DISPATCH1();
            }
            TARGET_OP(TRAP) {
                auto handler = I32Arg(cu_frame->instr_ptr);
                ArObject *tmp = TrapPanic(fiber, cu_frame);

                cu_frame->trap_ptr = handler > 0 ? JUMPADDR(handler) : nullptr;

                if (handler == 0)
                    cu_frame->panic_baseline = nullptr;

                ret = (ArObject *) (tmp == nullptr ? ResultNew(TOP(), true)
                                                   : ResultNew(tmp, false));

                Release(tmp);

                if (ret == nullptr)
                    break;

                if (cu_frame->eval_stack - cu_frame->extra > 0)
                    TOP_REPLACE(ret);
                else
                    PUSH(ret);

                DISPATCH4();
            }
            TARGET_OP(TSTORE) {
                auto *base = (TypeInfo *) PEEK2();

                if (AR_GET_TYPE(base) != type_type_) {
                    ErrorFormat(kRuntimeError[0], "expected type in evaluation stack during TSTORE execution");
                    break;
                }

                if (!NamespaceNewSymbol((Namespace *) base->tp_map, TOP(), PEEK1(),
                                        (AttributeFlag) I16Arg(cu_frame->instr_ptr)))
                    break;

                POP();
                POP();
                DISPATCH2();
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
                DISPATCH2();
            }
            TARGET_OP(UNSYNC) {
                MonitorRelease(*(cu_frame->sync_keys - 1));

                cu_frame->sync_keys--;
                Release(cu_frame->sync_keys);

                DISPATCH1();
            }
            TARGET_OP(YLD) {
                ret = TOP();

                cu_frame->eval_stack--;

                cu_frame->instr_ptr++;

                FiberPopFrame(fiber);

                cu_frame = fiber->frame;
                cu_code = cu_frame->code;

                cu_frame->counter--;

                ((Function *) TOP())->Unlock(fiber);

                Replace(cu_frame->eval_stack - 1, ret);

                continue;
            }
            default:
                ErrorFormat(kRuntimeError[0], "unknown opcode: 0x%X", *cu_frame->instr_ptr);
        }

        if (IsPanickingFrame() && (uintptr_t) cu_frame->trap_ptr > 0) {
            cu_frame->instr_ptr = cu_frame->trap_ptr;
            continue;
        }

        if (!PopExecutedFrame(fiber, &cu_code, &cu_frame, &ret))
            break;
    }

    return ret;
}

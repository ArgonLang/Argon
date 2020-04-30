// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cassert>

#include "argon_vm.h"
#include <lang/opcodes.h>
#include <object/bool.h>
#include <object/error.h>
#include <object/map.h>
#include <object/function.h>
#include <object/nil.h>
#include <object/bounds.h>

using namespace lang;
using namespace argon::object;
using namespace argon::vm;

#ifndef COMPUTED_GOTO
#define TARGET_OP(op) case OpCodes::op:

#define DISPATCH()                  \
frame->instr_ptr+=sizeof(Instr8);   \
continue

#define DISPATCH2()                 \
frame->instr_ptr+=sizeof(Instr16);  \
continue

#define DISPATCH4()                 \
frame->instr_ptr+=sizeof(Instr32);  \
continue

#define JUMPTO(offset)                                      \
frame->instr_ptr = (unsigned char *) code->instr + offset;  \
continue

#else
#define TARGET_OP(op)    \
    case OpCodes::op:   \
    LBL_##op:

#define DISPATCH goto *computed_goto[(unsigned char)*(lang::OpCodes *) frame->instr_ptr]
#endif


// STACK MANIPULATION MACRO
#define PUSH(obj)   frame->eval_stack[es_cur++] = obj
#define POP()       Release(frame->eval_stack[--es_cur])

#define TOP()       frame->eval_stack[es_cur-1]
#define PEEK1()     frame->eval_stack[es_cur-2]

#define TOP_REPLACE(obj)                \
Release(frame->eval_stack[es_cur-1]);   \
frame->eval_stack[es_cur-1]=obj

#define GET_BINARY_OP(struct, offset) *((BinaryOp *) (((unsigned char *) struct) + offset))

ArObject *Binary(ArObject *l, ArObject *r, int offset) {
    BinaryOp lop = GET_BINARY_OP(l->type->ops, offset);
    BinaryOp rop = GET_BINARY_OP(r->type->ops, offset);

    ArObject *result = nullptr;

    if (lop != nullptr)
        result = lop(l, r);
    if (rop != nullptr && result == NotImpl) {
        Release(result);
        result = rop(l, r);
    }

    if (result == NotImpl) {
        // TODO: SET ROUTINE ERROR
        result = nullptr;
    }

    return result;
}

#define BINARY_OP(op)                                                   \
if ((ret = Binary(PEEK1(), TOP(), offsetof(OpSlots, op))) == nullptr)   \
    goto error;                                                         \
POP();                                                                  \
TOP_REPLACE(ret);                                                       \
DISPATCH()

void ArgonVM::Eval(ArRoutine *routine) {
    begin:
    Frame *frame = routine->frame;
    object::Code *code = frame->code;
    unsigned int es_cur =
            (((unsigned char *) frame->eval_stack) - (unsigned char *) frame->stack_extra_base) / sizeof(ArObject *);
    frame->eval_stack = (ArObject **) frame->stack_extra_base;

    ArObject *ret = nullptr;

    while (frame->instr_ptr < (code->instr + code->instr_sz)) {
        switch (*(lang::OpCodes *) frame->instr_ptr) {

            TARGET_OP(CALL) {
                // TODO: check if callable!
                auto largs = I16Arg(frame->instr_ptr);
                auto args = largs;
                auto func = (Function *) frame->eval_stack[es_cur - args - 1];

                if (func->currying != nullptr)
                    args += func->currying->len;

                if (args > func->arity) {
                    if (!func->variadic)
                        goto error; // TODO: Too much arguments!

                    auto plus = args - func->arity;

                    if ((ret = ListNew(plus)) == nullptr)
                        goto error;

                    // +1 is the rest element itself
                    for (unsigned int i = es_cur - (plus + 1); i < es_cur; i++) {
                        if (!ListAppend((List *) ret, frame->eval_stack[i])) {
                            Release(ret);
                            goto error;
                        }
                        Release(frame->eval_stack[i]);
                    }
                    es_cur -= plus;
                    largs -= plus;
                    TOP_REPLACE(ret);
                } else if (args < func->arity) {
                    if (args == 0) {
                        // TODO: MISSING PARAMS!
                        goto error;
                    }
                    // Missing argument/s -> Currying
                    if (func->arity - args > 1 || !func->variadic) {
                        Function *new_fn;
                        if ((new_fn = FunctionNew(func, args)) == nullptr)
                            goto error;

                        for (unsigned int i = es_cur - I16Arg(frame->instr_ptr); i < es_cur; i++) {
                            if (!ListAppend((List *) new_fn->currying, frame->eval_stack[i])) {
                                Release(new_fn);
                                goto error;
                            }
                            Release(frame->eval_stack[i]);
                        }
                        es_cur -= I16Arg(frame->instr_ptr);
                        TOP_REPLACE(new_fn);
                        DISPATCH2();
                    }
                }

                //****+ TODO: TEMPORARY, REMOVED AT THE END OF TEST *********
                auto fr = FrameNew(func->code);
                if (fr == nullptr) {
                    assert(false);
                    goto error;
                }
                int pos = 0;
                // fill locals!
                if (func->currying != nullptr) {
                    for (pos = 0; pos < func->currying->len; pos++) {
                        fr->locals[pos] = func->currying->objects[pos];
                        IncRef(fr->locals[pos]);
                    }
                }
                for (unsigned int i = es_cur - largs; i < es_cur; i++)
                    fr->locals[pos++] = frame->eval_stack[i];
                es_cur -= largs;

                if (func->enclosed != nullptr) {
                    for (size_t i = 0; i < func->enclosed->len; i++) {
                        IncRef(func->enclosed->objects[i]);
                        fr->enclosed[i] = func->enclosed->objects[i];
                    }
                }

                assert(TOP()->type == &type_function_);
                POP(); // pop function!

                // Invoke function!
                // save PC
                frame->instr_ptr += sizeof(Instr16);
                frame->eval_stack = &frame->eval_stack[es_cur];
                fr->globals = frame->globals;
                fr->back = frame;
                routine->frame = fr;
                goto begin;
            }

            TARGET_OP(CMP) {
                auto mode = (CompareMode) I16Arg(frame->instr_ptr);
                ArObject *left = PEEK1();
                if (mode == CompareMode::EQ)
                    ret = BoolToArBool(left->type->equal(left, TOP()));
                else if (mode == CompareMode::NE)
                    ret = BoolToArBool(!left->type->equal(left, TOP()));
                else {
                    if (left->type->compare != nullptr)
                        ret = left->type->compare(left, TOP(), mode);
                    else
                        assert(false); // TODO: not impl error!
                }
                POP();
                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(TEST) {
                ArObject *left = PEEK1();
                if (left->type->equal(left, TOP())) {
                    POP();
                    TOP_REPLACE(BoolToArBool(true));
                    DISPATCH();
                }
                TOP_REPLACE(BoolToArBool(false));
                DISPATCH();
            }
            TARGET_OP(NOT) {
                ret = True;
                if (IsTrue(TOP()))
                    ret = False;
                IncRef(ret);
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** JUMP ***

            TARGET_OP(JF) {
                if (!IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JFOP) {
                if (IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(JMP) {
                JUMPTO(I32Arg(frame->instr_ptr));
            }
            TARGET_OP(JT) {
                if (IsTrue(TOP())) {
                    POP();
                    JUMPTO(I32Arg(frame->instr_ptr));
                }
                POP();
                DISPATCH4();
            }
            TARGET_OP(JTOP) {
                if (!IsTrue(TOP())) {
                    POP();
                    DISPATCH4();
                }
                JUMPTO(I32Arg(frame->instr_ptr));
            }

                // *** NEW VARIABLES ***

            TARGET_OP(NGV) {
                assert(code->names != nullptr);
                ret = TupleGetItem(code->names, I16Arg(frame->instr_ptr));

                assert(!MapContains((Map *) frame->globals, ret)); // Double declaration, compiler error!
                if (!MapInsert((Map *) frame->globals, ret, TOP())) {
                    Release(ret);
                    goto error; // todo: memory error!
                }

                Release(ret);
                POP();
                DISPATCH2();
            }
            TARGET_OP(NLV) {
                IncRef(TOP());
                frame->locals[I16Arg(frame->instr_ptr)] = TOP();
                POP();
                DISPATCH2();
            }

                // *** STORE VALUES ***

            TARGET_OP(STGBL) {
                assert(code->names != nullptr);
                ret = TupleGetItem(code->names, I16Arg(frame->instr_ptr));

                if (!MapContains((Map *) frame->globals, ret)) {
                    Release(ret);
                    goto error; // todo: Unknown variable
                }

                MapInsert((Map *) frame->globals, ret, TOP());
                Release(ret);
                POP();
                DISPATCH2();
            }
            TARGET_OP(STLC) {
                assert(frame->locals != nullptr);
                Release(frame->locals[I16Arg(frame->instr_ptr)]);
                frame->locals[I16Arg(frame->instr_ptr)] = TOP();
                --es_cur; // back eval pointer without release object!
                DISPATCH2();
            }

                // *** LOAD VARIABLES ***

            TARGET_OP(LDENC) {
                assert(frame->enclosed != nullptr);
                PUSH(frame->enclosed[I16Arg(frame->instr_ptr)]);
                DISPATCH2();
            }
            TARGET_OP(LDGBL) {
                assert(code->names != nullptr);
                ArObject *key = TupleGetItem(code->names, I16Arg(frame->instr_ptr));

                ret = MapGet((Map *) frame->globals, key);
                Release(key);

                if (ret == nullptr)
                    goto error; // todo: Unknown variable

                PUSH(ret);
                DISPATCH2();
            }
            TARGET_OP(LDLC) {
                assert(frame->locals != nullptr);
                PUSH(frame->locals[I16Arg(frame->instr_ptr)]);
                DISPATCH2();
            }
            TARGET_OP(LSTATIC) {
                PUSH(TupleGetItem(code->statics, I16Arg(frame->instr_ptr)));
                DISPATCH2();
            }
            TARGET_OP(SUBSCR) {
                if (IsMap(PEEK1())) {
                    // todo: check if key is hasbale!
                    if ((ret = PEEK1()->type->map_actions->get_item(PEEK1(), TOP())) == nullptr)
                        goto error; // todo key not exists
                } else if (IsSequence(PEEK1())) {
                    if (AsIndex(TOP())) {
                        ret = PEEK1()->type->sequence_actions->get_item(PEEK1(),
                                                                        TOP()->type->number_actions->as_index(TOP()));
                        if (ret == nullptr)
                            goto error; // todo outofbounderror
                    } else if (TOP()->type == &type_bounds_)
                        ret = PEEK1()->type->sequence_actions->get_slice(PEEK1(), TOP());
                    else
                        goto error; // todo: key must be index or bounds!
                } else
                    goto error; // todo: object not subscriptable
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }

                // *** COMMON MATH OPERATIONS ***

            TARGET_OP(ADD) {
                BINARY_OP(add);
            }
            TARGET_OP(SUB) {
                BINARY_OP(sub);
            }
            TARGET_OP(MUL) {
                BINARY_OP(mul);
            }
            TARGET_OP(DIV) {
                BINARY_OP(div);
            }
            TARGET_OP(IDIV) {
                BINARY_OP(idiv);
            }
            TARGET_OP(MOD) {
                BINARY_OP(module);
            }
            TARGET_OP(POS) {
                BINARY_OP(pos);
            }
            TARGET_OP(NEG) {
                BINARY_OP(neg);
            }

                // *** COMMON LOGICAL OPERATIONS ***

            TARGET_OP(LAND) {
                BINARY_OP(l_and);
            }
            TARGET_OP(LOR) {
                BINARY_OP(l_or);
            }
            TARGET_OP(LXOR) {
                BINARY_OP(l_xor);
            }
            TARGET_OP(SHL) {
                BINARY_OP(shl);
            }
            TARGET_OP(SHR) {
                BINARY_OP(shr);
            }
            TARGET_OP(INV) {
                BINARY_OP(invert);
            }

                // *** COMPOUND DATA STRUCTURES ***

            TARGET_OP(MK_BOUNDS) {
                unsigned short args = I16Arg(frame->instr_ptr);
                arsize step = 1;
                arsize stop = 0;
                arsize start;

                if (args == 3) {
                    if (!AsIndex(TOP())) {
                        goto error; // todo: MUST BE A NUMBER!
                    }
                    step = TOP()->type->number_actions->as_index(TOP());
                    POP();
                }

                if (args >= 2) {
                    if (!AsIndex(TOP())) {
                        goto error; // todo: MUST BE A NUMBER!
                    }
                    stop = TOP()->type->number_actions->as_index(TOP());
                    POP();
                }

                if (!AsIndex(TOP())) {
                    goto error; // todo: MUST BE A NUMBER!
                }

                start = TOP()->type->number_actions->as_index(TOP());

                if ((ret = BoundsNew(start, stop, step)) == nullptr)
                    goto error; // todo: memoryerror

                TOP_REPLACE(ret);
                DISPATCH2();
            }
            TARGET_OP(MK_CLOSURE) {
                if ((ret = FunctionNew((Function *) PEEK1(), (List *) TOP())) == nullptr)
                    goto error; // TODO: nomem
                POP();
                TOP_REPLACE(ret);
                DISPATCH();
            }
            TARGET_OP(MK_LIST) {
                unsigned int args = I32Arg(frame->instr_ptr);

                if ((ret = ListNew()) == nullptr)
                    goto error; // todo: memroyerror

                for (unsigned int i = es_cur - args; i < es_cur; i++) {
                    if (!ListAppend((List *) ret, frame->eval_stack[i]))
                        goto error; // TODO: memoryerror
                    Release(frame->eval_stack[i]);
                }

                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_TUPLE) {
                unsigned int args = I32Arg(frame->instr_ptr);

                if ((ret = TupleNew(args)) == nullptr)
                    goto error; // todo: memoryerror

                for (unsigned int i = es_cur - args, idx = 0; i < es_cur; i++) {
                    if (!TupleInsertAt((Tuple *) ret, idx++, frame->eval_stack[i]))
                        goto error; // TODO: out of range!
                    Release(frame->eval_stack[i]);
                }

                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }
            TARGET_OP(MK_MAP) {
                unsigned int args = I32Arg(frame->instr_ptr) * 2;

                if ((ret = MapNew()) == nullptr)
                    goto error; // todo: memoryerrpr

                for (unsigned int i = es_cur - args; i < es_cur; i += 2) {
                    if (!MapInsert((Map *) ret, frame->eval_stack[i], frame->eval_stack[i + 1])) {
                        Release(ret);
                        goto error; // todo: memoryerror or unhasbale key!!
                    }
                    Release(frame->eval_stack[i]);
                    Release(frame->eval_stack[i + 1]);
                }
                es_cur -= args;
                PUSH(ret);
                DISPATCH4();
            }

            TARGET_OP(RET) {
                assert(frame->back != nullptr);
                if (es_cur == 0) {
                    IncRef(NilVal);
                    frame->back->eval_stack[0] = NilVal;
                } else {
                    frame->back->eval_stack[0] = TOP();
                    es_cur--;
                }
                frame->back->eval_stack++;
                goto end_while;
            }
            TARGET_OP(POP) {
                POP();
                DISPATCH();
            }
            default:
                assert(false);
        }
        error:
        assert(false);
    }
    end_while:
    assert(es_cur == 0);

    if (frame->back != nullptr) {
        routine->frame = frame->back;
        FrameDel(frame);
        goto begin;
    }
}


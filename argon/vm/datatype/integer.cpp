// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <cmath>

#include <argon/vm/runtime.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/bytes.h>
#include <argon/vm/datatype/error.h>

#include <argon/vm/datatype/integer.h>

using namespace argon::vm::datatype;

#define CHECK_INTEGER(left, right)                                          \
    if (!(AR_TYPEOF(left, type_int_) || AR_TYPEOF(left, type_uint_)) ||     \
        !(AR_TYPEOF(right, type_int_) || AR_TYPEOF(right, type_uint_)))     \
        return nullptr

#define SIMPLE_OP(left, right, op)                                          \
    do {                                                                    \
        CHECK_INTEGER(left, right);                                         \
                                                                            \
        if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))    \
            return (ArObject *) UIntNew(left->uint op right->uint);         \
                                                                            \
        return (ArObject *) IntNew(left->sint op right->sint);              \
    } while(0)

ARGON_FUNCTION(number_frombytes, frombytes,
               "Return the number represented by the given array of bytes.\n"
               "\n"
               "- Parameter bytes: Array of bytes to convert.\n"
               "- KWParameters:"
               "  - byteorder: Byte order used to represent the integer (big | little).\n"
               "- Returns: Number.\n",
               "x: bytes", false, true) {
    ArBuffer buffer{};

    UIntegerUnderlying num = 0;

    if (!BufferGet(args[0], &buffer, BufferFlags::READ))
        return nullptr;

    if (buffer.length > sizeof(UIntegerUnderlying)) {
        ErrorFormat(kValueError[0], "bytes exceeds the maximum size that can be represented %d/%d",
                    buffer.length, sizeof(UIntegerUnderlying));

        BufferRelease(&buffer);

        return nullptr;
    }

    auto byteorder = DictLookupString((Dict *) kwargs, "byteorder", "big");
    if (byteorder == nullptr) {
        BufferRelease(&buffer);
        return nullptr;
    }

    auto big_endian = StringEqual(byteorder, "big");

    if (!big_endian && !StringEqual(byteorder, "little")) {
        ErrorFormat(kValueError[0], "byteorder must be 'big' or 'little', got: '%s'", ARGON_RAW_STRING(byteorder));

        Release(byteorder);
        BufferRelease(&buffer);

        return nullptr;
    }

    Release(byteorder);

    const unsigned char *bstart = buffer.buffer;
    int inc = 1;

    if (big_endian) {
        bstart = buffer.buffer + buffer.length - 1;
        inc = -1;
    }

    for (int i = 0; i < buffer.length; i++) {
        num |= ((UIntegerUnderlying) *bstart) << i * 8u;
        bstart += inc;
    }

    BufferRelease(&buffer);

    const auto *self_type = (const TypeInfo *) ((Function *) _func)->base;
    if (self_type == type_int_)
        return (ArObject *) IntNew((IntegerUnderlying) num);

    return (ArObject *) UIntNew(num);
}

ARGON_FUNCTION(number_parse, parse,
               "Convert a string or number to number, if possible.\n"
               "\n"
               "- Parameters:\n"
               "  - obj: Obj to convert.\n"
               "  - base: Base to be used while parsing `obj`.\n"
               "- Returns: Number.\n",
               "sx: obj, i: base", false, false) {
    ArBuffer buffer{};

    const auto *self_type = (const TypeInfo *) ((Function *) _func)->base;
    auto base = (int) ((Integer *) args[1])->sint;

    if (!BufferGet(*args, &buffer, argon::vm::datatype::BufferFlags::READ))
        return nullptr;

    if (buffer.length == 0) {
        BufferRelease(&buffer);

        ErrorFormat(kValueError[0], "empty value cannot be converted to %s",
                    self_type == type_int_ ? type_int_->name : type_uint_->name);

        return nullptr;
    }

    if (self_type == type_int_) {
        auto *result = IntNew((const char *) buffer.buffer, base);

        BufferRelease(&buffer);

        return (ArObject *) result;
    }

    auto *result = UIntNew((const char *) buffer.buffer, base);

    BufferRelease(&buffer);

    return (ArObject *) result;
}

ARGON_METHOD(number_bits, bits,
             "Return number of bits necessary to represent an integer in binary.\n"
             "\n"
             "- Returns: Number of bits.\n",
             nullptr, false, false) {
    if (AR_TYPEOF(_self, type_int_))
        return (ArObject *) IntNew(IntegerCountBits(((Integer *) _self)->sint));

    return (ArObject *) UIntNew(IntegerCountBits(((Integer *) _self)->uint));
}

ARGON_METHOD(number_digits, digits,
             "Return number of digits necessary to represent an integer in the given numeric base.\n"
             "\n"
             "- Parameter base: Numeric base (2, 8, 10, 16).\n"
             "- Returns: Number of digits.\n",
             "iu: base", false, false) {
    const auto *r_base = (Integer *) args[0];
    int base;

    if (AR_TYPEOF(r_base, type_int_) && r_base->sint < 0) {
        ErrorFormat(kValueError[0], "numeric base cannot be negative");
        return nullptr;
    } else if (r_base->uint > 26) {
        ErrorFormat(kValueError[0], "numeric base cannot be greater than 26");
        return nullptr;
    }

    base = (int) r_base->uint;

    if (AR_TYPEOF(_self, type_int_))
        return (ArObject *) IntNew(IntegerCountDigits(((Integer *) _self)->sint, base));

    return (ArObject *) UIntNew(IntegerCountDigits(((Integer *) _self)->uint, base));
}

ARGON_METHOD(number_tobytes, tobytes,
             "Return an array of bytes representing the number.\n"
             "\n"
             "- KWParameters:"
             "  - byteorder: Byte order used to represent the integer (big | little).\n"
             "- Returns: Bytes object.\n",
             nullptr, false, true) {
    auto byteorder = DictLookupString((Dict *) kwargs, "byteorder", "big");
    if (byteorder == nullptr)
        return nullptr;

    auto big_endian = StringEqual(byteorder, "big");

    if (!big_endian && !StringEqual(byteorder, "little")) {
        ErrorFormat(kValueError[0], "byteorder must be 'big' or 'little', got: '%s'", ARGON_RAW_STRING(byteorder));

        Release(byteorder);

        return nullptr;
    }

    Release(byteorder);

    // Calculate bytes array length
    int blength;

    if (AR_TYPEOF(_self, type_int_))
        blength = IntegerCountBits(((Integer *) _self)->sint);
    else
        blength = IntegerCountBits(((Integer *) _self)->uint);

    blength = (blength / 8) + 1;

    auto *buffer = (unsigned char *) argon::vm::memory::Alloc(blength);
    if (buffer == nullptr)
        return nullptr;

    unsigned char *bstart = buffer;
    int inc = 1;

    if (big_endian) {
        bstart = buffer + blength - 1;
        inc = -1;
    }

    for (int i = 0; i < blength; i++) {
        *bstart = (((Integer *) _self)->uint >> i * 8u) & 0xFF;
        bstart += inc;
    }

    auto *ret = BytesNewHoldBuffer(buffer, blength, blength, true);
    if (ret == nullptr)
        argon::vm::memory::Free(buffer);

    return (ArObject *) ret;
}

const FunctionDef number_methods[] = {
        number_parse,
        number_frombytes,

        number_bits,
        number_digits,
        number_tobytes,
        ARGON_METHOD_SENTINEL
};

ArObject *number_blength(const Integer *self) {
    return (ArObject *) IntNew(sizeof(UIntegerUnderlying));
}

const MemberDef number_members[] = {
        ARGON_MEMBER_GETSET("bytes_length", (UnaryConstOp) number_blength, nullptr),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots number_objslot = {
        number_methods,
        number_members,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *integer_compare(const Integer *self, const Integer *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other))
        return nullptr;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    auto left = self->sint;
    auto right = other->sint;

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArSize number_hash(const Integer *self) {
    return self->uint;
}

ArObject *integer_add(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, +);
}

ArObject *integer_sub(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, -);
}

ArObject *integer_mul(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, *);
}

ArObject *integer_div(const Integer *left, const Integer *right) {
    DecimalUnderlying lvalue;
    DecimalUnderlying rvalue;

    int l_size;
    int r_size;
    int lexp;
    int rexp;

    CHECK_INTEGER(left, right);

    if (AR_TYPEOF(left, type_int_))
        l_size = IntegerCountBits(left->sint);
    else
        l_size = IntegerCountBits(left->uint);

    if (AR_TYPEOF(right, type_int_))
        r_size = IntegerCountBits(right->sint);
    else
        r_size = IntegerCountBits(right->uint);

    if (r_size == 0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if (l_size <= LDBL_MANT_DIG && r_size <= LDBL_MANT_DIG) {
        if (AR_TYPEOF(left, type_int_))
            lvalue = (DecimalUnderlying) left->sint;
        else
            lvalue = (DecimalUnderlying) left->uint;

        if (AR_TYPEOF(right, type_int_))
            rvalue = (DecimalUnderlying) right->sint;
        else
            rvalue = (DecimalUnderlying) right->uint;

        return (ArObject *) DecimalNew(lvalue / rvalue);
    }

    if (AR_TYPEOF(left, type_int_))
        lvalue = Integer2ScaledDouble(left->sint, l_size, &lexp);
    else
        lvalue = Integer2ScaledDouble(left->uint, l_size, &lexp);

    if (AR_TYPEOF(right, type_int_))
        rvalue = Integer2ScaledDouble(right->sint, r_size, &rexp);
    else
        rvalue = Integer2ScaledDouble(right->uint, r_size, &rexp);

    auto ans_exp = lexp - rexp;
    if (ans_exp > LDBL_MAX_EXP) {
        ErrorFormat(kOverflowError[0], "integer division result too large for a %s", type_decimal_->qname);
        return nullptr;
    } else if (ans_exp < LDBL_MIN_EXP - LDBL_MANT_DIG - 1) {
        // Underflow
        auto negate = (lvalue < 0) ^ (rvalue < 0);
        return (ArObject *) DecimalNew(negate ? -0.0 : 0.0);
    }

    lvalue /= rvalue;

    lvalue = std::ldexp(lvalue, ans_exp);

    return (ArObject *) DecimalNew(lvalue);
}

ArObject *integer_idiv(const Integer *left, const Integer *right) {
    CHECK_INTEGER(left, right);

    if (right->uint == 0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))
        return (ArObject *) UIntNew(left->uint / right->uint);

    return (ArObject *) IntNew(left->sint / right->sint);
}

ArObject *integer_mod(const Integer *left, const Integer *right) {
    IntegerUnderlying ans;

    CHECK_INTEGER(left, right);

    if (right->uint == 0) {
        argon::vm::Panic((ArObject *) error_div_by_zero);
        return nullptr;
    }

    if (AR_TYPEOF(left, type_uint_) || AR_TYPEOF(right, type_uint_))
        return (ArObject *) UIntNew(left->uint % right->uint);

    ans = left->sint % right->sint;
    if (ans < 0)
        ans += right->sint;

    return (ArObject *) IntNew(ans);
}

ArObject *integer_pos(Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) IncRef(self);

    if (self->sint < 0)
        return (ArObject *) IntNew(-self->sint);

    return (ArObject *) IncRef(self);
}

ArObject *integer_neg(Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) IncRef(self);

    return (ArObject *) IntNew(-self->sint);
}

ArObject *integer_land(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, &);
}

ArObject *integer_lor(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, |);
}

ArObject *integer_lxor(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, ^);
}

ArObject *integer_shl(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, <<);
}

ArObject *integer_shr(const Integer *left, const Integer *right) {
    SIMPLE_OP(left, right, >>);
}

ArObject *integer_inv(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(~self->uint);

    return (ArObject *) IntNew(~self->sint);
}

ArObject *integer_inc(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(self->uint + 1);

    return (ArObject *) IntNew(self->sint + 1);
}

ArObject *integer_dec(const Integer *self) {
    if (AR_TYPEOF(self, type_uint_))
        return (ArObject *) UIntNew(self->uint - 1);

    return (ArObject *) IntNew(self->sint - 1);
}

const OpSlots integer_ops = {
        (BinaryOp) integer_add,
        (BinaryOp) integer_sub,
        (BinaryOp) integer_mul,
        (BinaryOp) integer_div,
        (BinaryOp) integer_idiv,
        (BinaryOp) integer_mod,
        (UnaryOp) integer_pos,
        (UnaryOp) integer_neg,
        (BinaryOp) integer_land,
        (BinaryOp) integer_lor,
        (BinaryOp) integer_lxor,
        (BinaryOp) integer_shl,
        (BinaryOp) integer_shr,
        (UnaryOp) integer_inv,
        (BinaryOp) integer_add,
        (BinaryOp) integer_sub,
        (UnaryOp) integer_inc,
        (UnaryOp) integer_dec
};

ArObject *integer_repr(const Integer *self) {
    return (ArObject *) StringFormat("%lld", self->sint);
}

bool integer_is_true(const Integer *self) {
    return self->sint > 0;
}

TypeInfo IntegerType = {
        AROBJ_HEAD_INIT_TYPE,
        "Int",
        nullptr,
        nullptr,
        sizeof(Integer),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) number_hash,
        (Bool_UnaryOp) integer_is_true,
        (CompareOp) integer_compare,
        (UnaryConstOp) integer_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &number_objslot,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_int_ = &IntegerType;

ArObject *uint_compare(const Integer *self, const Integer *other, CompareMode mode) {
    if (!AR_TYPEOF(other, type_int_) && !AR_TYPEOF(other, type_uint_))
        return nullptr;

    if (self == other && mode == CompareMode::EQ)
        return BoolToArBool(true);

    auto left = self->uint;
    auto right = other->uint;

    ARGON_RICH_COMPARE_CASES(left, right, mode);
}

ArObject *uinteger_repr(const Integer *self) {
    return (ArObject *) StringFormat("%lu", self->uint);
}

bool uinteger_is_true(const Integer *self) {
    return self->uint > 0;
}

TypeInfo UIntegerType = {
        AROBJ_HEAD_INIT_TYPE,
        "UInt",
        nullptr,
        nullptr,
        sizeof(Integer),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        (ArSize_UnaryOp) number_hash,
        (Bool_UnaryOp) uinteger_is_true,
        (CompareOp) uint_compare,
        (UnaryConstOp) uinteger_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &number_objslot,
        nullptr,
        &integer_ops,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_uint_ = &UIntegerType;

Integer *argon::vm::datatype::IntNew(IntegerUnderlying number) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr) {
        si->uint = 0;
        si->sint = number;
    }

    return si;
}

Integer *argon::vm::datatype::IntNew(const char *string, int base) {
    auto *si = MakeObject<Integer>(&IntegerType);

    if (si != nullptr) {
        si->uint = 0;
        si->sint = std::strtol(string, nullptr, base);
    }

    return si;
}

Integer *argon::vm::datatype::UIntNew(UIntegerUnderlying number) {
    auto *ui = MakeObject<Integer>(&UIntegerType);

    if (ui != nullptr)
        ui->uint = number;

    return ui;
}

Integer *argon::vm::datatype::UIntNew(const char *string, int base) {
    auto *ui = MakeObject<Integer>(&UIntegerType);

    if (ui != nullptr)
        ui->uint = std::strtoul(string, nullptr, base);

    return ui;
}

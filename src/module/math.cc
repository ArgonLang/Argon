// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/decimal.h>

#include "math.h"

using namespace argon::object;

#define CHECK_INTEGER(obj)              \
    if(!AR_TYPEOF(obj, type_integer_))  \
        return ErrorFormat(type_type_error_, "expected integer, found '%s'", AR_TYPE_NAME(obj))

#define CHECK_NUMBER(obj)                                                   \
    if(!AR_TYPEOF(obj, type_integer_) && !AR_TYPEOF(obj, type_decimal_))    \
        return ErrorFormat(type_type_error_, "expected number, found '%s'", AR_TYPE_NAME(obj))

#define CONVERT_DOUBLE(object, dbl)             \
    if(AR_TYPEOF(object, type_decimal_))        \
        (dbl) = ((Decimal*)(object))->decimal;  \
    else if(!AR_TYPEOF(object, type_integer_) || !DecimalCanConvertFromInt(((Integer*)object), &dbl)) \
        return nullptr

ARGON_FUNCTION5(math_, acos,
                "Return the arc cosine (measured in radians) of x."
                ""
                "- Parameter x: value whose arc cosine is computed, in the interval [-1,+1]."
                "- Returns: principal arc cosine of x, in the interval [0, pi] radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = acos(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, asin,
                "Return the arc sine (measured in radians) of x."
                ""
                "- Parameter x: value whose arc sine is computed, in the interval [-1,+1]."
                "- Returns: principal arc sine of x, in the interval [-pi/2,+pi/2] radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = asin(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, atan,
                "Return the arc tangent (measured in radians) of x."
                ""
                "- Parameter x: value whose arc tangent is computed."
                "- Returns: principal arc tangent of x, in the interval [-pi/2,+pi/2] radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = atan(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, ceil,
                "Rounds x upward, returning the smallest integral value that is not less than x."
                ""
                "- Parameter x: value to round up."
                "- Returns: the smallest integral value that is not less than x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = ceil(num);

    return IntegerNew(num);
}

ARGON_FUNCTION5(math_, cos,
                "Returns the cosine of an angle of x radians."
                ""
                "- Parameter x: value representing an angle expressed in radians."
                "- Returns: cosine of x radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = cos(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, cosh,
                "Returns the hyperbolic cosine of x."
                ""
                "- Parameter x: value representing a hyperbolic angle."
                "- Returns: hyperbolic cosine of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = cosh(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, exp,
                "Returns the base-e exponential function of x, which is e raised to the power x: e^x."
                ""
                "- Parameter x: value of the exponent."
                "- Returns: exponential value of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = exp(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, fabs,
                "Returns the absolute value of x: |x|."
                ""
                "- Parameter x: value whose absolute value is returned."
                "- Returns: the absolute value of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = fabs(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, floor,
                "Rounds x downward, returning the largest integral value that is not greater than x."
                ""
                "- Parameter x: value to round down."
                "- Returns: the value of x rounded downward.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = floor(num);

    return IntegerNew(num);
}

ARGON_FUNCTION5(math_, fmod,
                "Returns the floating-point remainder of numer/denom (rounded towards zero)."
                ""
                "- Parameters "
                "   - numer: value of the quotient numerator."
                "   - denom: value of the quotient denominator."
                "- Returns: the remainder of dividing the arguments.", 2, false) {
    DecimalUnderlying l;
    DecimalUnderlying r;

    CHECK_NUMBER(argv[0]);
    CHECK_NUMBER(argv[1]);

    CONVERT_DOUBLE(argv[0], l);
    CONVERT_DOUBLE(argv[1], r);

    l = fmod(l, r);

    return DecimalNew(l);
}

ARGON_FUNCTION5(math_, log,
                "Compute natural logarithm."
                ""
                "- Parameter x: value whose logarithm is calculated."
                "- Returns: natural logarithm of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = log(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, log10,
                "Compute base-10 logarithm."
                ""
                "- Parameter x: value whose logarithm is calculated."
                "- Returns: base-10 logarithm of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = log10(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, mpow,
                "Returns base raised to the power exponent modulo m: (base^exponent) % m."
                ""
                "- Parameters "
                "   - base: base value."
                "   - exponent: exponent value."
                "   - module: module value"
                "- Returns: the result of raising base to the power exponent module m.",
                3, false) {
    IntegerUnderlying x;
    IntegerUnderlying y;
    IntegerUnderlying m;
    IntegerUnderlying res = 1;

    CHECK_INTEGER(argv[0]);
    CHECK_INTEGER(argv[1]);
    CHECK_INTEGER(argv[2]);

    x = ((Integer *) argv[0])->integer;
    y = ((Integer *) argv[1])->integer;
    m = ((Integer *) argv[2])->integer;

    x = x % m;

    if (x == 0)
        return IntegerNew(0);

    while (y > 0) {
        if (y & 1)
            res = (res * x) % m;

        y = y >> 1;
        x = (x * x) % m;
    }

    return IntegerNew(res);
}

ARGON_FUNCTION5(math_, pow,
                "Returns base raised to the power exponent: base^exponent."
                ""
                "- Parameters "
                "   - base: base value."
                "   - exponent: exponent value."
                "- Returns: the result of raising base to the power exponent.",
                2, false) {
    DecimalUnderlying base;
    DecimalUnderlying exp;

    CHECK_NUMBER(argv[0]);
    CHECK_NUMBER(argv[1]);

    CONVERT_DOUBLE(argv[0], base);
    CONVERT_DOUBLE(argv[1], exp);

    base = pow(base, exp);

    return DecimalNew(base);
}

ARGON_FUNCTION5(math_, sin,
                "Returns the sine of an angle of x radians."
                ""
                "- Parameter x: value representing an angle expressed in radians."
                "- Returns: sine of x radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = sin(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, sinh,
                "Returns the hyperbolic sine of x."
                ""
                "- Parameter x: value representing a hyperbolic angle."
                "- Returns: hyperbolic sine of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = sinh(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, sqrt,
                "Returns the square root of x."
                ""
                "- Parameter x: value whose square root is computed."
                "- Returns: square root of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = sqrt(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, isqrt,
                "Returns the inverse square root of x (1/sqrt(x))."
                ""
                "The Quake III Arena Fast Inverse Square Root algorithm is used"
                "to calculate the inverse square root of x."
                ""
                "- Parameter x: value whose inverse square root is computed."
                "- Returns: inverse square root of x.",
                1, false) {
#if __WORDSIZE == 32
#define MAGIC_CONSTANT 0x5F375A86
#elif __WORDSIZE == 64
#define MAGIC_CONSTANT 0x5FE6EB50C7B537A9
#endif

    DecimalUnderlying num;
    DecimalUnderlying x2;
    DecimalUnderlying th = 1.5f;
    long i;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    if (num < 0)
        return IncRef(NaN);

    x2 = num * 0.5f;
    i = *(long *) &num;
    i = MAGIC_CONSTANT - (i >> 1);
    num = *(DecimalUnderlying *) &i;
    num *= (th - (x2 * num * num)); // 1st iteration (Newton's method)

    return DecimalNew(num);
#undef MAGIC_CONSTANT
}

ARGON_FUNCTION5(math_, tan,
                "Returns the tangent of an angle of x radians."
                ""
                "- Parameter x: value representing an angle, expressed in radians."
                "- Returns: tangent of x radians.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = tan(num);

    return DecimalNew(num);
}

ARGON_FUNCTION5(math_, tanh,
                "Returns the hyperbolic tangent of x."
                ""
                "- Parameter x: value representing a hyperbolic angle."
                "- Returns: hyperbolic tangent of x.",
                1, false) {
    DecimalUnderlying num;

    CHECK_NUMBER(*argv);

    CONVERT_DOUBLE(*argv, num);

    num = tanh(num);

    return DecimalNew(num);
}

bool math_init(Module *module) {
#define ADD_PROPERTY(name, pinfo)                                                           \
    do {                                                                                    \
        if(object == nullptr || !(ok = ModuleAddProperty(module, name, object, pinfo))) {   \
            Release(object);                                                                \
            goto error;                                                                     \
        }                                                                                   \
        Release(object);                                                                    \
    } while(0)                                                                              \

    ArObject *object;
    bool ok = false;

    object = DecimalNew(M_PI);
    ADD_PROPERTY("pi", MODULE_ATTRIBUTE_PUB_CONST);

    object = DecimalNew(M_2_SQRTPI);
    ADD_PROPERTY("two_sqrtpi", MODULE_ATTRIBUTE_PUB_CONST);

    object = DecimalNew(M_E);
    ADD_PROPERTY("e", MODULE_ATTRIBUTE_PUB_CONST);

    object = DecimalNew(6.2831853071795864769252867665590057683943L);
    ADD_PROPERTY("tau", MODULE_ATTRIBUTE_PUB_CONST);

    object = IncRef(NaN);
    ADD_PROPERTY("nan", MODULE_ATTRIBUTE_PUB_CONST);

    object = IncRef(Inf);
    ADD_PROPERTY("inf", MODULE_ATTRIBUTE_PUB_CONST);

    error:
    return ok;
#undef ADD_PROPERTY
}

const PropertyBulk math_bulk[] = {
        MODULE_BULK_EXPORT_FUNCTION(math_acos_),
        MODULE_BULK_EXPORT_FUNCTION(math_asin_),
        MODULE_BULK_EXPORT_FUNCTION(math_atan_),
        MODULE_BULK_EXPORT_FUNCTION(math_ceil_),
        MODULE_BULK_EXPORT_FUNCTION(math_cos_),
        MODULE_BULK_EXPORT_FUNCTION(math_cosh_),
        MODULE_BULK_EXPORT_FUNCTION(math_exp_),
        MODULE_BULK_EXPORT_FUNCTION(math_fabs_),
        MODULE_BULK_EXPORT_FUNCTION(math_floor_),
        MODULE_BULK_EXPORT_FUNCTION(math_fmod_),
        MODULE_BULK_EXPORT_FUNCTION(math_log_),
        MODULE_BULK_EXPORT_FUNCTION(math_log10_),
        MODULE_BULK_EXPORT_FUNCTION(math_mpow_),
        MODULE_BULK_EXPORT_FUNCTION(math_pow_),
        MODULE_BULK_EXPORT_FUNCTION(math_sin_),
        MODULE_BULK_EXPORT_FUNCTION(math_sinh_),
        MODULE_BULK_EXPORT_FUNCTION(math_sqrt_),
        MODULE_BULK_EXPORT_FUNCTION(math_isqrt_),
        MODULE_BULK_EXPORT_FUNCTION(math_tan_),
        MODULE_BULK_EXPORT_FUNCTION(math_tanh_),
        {nullptr, nullptr, false, PropertyInfo()} // Sentinel
};

const ModuleInit module_math = {
        "math",
        "Mathematical functions",
        math_bulk,
        math_init,
        nullptr
};

Module *argon::module::MathNew() {
    return ModuleNew(&module_math);
}

#undef CHECK_INTEGER
#undef CHECK_NUMBER
#undef CONVERT_DOUBLE

// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <random>

#include <utils/macros.h>

#include <object/datatype/decimal.h>
#include <object/datatype/error.h>
#include <object/datatype/integer.h>
#include <object/datatype/nil.h>

#include "modules.h"

using namespace argon::object;
using namespace argon::module;

#if _ARGON_ENVIRON == 32
using RaEngine = std::mt19937;
#else
using RaEngine = std::mt19937_64;
#endif

ARGON_METHOD5(random_, discard, "", 1, false) {
    return IncRef(NilVal);
}

ARGON_METHOD5(random_, random, "", 0, false) {
    return ErrorFormat(type_not_implemented_, "you must implement %s::random", AR_TYPE_NAME(self));
}

ARGON_METHOD5(random_, randbits, "", 1, false) {
    return ErrorFormat(type_not_implemented_, "you must implement %s::randbits", AR_TYPE_NAME(self));
}

ARGON_METHOD5(random_, seed, "", 1, false) {
    return IncRef(NilVal);
}

const NativeFunc random_methods[] = {
        random_discard_,
        random_random_,
        random_randbits_,
        random_seed_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots random_obj = {
        random_methods,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

const TypeInfo RandomTraitType = {
        TYPEINFO_STATIC_INIT,
        "Random",
        nullptr,
        0,
        TypeInfoFlags::TRAIT,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &random_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *type_random_ = &RandomTraitType;

ArObject *RandomNew(ArSize seed);

struct RdEngine : ArObject {
    RaEngine engine;
    ArSize seed;
};

ARGON_FUNCTION5(rdengine_, new, "", 0, true) {
    std::random_device rd;

    ArSize seed = rd();

    if (!VariadicCheckPositional("RdEngine::new", count, 0, 1))
        return nullptr;

    if (count == 1) {
        if (!AR_TYPEOF(*argv, type_integer_))
            return ErrorFormat(type_type_error_, "random::new expected integer as seed optional parameter, not '%s'",
                               AR_TYPE_NAME(*argv));

        seed = ((Integer *) *argv)->integer;
    }

    return RandomNew(seed);
}

ARGON_METHOD5(rdengine_, discard, "", 1, false) {
    auto random = (RdEngine *) self;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "RdEngine::discard expected integer, not '%s'", AR_TYPE_NAME(*argv));

    random->engine.discard(((Integer *) *argv)->integer);

    return IncRef(NilVal);
}

ARGON_METHOD5(rdengine_, random, "", 0, false) {
    std::uniform_real_distribution<DecimalUnderlying> dis(0.0, 1.0);

    return DecimalNew(dis(((RdEngine *) self)->engine));
}

ARGON_METHOD5(rdengine_, randbits, "", 1, false) {
    auto random = (RdEngine *) self;
    IntegerUnderlying bits;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "RdEngine::randbits expected integer, not '%s'", AR_TYPE_NAME(*argv));

    bits = ((Integer *) *argv)->integer;

    if (bits >= _ARGON_ENVIRON)
        return ErrorFormat(type_value_error_, "RdEngine::randbits param bits must be between [1,%d)", _ARGON_ENVIRON);

    return IntegerNew((IntegerUnderlying) (random->engine() >> (_ARGON_ENVIRON - bits)));
}

ARGON_METHOD5(rdengine_, seed, "", 1, false) {
    auto random = (RdEngine *) self;

    IntegerUnderlying seed;

    if (!AR_TYPEOF(*argv, type_integer_))
        return ErrorFormat(type_type_error_, "RdEngine::seed expected integer, not '%s'", AR_TYPE_NAME(*argv));

    seed = ((Integer *) *argv)->integer;

    random->engine.seed(seed);
    random->seed = seed;

    return IncRef(NilVal);
}

const NativeFunc rdengine_methods[] = {
        rdengine_discard_,
        rdengine_new_,
        rdengine_random_,
        rdengine_randbits_,
        rdengine_seed_,
        ARGON_METHOD_SENTINEL
};

const TypeInfo *rdengine_bases[] = {
        type_random_,
        nullptr
};

const ObjectSlots rdengine_obj = {
        rdengine_methods,
        nullptr,
        rdengine_bases,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *rdengine_str(RdEngine *self) {
    return StringNewFormat("<%s(mersenne twister) with seed: %lu>", AR_TYPE_NAME(self), self->seed);
}

const TypeInfo RdEngineType = {
        TYPEINFO_STATIC_INIT,
        "RdEngine",
        nullptr,
        sizeof(RdEngine),
        TypeInfoFlags::BASE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (UnaryOp) rdengine_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &rdengine_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *type_rdengine_ = &RdEngineType;

ArObject *RandomNew(ArSize seed) {
    auto rd = ArObjectNew<RdEngine>(RCType::INLINE, type_rdengine_);

    if (rd != nullptr) {
        rd->engine = RaEngine(seed);
        rd->seed = seed;
    }

    return rd;
}

bool random_init(Module *self) {
    if (!TypeInit((TypeInfo *) type_random_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_rdengine_, nullptr))
        return false;

    if(!ModuleAddProperty(self, type_random_->name, (ArObject *) type_random_, MODULE_ATTRIBUTE_PUB_CONST))
        return false;

    return ModuleAddProperty(self, type_rdengine_->name, (ArObject *) type_rdengine_, MODULE_ATTRIBUTE_PUB_CONST);
}

const ModuleInit module_random = {
        "_random",
        "",
        nullptr,
        random_init,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_random_ = &module_random;

// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <object/datatype/integer.h>
#include <object/datatype/error.h>
#include <object/datatype/string.h>
#include <object/datatype/tuple.h>

#include "modules.h"
#include "regex.h"
#include "object/datatype/bytes.h"
#include "object/datatype/bool.h"

using namespace argon::object;
using namespace argon::module;

NativeMember match_members[] = {
        {"match", NativeMemberType::AROBJECT, offsetof(Match, match), true},
        {"start", NativeMemberType::INT, offsetof(Match, start), true},
        {"end", NativeMemberType::INT, offsetof(Match, end), true},
        ARGON_MEMBER_SENTINEL
};

ObjectSlots match_obj{
        nullptr,
        match_members,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *match_compare(const Match *self, ArObject *other, CompareMode mode) {
    auto *o = (Match *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    return BoolToArBool(self->start == o->start && self->end == o->end && Equal(self->match, o->match));
}

ArObject *match_str(const Match *self) {
    ArObject *ret;
    String *tmp;

    if ((tmp = (String *) ToString(self->match)) == nullptr)
        return nullptr;

    ret = StringNewFormat("<%s; (%d:%d); %s>", AR_TYPE_NAME(self), self->start, self->end, tmp->buffer);

    Release(tmp);

    return ret;
}

bool match_is_true(const Match *self) {
    return true;
}

void match_cleanup(Match *self) {
    Release(self->match);
}

const TypeInfo REMatchType = {
        TYPEINFO_STATIC_INIT,
        "match",
        nullptr,
        sizeof(Match),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) match_cleanup,
        nullptr,
        (CompareOp) match_compare,
        (BoolUnaryOp) match_is_true,
        nullptr,
        (UnaryOp) match_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &match_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::type_re_match_ = &REMatchType;

Match *MatchNew(const std::csub_match &csmatch, const ArBuffer &buffer) {
    Match *match;
    ArSize length;


    if ((match = ArObjectNew<Match>(RCType::INLINE, type_re_match_)) == nullptr)
        return nullptr;

    match->start = (unsigned char *) csmatch.first - buffer.buffer;
    match->end = (unsigned char *) csmatch.second - buffer.buffer;

    length = match->end - match->start;

    if (AR_TYPEOF(buffer.obj, type_string_))
        match->match = StringNew((const char *) (buffer.buffer + match->start), length);
    else if (AR_TYPEOF(buffer.obj, type_bytes_))
        match->match = BytesNew((Bytes *) buffer.obj, match->start, length);
    else
        match->match = BytesNew((buffer.buffer + match->start), length, true);

    return match;
}

Tuple *MatchesNew(const std::cmatch &cmatch, const ArBuffer &arbuf) {
    Tuple *matches;
    Match *tmp;

    if ((matches = TupleNew(cmatch.size())) != nullptr) {
        for (int i = 0; i < cmatch.size(); i++) {
            if ((tmp = MatchNew(cmatch[i], arbuf)) == nullptr) {
                Release(matches);
                return nullptr;
            }

            TupleInsertAt(matches, i, tmp);
            Release(tmp);
        }
    }

    return matches;
}

// *** PATTERN ***

ARGON_METHOD5(pattern_, match, "", 1, false) {
    ArBuffer buffer{};
    std::cmatch cmatch{};
    const auto *pattern = (Pattern *) self;
    const char *endbuf;
    ArObject *ret;

    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    endbuf = (const char *) (buffer.buffer + buffer.len);

    if (std::regex_match((const char *) buffer.buffer, endbuf, cmatch, *pattern->pattern))
        ret = MatchesNew(cmatch, buffer);
    else
        ret = ARGON_OBJECT_NIL;

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD5(pattern_, search, "", 1, false) {
    ArBuffer buffer{};
    std::cmatch cmatch{};
    const auto *pattern = (Pattern *) self;
    const char *endbuf;
    ArObject *ret;

    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    endbuf = (const char *) (buffer.buffer + buffer.len);

    if (std::regex_search((const char *) buffer.buffer, endbuf, cmatch, *pattern->pattern))
        ret = MatchesNew(cmatch, buffer);
    else
        ret = ARGON_OBJECT_NIL;

    BufferRelease(&buffer);

    return ret;
}

NativeFunc pattern_method[] = {
        pattern_match_,
        pattern_search_,
        ARGON_METHOD_SENTINEL
};

ObjectSlots pattern_obj = {
        pattern_method,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

void pattern_cleanup(Pattern *self) {
    Release(self->init_str);
    delete self->pattern;
}

ArObject *pattern_str(Pattern *self) {
    return StringNewFormat("<%s; \"%s\">", AR_TYPE_NAME(self), self->init_str->buffer);
}

const TypeInfo REPatternType = {
        TYPEINFO_STATIC_INIT,
        "pattern",
        nullptr,
        sizeof(Pattern),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) pattern_cleanup,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        (UnaryOp) pattern_str,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &pattern_obj,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::type_re_pattern_ = &REPatternType;

ARGON_FUNCTION5(regex_, compile, "", 2, false) {
    const auto *mode = (Integer *) argv[1];
    Pattern *pattern;

    if (!CheckArgs("s:pattern,i:mode", func, argv, count))
        return nullptr;

    if ((pattern = ArObjectNew<Pattern>(RCType::INLINE, type_re_pattern_)) == nullptr)
        return nullptr;

    pattern->init_str = (String *) IncRef(argv[0]);

    pattern->pattern = nullptr;
    try {
        pattern->pattern = new std::regex((const char *) pattern->init_str->buffer,
                                          (std::regex::flag_type) mode->integer);
    } catch (const std::regex_error &err) {
        Release(pattern);
        return ErrorFormat(type_runtime_error_, "%s", err.what());
    }

    return pattern;
}

PropertyBulk regex_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("pattern", type_re_pattern_),
        MODULE_EXPORT_FUNCTION(regex_compile_),
        MODULE_EXPORT_SENTINEL
};

bool RegexInit(Module *self) {
#define AddIntConstant(alias, value)                \
    if(!ModuleAddIntConstant(self, #alias, value))  \
        return false

    AddIntConstant(IGNORECASE, std::regex::icase);
    AddIntConstant(OPTIMIZE, std::regex::optimize);

    AddIntConstant(MODE_BASIC, std::regex::basic);
    AddIntConstant(MODE_EXTENDED, std::regex::extended);
    AddIntConstant(MODE_ECMASCRIPT, std::regex::ECMAScript);
    AddIntConstant(MODE_AWK, std::regex::awk);
    AddIntConstant(MODE_GREP, std::regex::grep);
    AddIntConstant(MODE_EGREP, std::regex::egrep);

    TypeInit((TypeInfo *) type_re_pattern_, nullptr);

    TypeInit((TypeInfo *) type_re_match_, nullptr);

    return true;
#undef AddIntConstant
}

const ModuleInit module_regex = {
        "_regex",
        "",
        regex_bulk,
        RegexInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_regex_ = &module_regex;

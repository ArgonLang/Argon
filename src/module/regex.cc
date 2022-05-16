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

const NativeMember match_members[] = {
        ARGON_MEMBER("match", offsetof(Match, match), NativeMemberType::AROBJECT, true),
        ARGON_MEMBER("start", offsetof(Match, start), NativeMemberType::INT, true),
        ARGON_MEMBER("end", offsetof(Match, end), NativeMemberType::INT, true),
        ARGON_MEMBER_SENTINEL
};

const ObjectSlots match_obj{
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
    const auto *o = (Match *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == other)
        return BoolToArBool(true);

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
        TypeInfo_IsTrue_True,
        nullptr,
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
    ArSize length = cmatch.size();
    int start = 0;

    Tuple *matches;
    Match *tmp;

    if (length > 1) {
        start++;
        length--;
    }

    if ((matches = TupleNew(length)) != nullptr) {
        for (int i = start; i < cmatch.size(); i++) {
            if ((tmp = MatchNew(cmatch[i], arbuf)) == nullptr) {
                Release(matches);
                return nullptr;
            }

            TupleInsertAt(matches, i - start, tmp);
            Release(tmp);
        }
    }

    return matches;
}

// *** ITERATOR ***

ArObject *re_iterator_next(REIterator *self) {
    UniqueLock lock(self->lock);
    ArBuffer buffer{};
    std::cmatch cmatch{};
    ArObject *ret = nullptr;
    const char *startbuf;
    const char *endbuf;

    if (!BufferGet(self->target, &buffer, ArBufferFlags::READ))
        return nullptr;

    if (self->lpos >= buffer.len) {
        BufferRelease(&buffer);
        return nullptr;
    }

    startbuf = (const char *) (buffer.buffer + self->lpos);
    endbuf = (const char *) (buffer.buffer + buffer.len);

    if (std::regex_search(startbuf, endbuf, cmatch, *self->pattern->pattern)) {
        ret = MatchesNew(cmatch, buffer);
        if (ret != nullptr) {
            Release(self->last);
            self->last = IncRef(ret);

            self->lpos = cmatch[cmatch.size() - 1].second - (const char *) buffer.buffer;
        }
    }

    BufferRelease(&buffer);

    return ret;
}

ArObject *re_iterator_peek(REIterator *self) {
    UniqueLock lock(self->lock);
    return IncRef(self->last);
}

const IteratorSlots re_iterator = {
        nullptr,
        (UnaryOp) re_iterator_next,
        (UnaryOp) re_iterator_peek,
        nullptr
};

ArObject *re_iterator_compare(REIterator *self, ArObject *other, CompareMode mode) {
    auto *o = (REIterator *) other;

    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if (self == other)
        return BoolToArBool(true);

    UniqueLock lock_self(self->lock);
    UniqueLock lock_other(o->lock);

    return BoolToArBool(self->lpos == o->lpos
                        && Equal(self->pattern, o->pattern)
                        && Equal(self->target, o->target));
}

ArObject *re_iterator_get_iter(ArObject *self) {
    return IncRef(self);
}

void re_iterator_cleanup(REIterator *self) {
    Release(self->pattern);
    Release(self->target);
}

const TypeInfo REIteratorType = {
        TYPEINFO_STATIC_INIT,
        "regex_iterator",
        nullptr,
        sizeof(REIterator),
        TypeInfoFlags::BASE,
        nullptr,
        (VoidUnaryOp) re_iterator_cleanup,
        nullptr,
        (CompareOp) re_iterator_compare,
        TypeInfo_IsTrue_True,
        nullptr,
        nullptr,
        nullptr,
        re_iterator_get_iter,
        nullptr,
        nullptr,
        &re_iterator,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::module::type_re_iterator_ = &REIteratorType;

ArObject *REIteratorNew(Pattern *pattern, ArObject *buffer) {
    REIterator *riter;

    if ((riter = ArObjectNew<REIterator>(RCType::INLINE, type_re_iterator_)) != nullptr) {
        riter->pattern = IncRef(pattern);
        riter->target = IncRef(buffer);
        riter->last = nullptr;
        riter->lpos = 0;
    }

    return riter;
}

// *** PATTERN ***

ARGON_METHOD5(pattern_, findall, "Return all non-overlapping matches of pattern in string."
                                 ""
                                 "- Parameter buffer: buffer object to search on."
                                 "- Returns: tuple of Match object or tuple of tuples of Match object.", 1, false) {
    ArBuffer buffer{};
    std::cmatch cmatch{};
    const auto *pattern = (Pattern *) self;
    const char *endbuf;
    Tuple *ret;

    int idx = 0;

    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    endbuf = (const char *) (buffer.buffer + buffer.len);

    auto re_begin = std::cregex_iterator((const char *) buffer.buffer, endbuf, *pattern->pattern);
    auto re_end = std::cregex_iterator();

    ret = TupleNew(std::distance(re_begin, re_end));

    for (std::cregex_iterator i = re_begin; i != re_end; i++) {
        const auto &matches = *i;

        if (matches.size() > 1) {
            const auto tmatch = MatchesNew(matches, buffer);
            TupleInsertAt(ret, idx, tmatch);
            Release(tmatch);
        } else {
            const auto match = MatchNew(matches[0], buffer);
            TupleInsertAt(ret, idx, match);
            Release(match);
        }

        idx++;
    }

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD5(pattern_, finditer, "Return an iterator yielding regex results."
                                  ""
                                  "- Parameter buffer: buffer object to search on."
                                  "- Returns: regex iterator.", 1, false) {
    if (!CheckArgs("B:buffer", func, argv, count))
        return nullptr;

    return REIteratorNew((Pattern *) self, argv[0]);
}

ARGON_METHOD5(pattern_, match, "Check if zero or more characters ad the beginning of buffer match the regex pattern."
                               ""
                               "- Parameter buffer: buffer object to search on."
                               "- Returns: tuple of Match object or tuple of tuples of Match object.", 1, false) {
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

    std::regex_match((const char *) buffer.buffer, endbuf, cmatch, *pattern->pattern);
    ret = MatchesNew(cmatch, buffer);

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD5(pattern_, search, "Scan through buffer looking for the first location where the regex produces a match."
                                ""
                                "- Parameter buffer: buffer object to search on."
                                "- Returns: tuple of Match object or tuple of tuples of Match object.", 1, false) {
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

    std::regex_search((const char *) buffer.buffer, endbuf, cmatch, *pattern->pattern);
    ret = MatchesNew(cmatch, buffer);

    BufferRelease(&buffer);

    return ret;
}

ARGON_METHOD5(pattern_, sub, "Replaces occurrences of the pattern with the new passed value."
                             ""
                             "- Parameters:"
                             "  - old: buffer on which to search for occurrences."
                             "  - new: buffer containing the new value."
                             "  - count: maximum number of occurrences to replace (-1 all occurrences)."
                             "- Returns: new buffer of the same type as the old buffer with occurrences "
                             "replaced with the value of 'new' buffer.", 3, false) {
    ArBuffer buffer{};
    ArBuffer rbuffer{};
    std::cmatch cmatch{};
    auto *pattern = (Pattern *) self;
    ArObject *ret;
    const char *sbuf;
    const char *ebuf;
    char *newbuf;
    char *newbuf_c;
    ArSize nlen;
    int scount;
    int rcount;

    if (!CheckArgs("B:old,B:new,i:count", func, argv, count))
        return nullptr;

    if (!BufferGet(argv[0], &buffer, ArBufferFlags::READ))
        return nullptr;

    if (!BufferGet(argv[1], &rbuffer, ArBufferFlags::READ))
        return nullptr;

    scount = (int) ((Integer *) argv[2])->integer;
    rcount = scount;

    // Process subs length
    sbuf = (const char *) buffer.buffer;
    ebuf = (const char *) buffer.buffer + buffer.len;
    nlen = buffer.len;
    while (std::regex_search(sbuf, ebuf, cmatch, *pattern->pattern) && (rcount > 0 || rcount == -1)) {
        nlen -= cmatch[0].second - cmatch[0].first;
        nlen += rbuffer.len;
        sbuf = cmatch[0].second;

        if (rcount > 0)
            rcount--;
    }

    if (AR_TYPEOF(argv[0], type_string_))
        nlen++;

    if ((newbuf = ArObjectNewRaw<char *>(nlen)) == nullptr) {
        BufferRelease(&buffer);
        BufferRelease(&rbuffer);
        return nullptr;
    }

    // Write replaces
    sbuf = (const char *) buffer.buffer;
    ebuf = (const char *) buffer.buffer + buffer.len;
    newbuf_c = newbuf;
    while (std::regex_search(sbuf, ebuf, cmatch, *pattern->pattern) && (rcount < scount || scount == -1)) {
        newbuf_c = (char *) argon::memory::MemoryCopy(newbuf_c, sbuf, cmatch[0].first - sbuf);
        newbuf_c = (char *) argon::memory::MemoryCopy(newbuf_c, rbuffer.buffer, rbuffer.len);
        sbuf = cmatch[0].second;

        if (scount != -1)
            rcount++;
    }

    newbuf_c = (char *) argon::memory::MemoryCopy(newbuf_c, sbuf, ebuf - sbuf);

    if (AR_TYPEOF(argv[0], type_string_)) {
        *newbuf_c = '\0';
        ret = StringNewHoldBuffer((unsigned char *) newbuf, nlen);
    } else
        ret = BytesNewHoldBuffer((unsigned char *) newbuf, nlen, nlen, true);

    BufferRelease(&buffer);
    BufferRelease(&rbuffer);

    if (ret == nullptr)
        argon::memory::Free(newbuf);

    return ret;
}

const NativeFunc pattern_method[] = {
        pattern_findall_,
        pattern_finditer_,
        pattern_match_,
        pattern_search_,
        pattern_sub_,
        ARGON_METHOD_SENTINEL
};

const ObjectSlots pattern_obj = {
        pattern_method,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        -1
};

ArObject *pattern_str(Pattern *self) {
    return StringNewFormat("<%s; \"%s\">", AR_TYPE_NAME(self), self->init_str->buffer);
}

void pattern_cleanup(Pattern *self) {
    Release(self->init_str);
    delete self->pattern;
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
        TypeInfo_IsTrue_True,
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

ARGON_FUNCTION5(regex_, compile, "Compile a regular expression pattern into a Pattern object"
                                 ""
                                 "- Parameters:"
                                 "  - pattern: regex string."
                                 "  - mode: engine options."
                                 "- Returns: pattern object."
                                 "- Panic RegexError: regex doesn't compile.", 2, false) {
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
        return ErrorFormat(type_regex_error_, "%s", err.what());
    }

    return pattern;
}

const PropertyBulk regex_bulk[] = {
        MODULE_EXPORT_TYPE_ALIAS("match", type_re_match_),
        MODULE_EXPORT_TYPE_ALIAS("pattern", type_re_pattern_),
        MODULE_EXPORT_TYPE_ALIAS("regex_iterator", type_re_iterator_),
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

    if (!TypeInit((TypeInfo *) type_re_pattern_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_re_match_, nullptr))
        return false;

    return true;
#undef AddIntConstant
}

const ModuleInit module_regex = {
        "_regex",
        "This module provides native support for regex. If you are looking "
        "for advance regex features, you should import regex, not _regex!",
        regex_bulk,
        RegexInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_regex_ = &module_regex;

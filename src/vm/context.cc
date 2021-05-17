// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include <object/datatype/error.h>

#include "runtime.h"
#include "context.h"

using namespace argon::memory;
using namespace argon::vm;
using namespace argon::object;

Context *argon::vm::ContextNew() {
    Context *ctx;

    if ((ctx = (Context *) Alloc(sizeof(Context))) == nullptr)
        return nullptr;

    if ((ctx->import = ImportNew()) == nullptr)
        goto error;

    if ((ctx->bltins = ImportModule(ctx->import, "builtins")) == nullptr)
        goto error;

    if ((ctx->runtime = ImportModule(ctx->import, "runtime")) == nullptr)
        goto error;

    ctx->recursion_limit = 1000; // default maximum recursion depth

    return ctx;

    error:
    ContextDel(ctx);
    return nullptr;
}

ArObject *argon::vm::ContextRuntimeGetProperty(const char *key, const TypeInfo *info) {
    auto *str = StringIntern(key);
    ArObject *ret;

    if (str == nullptr)
        return nullptr;

    ret = NamespaceGetValue(GetContext()->runtime->module_ns, str, nullptr);
    Release(str);

    if (info != nullptr && ret->type != info) {
        Release(ret);
        return ErrorFormat(type_type_error_, "expected '%s' found '%s'", info->name, AR_TYPE_NAME(ret));
    }

    return ret;
}

void argon::vm::ContextDel(Context *context) {
    if (context == nullptr)
        return;

    argon::object::Release(context->bltins);
    Release(context->import);
    Free(context);
}
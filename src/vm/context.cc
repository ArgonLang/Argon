// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <memory/memory.h>

#include "context.h"

using namespace argon::memory;
using namespace argon::vm;

Context *argon::vm::ContextNew() {
    Context *ctx;

    if ((ctx = (Context *) Alloc(sizeof(Context))) == nullptr)
        return nullptr;

    if ((ctx->import = ImportNew()) == nullptr)
        goto error;

    if ((ctx->bltins = ImportModule(ctx->import, "builtins", nullptr)) == nullptr)
        goto error;

    return ctx;

    error:
    ContextDel(ctx);
    return nullptr;
}

void argon::vm::ContextDel(Context *context) {
    if (context == nullptr)
        return;

    argon::object::Release(context->bltins);
    ImportDel(context->import);
    Free(context);
}
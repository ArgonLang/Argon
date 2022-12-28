// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "context.h"

using namespace argon::vm;
using namespace argon::vm::datatype;

Context *argon::vm::ContextNew() {
    auto *context = (Context *) memory::Calloc(sizeof(Context));

    if (context != nullptr) {
        if ((context->imp = importer::ImportNew(context)) == nullptr)
            goto ERROR;

        context->builtins = LoadModule(context->imp, mod::module_builtins_->name, nullptr);
        if (context->builtins == nullptr)
            goto ERROR;

        return context;
    }

    ERROR:
    if (context != nullptr) {
        Release(context->imp);
        Release(context->builtins);
    }

    Release(context);
    return nullptr;
}

void argon::vm::ContextDel(Context *context) {
    Release(context->imp);

    Release(context->builtins);

    memory::Free(context);
}

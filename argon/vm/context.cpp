// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/context.h>

using namespace argon::vm;
using namespace argon::vm::datatype;

Context *argon::vm::ContextNew(Config *global_config) {
    auto *context = (Context *) memory::Calloc(sizeof(Context));

    if (context != nullptr) {
        context->global_config = global_config;

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

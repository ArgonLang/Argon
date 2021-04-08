// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_TRAIT_H_
#define ARGON_OBJECT_TRAIT_H_

#include <object/arobject.h>
#include "namespace.h"
#include "list.h"
#include "tuple.h"
#include "string.h"

namespace argon::object {
    struct Trait : ArObject {
        Namespace *names;
        Tuple *mro;
    };

    extern const TypeInfo type_trait_;

    Trait *TraitNew(String *name, Namespace *names, Trait **bases, ArSize count);

    Trait *TraitNew(String *name, Namespace *names, List *mro);

    List *ComputeMRO(List *bases);

    List *BuildBasesList(Trait **traits, size_t count);

} // namespace argon::object

#endif // !ARGON_OBJECT_TRAIT_H_

// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <argon/vm/importer/ispec.h>

#include <argon/vm/datatype/boolean.h>
#include <argon/vm/datatype/error.h>
#include <argon/vm/datatype/function.h>
#include <argon/vm/datatype/integer.h>
#include <argon/vm/datatype/module.h>

using namespace argon::vm::datatype;

// Prototypes

String *ModuleGetQname(const Module *);

ARGON_FUNCTION(module_module, Module,
               "Create a new empty module.\n"
               "\n"
               "- Parameters:\n"
               "  - name: Module name.\n"
               "  - doc: Module documentations.\n"
               "- Returns: New module object.\n",
               "s: name, s: doc", false, false) {
    return (ArObject *) ModuleNew((String *) args[0], (String *) args[1]);
}

const FunctionDef module_methods[] = {
        module_module,

        ARGON_METHOD_SENTINEL
};

ArObject *module_get_attr(const Module *self, ArObject *key, bool static_attr) {
    String *qname;
    ArObject *value;

    AttributeProperty aprop{};

    if (static_attr) {
        ErrorFormat(kAttributeError[0], kAttributeError[2], AR_TYPE_NAME(self));
        return nullptr;
    }

    if ((value = NamespaceLookup(self->ns, key, &aprop)) == nullptr) {
        value = NamespaceLookup((Namespace *) AR_GET_TYPE(self)->tp_map, key, &aprop);
        if (value == nullptr) {
            qname = ModuleGetQname(self);

            ErrorFormat(kAttributeError[0], "unknown property '%s' of module '%s'",
                        ARGON_RAW_STRING((String *) key), ARGON_RAW_STRING(qname));

            Release(qname);
            return nullptr;
        }
    }

    if (!aprop.IsPublic()) {
        qname = ModuleGetQname(self);

        ErrorFormat(kAttributeError[0], "access violation, member '%s' of module '%s' are private",
                    ARGON_RAW_STRING((String *) key), ARGON_RAW_STRING(qname));

        Release(qname);
        return nullptr;
    }

    return value;
}

bool module_set_attr(Module *self, struct ArObject *key, struct ArObject *value, bool static_attr) {
    String *qname;

    AttributeProperty aprop{};

    if (static_attr) {
        ErrorFormat(kAttributeError[0], kAttributeError[2], AR_TYPE_NAME(self));
        return false;
    }

    if (!NamespaceContains(self->ns, key, &aprop) &&
        !NamespaceContains((Namespace *) AR_GET_TYPE(self)->tp_map, key, &aprop)) {

        qname = ModuleGetQname(self);

        ErrorFormat(kAttributeError[0], "unknown property '%s' of module '%s'",
                    ARGON_RAW_STRING((String *) key), ARGON_RAW_STRING(qname));

        Release(qname);
        return false;
    }

    if (!aprop.IsPublic()) {
        qname = ModuleGetQname(self);

        ErrorFormat(kAttributeError[0], "access violation, member '%s' of module '%s' are private",
                    ARGON_RAW_STRING((String *) key), ARGON_RAW_STRING(qname));

        Release(qname);
        return false;
    }

    if (aprop.IsConstant()) {
        qname = ModuleGetQname(self);

        ErrorFormat(kUnassignableError[0], "property '%s' of module '%s' is constant",
                    ARGON_RAW_STRING((String *) key), ARGON_RAW_STRING(qname));

        Release(qname);
        return false;
    }

    return NamespaceSet(self->ns, key, value);
}

const ObjectSlots module_objslot = {
        module_methods,
        nullptr,
        nullptr,
        (AttributeGetter) module_get_attr,
        (AttributeWriter) module_set_attr,
        offsetof(Module, ns)
};

ArObject *module_compare(Module *self, ArObject *other, CompareMode mode) {
    if (!AR_SAME_TYPE(self, other) || mode != CompareMode::EQ)
        return nullptr;

    if ((ArObject *) self == other)
        return BoolToArBool(true);

    auto *nself = ModuleGetQname(self);
    auto *nother = ModuleGetQname((Module *) other);

    auto *result = BoolToArBool(Equal((ArObject *) nself, (ArObject *) nother));

    Release(nself);
    Release(nother);

    return result;
}

ArObject *module_repr(Module *self) {
    auto *name = ModuleGetQname(self);
    auto *ispec = (argon::vm::importer::ImportSpec *) ModuleLookup(self, "__spec", nullptr);

    ArObject *ret;

    if (ispec != nullptr) {
        if (ispec->origin != nullptr) {
            ret = (ArObject *) StringFormat("<module '%s' from: %s>",
                                            ARGON_RAW_STRING(name),
                                            ARGON_RAW_STRING(ispec->origin));

            Release(name);
            Release(ispec);

            return ret;
        }

        Release(ispec);

        ret = (ArObject *) StringFormat("<native module '%s'>", (ArObject *) name);

        Release(name);

        return ret;
    }

    ret = (ArObject *) StringFormat("<module '%s'>", (ArObject *) name);

    Release(name);

    return ret;
}

bool module_dtor(Module *self) {
    if (self->fini != nullptr)
        self->fini(self);

    if (self->_nfini != nullptr)
        self->_nfini(self);

    Release(self->ns);

    return true;
}

void module_trace(Module *self, Void_UnaryOp trace) {
    trace((ArObject *) self->ns);
}

TypeInfo ModuleType = {
        AROBJ_HEAD_INIT_TYPE,
        "Module",
        nullptr,
        nullptr,
        sizeof(Module),
        TypeInfoFlags::BASE,
        nullptr,
        (Bool_UnaryOp) module_dtor,
        (TraceOp) module_trace,
        nullptr,
        nullptr,
        (CompareOp) module_compare,
        (UnaryConstOp) module_repr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &module_objslot,
        nullptr,
        nullptr,
        nullptr,
        nullptr
};
const TypeInfo *argon::vm::datatype::type_module_ = &ModuleType;

bool AddObject(Module *mod, const ModuleEntry *entry) {
    bool ok = true;

    for (const ModuleEntry *cursor = entry; cursor->prop.object != nullptr && ok; cursor++) {
        const auto *name = cursor->name;
        ArObject *value = cursor->prop.object;

        if (cursor->func) {
            value = (ArObject *) FunctionNew(cursor->prop.func, nullptr, mod->ns);
            if (value == nullptr)
                return false;
        }

        if (name == nullptr && AR_TYPEOF(cursor->prop.object, type_type_))
            name = ((const TypeInfo *) cursor->prop.object)->name;

        assert(name != nullptr);

        ok = NamespaceNewSymbol(mod->ns, name, value, cursor->flags);

        if (cursor->func)
            Release(value);
    }

    return ok;
}

bool MakeID(const Module *mod, const char *id, ArObject *value) {
    auto *key = StringIntern(id);
    if (key == nullptr)
        return false;

    auto ok = NamespaceNewSymbol(mod->ns, (ArObject *) key, value,
                                 MODULE_ATTRIBUTE_DEFAULT | AttributeFlag::NON_COPYABLE);

    Release(key);

    return ok;
}

ArObject *argon::vm::datatype::ModuleLookup(const Module *mod, const char *key, AttributeProperty *out_prop) {
    auto *skey = StringNew(key);
    if (skey == nullptr)
        return nullptr;

    auto *ret = NamespaceLookup(mod->ns, (ArObject *) skey, out_prop);

    Release(skey);

    return ret;
}

bool argon::vm::datatype::ModuleAddIntConstant(Module *mod, const char *key, ArSSize value) {
    auto *avalue = IntNew(value);
    bool ok = false;

    if (avalue != nullptr) {
        ok = ModuleAddObject(mod, key, (ArObject *) avalue, MODULE_ATTRIBUTE_DEFAULT);
        Release(avalue);
    }

    return ok;
}

bool argon::vm::datatype::ModuleAddObject(Module *mod, const char *key, ArObject *object, AttributeFlag flags) {
    auto *skey = StringIntern(key);
    if (skey == nullptr)
        return false;

    auto ok = NamespaceNewSymbol(mod->ns, (ArObject *) skey, object, flags);

    Release(skey);

    return ok;
}

bool argon::vm::datatype::ModuleAddUIntConstant(Module *mod, const char *key, ArSize value) {
    auto *avalue = UIntNew(value);
    bool ok = false;

    if (avalue != nullptr) {
        ok = ModuleAddObject(mod, key, (ArObject *) avalue, MODULE_ATTRIBUTE_DEFAULT);
        Release(avalue);
    }

    return ok;
}

Module *argon::vm::datatype::ModuleNew(const ModuleInit *init) {
    auto *mod = ModuleNew(init->name, init->doc);

    if (mod != nullptr) {
        if (init->init != nullptr && !init->init(mod)) {
            Release(mod);

            return nullptr;
        }

        if (init->bulk != nullptr && !AddObject(mod, init->bulk)) {
            Release(mod);

            return nullptr;
        }

        mod->fini = init->fini;

        if (init->version != nullptr) {
            auto *ver = StringNew(init->version);
            if (ver == nullptr) {
                Release(mod);
                return nullptr;
            }

            if (!MakeID(mod, "__version", (ArObject *) ver)) {
                Release(ver);
                Release(mod);

                return nullptr;
            }

            Release(ver);
        }
    }

    return mod;
}

Module *argon::vm::datatype::ModuleNew(String *name, String *doc) {
    auto *mod = MakeGCObject<Module>(type_module_, true);

    if (mod != nullptr) {
        mod->fini = nullptr;
        mod->_nfini = nullptr;
        mod->_dlhandle = 0;

        if ((mod->ns = NamespaceNew()) == nullptr) {
            Release(mod);
            return nullptr;
        }

        if (!MakeID(mod, "__name", (ArObject *) name)) {
            Release(mod);
            return nullptr;
        }

        if (!MakeID(mod, "__doc", (ArObject *) doc)) {
            Release(mod);
            return nullptr;
        }
    }

    return mod;
}

Module *argon::vm::datatype::ModuleNew(const char *name, const char *doc) {
    auto *sname = StringNew(name);
    auto sdoc = StringNew(doc);

    if (sname == nullptr || sdoc == nullptr) {
        Release(sname);
        Release(sdoc);

        return nullptr;
    }

    auto *mod = ModuleNew(sname, sdoc);

    Release(sname);
    Release(sdoc);

    return mod;
}

String *ModuleGetQname(const Module *self) {
    String *qname;

    qname = (String *) ModuleLookup(self, "__qname", nullptr);
    if (qname == nullptr) {
        qname = (String *) ModuleLookup(self, "__name", nullptr);
        if (qname == nullptr)
            assert(false);
    }

    return qname;
}
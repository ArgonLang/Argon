// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_OBJECT_NAMESPACE_H_
#define ARGON_OBJECT_NAMESPACE_H_

#include "object.h"

#define ARGON_OBJECT_NS_INITIAL_SIZE   16
#define ARGON_OBJECT_NS_LOAD_FACTOR    0.75f
#define ARGON_OBJECT_NS_MUL_FACTOR     (ARGON_OBJECT_NS_LOAD_FACTOR * 2)


#define ARGON_OBJECT_NS_PROP_PUB    (unsigned char) 0x01
#define ARGON_OBJECT_NS_PROP_CONST  (unsigned char)((unsigned char) 0x01 << (unsigned char) 1)
#define ARGON_OBJECT_NS_PROP_WEAK   (unsigned char)((unsigned char) 0x01 << (unsigned char) 2)
#define ARGON_OBJECT_NS_PROP_MEMBER (unsigned char)((unsigned char) 0x01 << (unsigned char) 3)

namespace argon::object {
    class PropertyInfo {
        unsigned char flags_;
    public:
        PropertyInfo() = default;

        explicit PropertyInfo(unsigned char flags) {
            this->flags_ = flags;
        }

        PropertyInfo &operator=(unsigned char flags) {
            this->flags_ = flags;
            return *this;
        }

        [[nodiscard]] bool IsPublic() const {
            return (this->flags_ & ((unsigned char) ARGON_OBJECT_NS_PROP_PUB));
        }

        [[nodiscard]] bool IsConstant() const {
            return this->flags_ & ((unsigned char) ARGON_OBJECT_NS_PROP_CONST);
        };

        [[nodiscard]] bool IsWeak() const {
            return (this->flags_ & ((unsigned char) ARGON_OBJECT_NS_PROP_WEAK));
        }

        [[nodiscard]] bool IsMember() const {
            return (this->flags_ & ((unsigned char) ARGON_OBJECT_NS_PROP_MEMBER));
        }
    };

    struct NsEntry {
        NsEntry *next = nullptr;
        NsEntry *iter_next = nullptr;
        NsEntry *iter_prev = nullptr;

        ArObject *key = nullptr;
        union {
            RefCount ref;
            ArObject *obj;
        };
        PropertyInfo info;
    };

    struct Namespace : ArObject {
        NsEntry **ns;
        NsEntry *iter_begin;
        NsEntry *iter_end;

        size_t cap;
        size_t len;
    };

    Namespace *NamespaceNew();

    bool NamespaceNewSymbol(Namespace *ns, PropertyInfo info, ArObject *key, ArObject *value);

    bool NamespaceSetValue(Namespace *ns, ArObject *key, ArObject *value);

    bool NamespaceContains(Namespace *ns, ArObject *key, PropertyInfo *info);

    ArObject *NamespaceGetValue(Namespace *ns, ArObject *key, PropertyInfo *info);

    void NamespaceRemove(Namespace *ns, ArObject *key);

} // namespace argon::object

#endif // !ARGON_OBJECT_NAMESPACE_H_

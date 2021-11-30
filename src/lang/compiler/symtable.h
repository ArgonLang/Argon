// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_COMPILER_SYMTABLE_H_
#define ARGON_LANG_COMPILER_SYMTABLE_H_

#include <object/arobject.h>
#include <object/datatype/list.h>
#include <object/datatype/map.h>
#include <object/datatype/string.h>

namespace argon::lang::compiler {

    enum class SymbolType {
        CONSTANT,
        FUNC,
        LABEL,
        STRUCT,
        TRAIT,
        UNKNOWN,
        VARIABLE
    };

    static const char *SymbolType2Name[] = {"let", "func", "label", "struct", "trait", "unknown", "var"};

    struct Symbol : object::ArObject {
        argon::object::String *name;
        struct SymbolTable *symt;

        SymbolType kind;

        unsigned short nested;
        unsigned int id;

        bool declared;
        bool free;
    };
    extern const object::TypeInfo *type_symbol_;

    Symbol *SymbolNew();

    struct SymbolTable : object::ArObject {
        // Weak reference to the parent SymT
        SymbolTable *prev;

        argon::object::Map *map;
        argon::object::List *namespaces;

        unsigned short nested;
    };
    extern const object::TypeInfo *type_symtable_;

    bool SymbolTableEnterSub(SymbolTable **symt);

    SymbolTable *SymbolTableNew(SymbolTable *prev);

    Symbol *SymbolTableInsert(SymbolTable *symt, argon::object::String *name, SymbolType kind, bool *out_inserted);

    Symbol *SymbolTableInsertNs(SymbolTable *symt, argon::object::String *name, SymbolType kind);

    Symbol *SymbolTableLookup(SymbolTable *symt, argon::object::String *name);

    void SymbolTableExitSub(SymbolTable **symt);
}

#endif // !ARGON_LANG_COMPILER_SYMTABLE_H_

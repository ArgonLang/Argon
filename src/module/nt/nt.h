// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_MODULE_NT_H_
#define ARGON_MODULE_NT_H_

#include <object/arobject.h>

namespace argon::module::nt {

     argon::object::ArObject *GetLogin();

     int GetExecutablePath(char *out_buf, int size);

} // namespace argon::module::nt

#endif // !ARGON_MODULE_NT_H_
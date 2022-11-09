// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include "config.h"

using namespace argon::vm;

constexpr Config DefaultConfig = {
        nullptr,
        0,

        true,
        false,
        false,
        0,
        0,
        0
};
const Config *argon::vm::kConfigDefault = &DefaultConfig;

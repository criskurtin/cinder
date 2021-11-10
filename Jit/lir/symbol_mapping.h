// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace jit {
namespace lir {

// maps symbols such as functions to addresses
extern const std::unordered_map<std::string, uint64_t> kSymbolMapping;

} // namespace lir
} // namespace jit

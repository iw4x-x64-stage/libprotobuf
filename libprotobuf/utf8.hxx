// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <string_view>

namespace protobuf
{
  // Return true if the bytes are valid UTF-8 as RFC 3629 defines it: each
  // character in its shortest form, with no surrogate halves and nothing
  // above U+10FFFF.
  //
  // We check the values of string fields with this function, which is
  // internal to the library (see the buildfile for details).
  //
  bool
  valid_utf8 (std::string_view) noexcept;
}

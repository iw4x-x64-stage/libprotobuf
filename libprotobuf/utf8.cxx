// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/utf8.hxx>

#include <cstddef> // size_t
#include <cstdint> // uint8_t

#include <unicode/utf8.h> // U8_NEXT()

using namespace std;

namespace protobuf
{
  bool
  valid_utf8 (string_view s) noexcept
  {
    // U8_NEXT() decodes the character at the offset and advances past it,
    // yielding a negative value for a sequence that is not well-formed as
    // RFC 3629 defines it. ICU documents the offset as int32_t, but the
    // macro works with any integer type and so we give it our size_t.
    //
    const uint8_t* p (reinterpret_cast<const uint8_t*> (s.data ()));
    const size_t n (s.size ());

    for (size_t i (0); i != n; )
    {
      UChar32 c;
      U8_NEXT (p, i, n, c);

      if (c < 0)
        return false;
    }

    return true;
  }
}

// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/varint.hxx>

using namespace std;

namespace protobuf
{
  void
  append_varint (string& s, uint64_t v)
  {
    char b[max_varint];
    size_t n (0);

    for (; v >= 0x80; v >>= 7)
      b[n++] = static_cast<char> ((v & 0x7f) | 0x80);

    b[n++] = static_cast<char> (v);
    s.append (b, n);
  }

  varint_status
  read_varint (string_view s, size_t& p, uint64_t& v) noexcept
  {
    v = 0;

    for (unsigned shift (0);; shift += 7)
    {
      if (p == s.size ())
        return varint_status::incomplete;

      const uint64_t b (static_cast<uint8_t> (s[p++]));

      if (shift == 63 && b > 1)
        return varint_status::overflow;

      v |= (b & 0x7f) << shift;

      if ((b & 0x80) == 0)
        return varint_status::complete;
    }
  }
}

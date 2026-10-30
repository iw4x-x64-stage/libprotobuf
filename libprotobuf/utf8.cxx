// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/utf8.hxx>

#include <cstddef> // size_t
#include <cstdint> // uint8_t

using namespace std;

namespace protobuf
{
  bool
  valid_utf8 (string_view s) noexcept
  {
    // Every character is a lead byte followed by a number of continuation
    // bytes, which are 80 to BF. The lead byte says how many there are and,
    // for some leads, narrows the range of the first one, which is how the
    // syntax in RFC 3629 (4) keeps out the invalid characters. So for each
    // lead we look up the number of continuation bytes and the range of the
    // first one.
    //
    struct lead
    {
      uint8_t first; // Range of lead bytes.
      uint8_t last;
      size_t  count; // Number of continuation bytes.
      uint8_t min;   // Range of the first continuation byte.
      uint8_t max;
    };

    static const lead leads[]
    {
      {0xc2, 0xdf, 1, 0x80, 0xbf},
      {0xe0, 0xe0, 2, 0xa0, 0xbf}, // Shortest form.
      {0xe1, 0xec, 2, 0x80, 0xbf},
      {0xed, 0xed, 2, 0x80, 0x9f}, // Below the surrogate halves (D800).
      {0xee, 0xef, 2, 0x80, 0xbf},
      {0xf0, 0xf0, 3, 0x90, 0xbf}, // Shortest form.
      {0xf1, 0xf3, 3, 0x80, 0xbf},
      {0xf4, 0xf4, 3, 0x80, 0x8f}  // Up to U+10FFFF.
    };

    for (size_t i (0); i != s.size (); )
    {
      const uint8_t c (static_cast<uint8_t> (s[i++]));

      if (c < 0x80)
        continue;

      // Find the lead. Bytes 80 to C1 and F5 to FF are never one: the
      // former are continuation bytes or would start an overlong form, and
      // the latter would start characters above U+10FFFF.
      //
      const lead* l (nullptr);

      for (const lead& x: leads)
      {
        if (c >= x.first && c <= x.last)
        {
          l = &x;
          break;
        }
      }

      if (l == nullptr || s.size () - i < l->count)
        return false;

      // Check the continuation bytes, the first against the range of the
      // lead.
      //
      for (size_t n (0); n != l->count; ++n)
      {
        const uint8_t b (static_cast<uint8_t> (s[i++]));

        if (n == 0 ? (b < l->min || b > l->max) : (b < 0x80 || b > 0xbf))
          return false;
      }
    }

    return true;
  }
}

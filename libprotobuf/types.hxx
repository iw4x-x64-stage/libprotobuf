// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <format>
#include <cstddef>     // size_t
#include <cstdint>     // uint8_t, uint32_t
#include <utility>     // forward()
#include <iterator>    // output_iterator
#include <stdexcept>   // invalid_argument
#include <string_view>

#include <libprotobuf/export.hxx>

// Protocol Buffers wire format.
//
// A message is a sequence of fields, each a key followed by a value. The key
// is a varint with the field number in the upper bits and the wire type in
// the lower three. The wire type tells us how the value is encoded and so how
// to step over it. What the value means is up to the schema, which we leave
// to the caller. For example, the len wire type carries strings and nested
// messages alike.
//
namespace protobuf
{
  // Field wire type with the names from the encoding specification.
  //
  // A varint is a little-endian base 128 integer: seven bits per byte with
  // the high bit set on all the bytes but the last. A 64-bit value takes at
  // most ten bytes.
  //
  enum class wire_type: std::uint8_t
  {
    varint = 0,
    i64    = 1, // 8 bytes, little-endian.
    len    = 2, // Varint length followed by that many bytes.
    sgroup = 3, // Group start (deprecated).
    egroup = 4, // Group end (deprecated).
    i32    = 5  // 4 bytes, little-endian.
  };

  // Return the wire type name as spelled in the enumeration (for example,
  // "varint").
  //
  LIBPROTOBUF_SYMEXPORT const char*
  to_string (wire_type) noexcept;

  // Largest field number. Valid field numbers are in the [1, max_field]
  // range: the key is a 32-bit value with the lower three bits taken by the
  // wire type.
  //
  inline constexpr std::uint32_t max_field ((1u << 29) - 1);

  // Thrown on invalid input with the fault description as the exception
  // message. The position is the offset, in bytes, of the offending field's
  // key in the input (see istreambuf for the stream semantics). For example:
  //
  // throw invalid_input (p, "invalid wire type {} in field {}", t, f);
  //
  // Note that the class has only inline members and so isn't exported (see
  // export.hxx for the reason).
  //
  class invalid_input: public std::invalid_argument
  {
  public:
    std::size_t position;

    template <std::formattable<char>... A>
    invalid_input (std::size_t p, std::format_string<A...> f, A&&... a)
      : std::invalid_argument (std::format (f, std::forward<A> (a)...)),
        position (p) {}
  };

  // Thrown by the serializer on a value that cannot be serialized, such as a
  // string that is not valid UTF-8, with the fault description as the
  // exception message. For example:
  //
  // throw invalid_output ("invalid UTF-8 in string field {}", f);
  //
  // Note that the class has only inline members and so isn't exported (see
  // export.hxx for the reason).
  //
  class invalid_output: public std::invalid_argument
  {
  public:
    template <std::formattable<char>... A>
    explicit
    invalid_output (std::format_string<A...> f, A&&... a)
      : std::invalid_argument (std::format (f, std::forward<A> (a)...)) {}
  };
}

namespace std
{
  // Format the wire type as its name (see to_string()).
  //
  // Note that format() accepts any output iterator, which std::formattable
  // requires and which invalid_input relies on.
  //
  template <>
  struct formatter<protobuf::wire_type>: formatter<string_view>
  {
    template <output_iterator<const char&> O>
    auto
    format (protobuf::wire_type t, basic_format_context<O, char>& c) const
    {
      return formatter<string_view>::format (protobuf::to_string (t), c);
    }
  };
}

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

// Types of the Protocol Buffers wire format.
//
// On the wire a message is a sequence of fields, each a key followed by a
// value. The key is a varint holding the field number shifted left by three
// bits, with the wire type in the low three bits. The wire type says how
// the value is encoded, which is all that the parser needs to know to step
// over a field. The schema type of the value follows from the schema alone:
// one wire type carries several schema types, which only the message
// definition tells apart.
//
namespace protobuf
{
  // Wire type of a field, with the names that the encoding specification
  // gives them. A varint is a little-endian base 128 integer: seven bits per
  // byte, with the high bit set on every byte but the last, and at most ten
  // bytes for 64 bits.
  //
  enum class wire_type: std::uint8_t
  {
    varint = 0, // Varint.
    i64    = 1, // Eight little-endian bytes.
    len    = 2, // Varint length followed by that many bytes.
    sgroup = 3, // Start of a group (deprecated).
    egroup = 4, // End of a group (deprecated).
    i32    = 5  // Four little-endian bytes.
  };

  // Return the name of the wire type, which is the same as the enumerator
  // (for example, "varint").
  //
  LIBPROTOBUF_SYMEXPORT const char*
  to_string (wire_type) noexcept;

  // Largest field number. A key is at most 32 bits wide, which leaves 29
  // bits for the field number next to the wire type. The smallest field
  // number is 1.
  //
  inline constexpr std::uint32_t max_field ((1u << 29) - 1);

  // Invalid input of the parser, with the description of the fault as the
  // exception message and the position (offset in bytes) in the input of the
  // key of the field that it concerns. For example:
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

  // Value that cannot be serialized, with the description of the fault as
  // the exception message. For example:
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
  // Print the name of the wire type (see to_string()).
  //
  // Note that the formatter is for any output iterator, which is what makes
  // the type formattable (and so usable in invalid_input, among others).
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

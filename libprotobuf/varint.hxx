// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <string>
#include <cstddef> // size_t
#include <cstdint> // uint64_t
#include <string_view>

// Varint encoding and decoding. This is an implementation detail that is
// not installed (see the buildfile for details).
//
namespace protobuf
{
  // Maximum varint size in bytes, which is what 64 bits take at 7 bits per
  // byte.
  //
  inline constexpr std::size_t max_varint (10);

  // Append the value to the string as a varint.
  //
  void
  append_varint (std::string&, std::uint64_t);

  // Result of read_varint().
  //
  enum class varint_status
  {
    complete,
    incomplete, // Input ends inside the varint.
    overflow    // Value does not fit into 64 bits.
  };

  // Decode the varint that starts at the position in the input into the
  // value, advancing the position past it.
  //
  // Note that we accept the non-minimal encodings (80 00 for 0, say), which
  // the encoding specification allows.
  //
  varint_status
  read_varint (std::string_view,
               std::size_t& position,
               std::uint64_t&) noexcept;
}

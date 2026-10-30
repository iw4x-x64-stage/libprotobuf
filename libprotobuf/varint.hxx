// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <string>
#include <cstddef> // size_t
#include <cstdint> // uint64_t
#include <string_view>

// Varint coding, which is internal to the library (see the buildfile for
// details).
//
namespace protobuf
{
  // Largest size of a varint, in bytes.
  //
  inline constexpr std::size_t max_varint (10);

  // Append the value as a varint.
  //
  void
  append_varint (std::string&, std::uint64_t);

  // Outcome of reading a varint.
  //
  enum class varint_status
  {
    complete,
    incomplete, // Input ends before the last byte.
    overflow    // More than 64 bits.
  };

  // Read the varint at the position into the value and move the position
  // past it.
  //
  // A varint takes at most ten bytes, of which the tenth holds bit 63
  // alone. Note that we accept the redundant forms with high zero groups
  // (80 00 for 0, say), which the encoding allows.
  //
  varint_status
  read_varint (std::string_view,
               std::size_t& position,
               std::uint64_t&) noexcept;
}

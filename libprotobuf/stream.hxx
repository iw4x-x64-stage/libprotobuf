// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <string>
#include <cstddef> // size_t
#include <istream>
#include <ostream>
#include <optional>
#include <string_view>

#include <libprotobuf/types.hxx>

#include <libprotobuf/export.hxx>

// Delimited messages in a stream.
//
// A message carries no length of its own and so takes up the rest of its
// input. To keep several messages in one stream, we write each one preceded
// by its size as a varint. This is the delimited form that the Protocol
// Buffers libraries write and read as well, with writeDelimitedTo() and
// parseDelimitedFrom() in Java, say. For example:
//
// ofstream ofs (..., ios::binary);
// ofs.exceptions (ofstream::badbit | ofstream::failbit);
//
// for (const string& m: messages)
//   write_delimited (ofs, m);
//
// ifstream ifs (..., ios::binary);
// ifs.exceptions (ifstream::badbit);
//
// while (optional<string> m = read_delimited (ifs))
//   ... // Parse *m.
//
namespace protobuf
{
  // Default limit of the message size in read_delimited(). The size comes
  // from the stream, so we limit it to keep a corrupt or hostile one from
  // making us wait for, and store, more than we expect.
  //
  inline constexpr std::size_t default_max_message (64 * 1024 * 1024);

  // Write the message preceded by its size.
  //
  // The badbit and failbit exceptions of the stream must be enabled, so
  // that a write error comes through as std::ios::failure.
  //
  LIBPROTOBUF_SYMEXPORT void
  write_delimited (std::ostream&, std::string_view message);

  // Read the next message, or return nullopt if the stream ends before it.
  // Throw invalid_input if the size is malformed or greater than max, or if
  // the stream ends inside the message. The position in the exception is 0,
  // which is where the size begins.
  //
  // The badbit exception of the stream must be enabled, so that a read
  // error comes through as an exception, and the failbit exception
  // disabled, since reaching the end of the stream sets failbit.
  //
  LIBPROTOBUF_SYMEXPORT std::optional<std::string>
  read_delimited (std::istream&, std::size_t max = default_max_message);
}

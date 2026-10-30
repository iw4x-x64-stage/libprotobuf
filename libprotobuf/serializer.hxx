// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <span>
#include <string>
#include <vector>
#include <cstddef> // size_t
#include <cstdint> // int*_t, uint*_t
#include <string_view>

#include <libprotobuf/types.hxx>

#include <libprotobuf/export.hxx>

namespace protobuf
{
  // Message serializer.
  //
  // For each field call the function of its schema type passing the field
  // number and the value. The fields are written in the order of the calls.
  // A repeated field is written by calling the function for each element or,
  // if the field is packed, by calling the packed_*() function once. A
  // nested message is written between begin_message() and end_message() and
  // a group between begin_group() and end_group(). For example:
  //
  // string m;
  // serializer s (m);
  //
  // s.uint32 (1, id);
  // s.string (2, name);
  // s.packed_sint32 (3, deltas);
  //
  // s.begin_message (4);
  // s.float64 (1, x);
  // s.float64 (2, y);
  // s.end_message ();
  //
  // The field number must be in the [1, max_field] range. All the messages
  // and groups must be ended before the result is used since the message
  // length is only written by end_message().
  //
  // The schema types are mapped to the wire types as follows:
  //
  // int32, int64       varint, two's complement (negative values take ten
  //                    bytes)
  // uint32, uint64     varint
  // sint32, sint64     varint, ZigZag (small negative values take few bytes)
  // bool               varint, 0 or 1 (boolean())
  // enum               varint, as int32 (enumeration())
  // fixed32, sfixed32  i32
  // fixed64, sfixed64  i64
  // float, double      i32 and i64, IEEE 754 binary32 and binary64
  //                    (float32() and float64())
  // string, bytes      len
  //
  // If string() throws invalid_output, then nothing has been written. If
  // any other exception is thrown (std::bad_alloc, for example), then the
  // string may end with a partially written field and should be discarded.
  //
  // Note that the serializer keeps a reference to the string, which should
  // therefore outlive it.
  //
  class LIBPROTOBUF_SYMEXPORT serializer
  {
  public:
    // Append the message to the string.
    //
    explicit
    serializer (std::string&);

    serializer (const serializer&) = delete;
    serializer& operator= (const serializer&) = delete;

    serializer (serializer&&) = delete;
    serializer& operator= (serializer&&) = delete;

    // Scalar fields.
    //
    // Write the field encoding the value as the corresponding schema type.
    //
    void
    int32 (std::uint32_t field, std::int32_t);

    void
    int64 (std::uint32_t field, std::int64_t);

    void
    uint32 (std::uint32_t field, std::uint32_t);

    void
    uint64 (std::uint32_t field, std::uint64_t);

    void
    sint32 (std::uint32_t field, std::int32_t);

    void
    sint64 (std::uint32_t field, std::int64_t);

    void
    boolean (std::uint32_t field, bool);

    void
    enumeration (std::uint32_t field, std::int32_t);

    void
    fixed32 (std::uint32_t field, std::uint32_t);

    void
    fixed64 (std::uint32_t field, std::uint64_t);

    void
    sfixed32 (std::uint32_t field, std::int32_t);

    void
    sfixed64 (std::uint32_t field, std::int64_t);

    void
    float32 (std::uint32_t field, float);

    void
    float64 (std::uint32_t field, double);

    void
    bytes (std::uint32_t field, std::string_view);

    // As bytes() but throw invalid_output if the value is not valid UTF-8,
    // as required for the string type.
    //
    void
    string (std::uint32_t field, std::string_view);

    // Packed repeated fields.
    //
    // Write the elements as a single len field. If there are no elements,
    // then write nothing since an empty packed field would not add anything
    // to the repeated field.
    //
    // Note that std::vector<bool> is not contiguous and so has to be copied
    // into an array of bool to be passed to packed_boolean().
    //
    void
    packed_int32 (std::uint32_t field, std::span<const std::int32_t>);

    void
    packed_int64 (std::uint32_t field, std::span<const std::int64_t>);

    void
    packed_uint32 (std::uint32_t field, std::span<const std::uint32_t>);

    void
    packed_uint64 (std::uint32_t field, std::span<const std::uint64_t>);

    void
    packed_sint32 (std::uint32_t field, std::span<const std::int32_t>);

    void
    packed_sint64 (std::uint32_t field, std::span<const std::int64_t>);

    void
    packed_boolean (std::uint32_t field, std::span<const bool>);

    void
    packed_enumeration (std::uint32_t field, std::span<const std::int32_t>);

    void
    packed_fixed32 (std::uint32_t field, std::span<const std::uint32_t>);

    void
    packed_fixed64 (std::uint32_t field, std::span<const std::uint64_t>);

    void
    packed_sfixed32 (std::uint32_t field, std::span<const std::int32_t>);

    void
    packed_sfixed64 (std::uint32_t field, std::span<const std::int64_t>);

    void
    packed_float32 (std::uint32_t field, std::span<const float>);

    void
    packed_float64 (std::uint32_t field, std::span<const double>);

    // Nested messages and groups.
    //
    // Begin the field and write the subsequent fields into it until the
    // matching end call, which must be for the innermost field that has not
    // yet been ended. Messages and groups can be nested to any depth.
    //
    // Note that the length of a message is only known at its end. So we
    // reserve one byte for it, which is enough for up to 127 bytes, and move
    // the message contents if the length turns out to need more.
    //
    void
    begin_message (std::uint32_t field);

    void
    end_message ();

    void
    begin_group (std::uint32_t field);

    void
    end_group ();

  private:
    bool
    invariant () const noexcept;

    // Message or group being written.
    //
    struct scope
    {
      wire_type     type;     // len for message, sgroup for group.
      std::uint32_t field;
      std::size_t   position; // Value position in out_.
    };

    std::string&       out_;
    std::vector<scope> scopes_; // Innermost last.
  };
}

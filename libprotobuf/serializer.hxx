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
  // Call the function of each field's schema type, in the order in which
  // the fields should appear, with the field number and the value. Write a
  // repeated field by calling the function once per element or, if the
  // schema packs the field, once with all the elements (the packed_*()
  // functions). Write a nested message between begin_message() and
  // end_message() and a group between begin_group() and end_group(). For
  // example:
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
  // The field number must be between 1 and max_field. Every message and
  // group that is begun must be ended before the result is used, since
  // end_message() is what writes the length of the message.
  //
  // The schema types map onto the wire types and C++ types as follows:
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
  // The string() function throws invalid_output before writing anything.
  // Any other exception, such as std::bad_alloc, can leave part of a field
  // in the string, which then cannot be used as a message.
  //
  // Note that the serializer refers to the string, which must therefore
  // outlive it.
  //
  class LIBPROTOBUF_SYMEXPORT serializer
  {
  public:
    // Serialize into the string, appending to what it already holds.
    //
    explicit
    serializer (std::string&);

    serializer (const serializer&) = delete;
    serializer& operator= (const serializer&) = delete;

    serializer (serializer&&) = delete;
    serializer& operator= (serializer&&) = delete;

    // Scalar fields.
    //
    // Write the field with the value as the schema type.
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
    // which a string is required to be.
    //
    void
    string (std::uint32_t field, std::string_view);

    // Packed repeated fields.
    //
    // Write the elements as one field with the values one after another.
    // If there are no elements, then write nothing, since an empty packed
    // field adds no elements to the repeated field.
    //
    // Note that std::vector<bool> doesn't store its elements contiguously
    // and so has to be copied into an array of bool for packed_boolean().
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
    // Begin the field and write the fields that follow into it until the
    // matching end call. Messages and groups nest to any depth. The end call
    // must match the innermost begin call that has not been ended yet.
    //
    // Note that we only learn the length of a message at its end, which is
    // where we write it. Up to 127 bytes of message, the length takes the
    // byte that we set aside for it. A longer message is moved to make room
    // for the longer length.
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
      wire_type     type;     // Wire type of the field's key.
      std::uint32_t field;
      std::size_t   position; // Position of the field's value in out_.
    };

    std::string&       out_;
    std::vector<scope> scopes_; // Innermost last.
  };
}

// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <vector>
#include <cstddef> // size_t
#include <cstdint> // int*_t, uint*_t
#include <string_view>

#include <libprotobuf/types.hxx>

#include <libprotobuf/export.hxx>

namespace protobuf
{
  // Message parser.
  //
  // Call next() to move to the next field and then the function of the
  // field's schema type to get its value. A field that the caller doesn't
  // look at is skipped, which is how the unknown fields of a message are
  // handled. For example:
  //
  // parser p (m);
  //
  // while (p.next ())
  // {
  //   switch (p.field ())
  //   {
  //     case 1: id = p.uint32 ();            break;
  //     case 2: name = p.string ();          break;
  //     case 3: p.sint32 (deltas);           break;
  //     case 4: point = read (p.message ()); break;
  //   }
  // }
  //
  // The functions of the schema types are the same as the serializer's (see
  // serializer for the mapping onto the wire types). They throw
  // invalid_input if the wire type of the field doesn't match the schema
  // type. A varint wider than the schema type is truncated to its lower
  // bits, as if converted to the type in C++, which is what the encoding
  // specification requires. This lets a schema change a field between
  // int32, uint32, int64, and uint64. Likewise, a boolean is true for any
  // value other than 0.
  //
  // The parser reads a field when it moves to it and throws invalid_input
  // if the field is malformed. The position in this exception, as in the
  // ones that the functions of the schema types throw, is that of the
  // field's key. The value of a len field is only checked against its
  // schema type when it is read with its function, so a nested message or
  // packed field that the caller skips is never parsed. A group is checked
  // as far as finding its end, which takes skipping over its fields.
  //
  // Note that the parser refers to the input, which must therefore outlive
  // it as well as the parsers and views that it returns.
  //
  class LIBPROTOBUF_SYMEXPORT parser
  {
  public:
    explicit
    parser (std::string_view) noexcept;

    // Move to the next field and return true, or return false at the end of
    // the message (and on every call after that). Throw invalid_input if the
    // field is malformed.
    //
    bool
    next ();

    // Return the number and the wire type of the current field, and the
    // position of its key in the input, for the caller's own diagnostics.
    // Must follow a call to next() that returned true.
    //
    // Note that the position of the fields of a nested message or group is
    // in the outermost input, the same as in invalid_input.
    //
    std::uint32_t
    field () const noexcept;

    wire_type
    type () const noexcept;

    std::size_t
    position () const noexcept;

    // Scalar fields.
    //
    // Return the value of the current field as the schema type. Throw
    // invalid_input if the wire type doesn't match the schema type. Must
    // follow a call to next() that returned true.
    //
    std::int32_t
    int32 () const;

    std::int64_t
    int64 () const;

    std::uint32_t
    uint32 () const;

    std::uint64_t
    uint64 () const;

    std::int32_t
    sint32 () const;

    std::int64_t
    sint64 () const;

    bool
    boolean () const;

    std::int32_t
    enumeration () const;

    std::uint32_t
    fixed32 () const;

    std::uint64_t
    fixed64 () const;

    std::int32_t
    sfixed32 () const;

    std::int64_t
    sfixed64 () const;

    float
    float32 () const;

    double
    float64 () const;

    // Return the view of the value in the input.
    //
    std::string_view
    bytes () const;

    // As bytes() but also throw invalid_input if the value is not valid
    // UTF-8, which a string is required to be.
    //
    std::string_view
    string () const;

    // Return the parser of the nested message or the group.
    //
    parser
    message () const;

    parser
    group () const;

    // Repeated fields.
    //
    // Append the value of the current field to the vector. The writer of the
    // message may pack a repeated field of a scalar type other than string
    // and bytes, and the reader must accept both forms. So if the field is
    // len, then append all the elements that it packs. Throw invalid_input
    // if the wire type is neither the schema type's nor len or if a packed
    // element is malformed, in which case the vector may hold the elements
    // before it. Must follow a call to next() that returned true.
    //
    void
    int32 (std::vector<std::int32_t>&) const;

    void
    int64 (std::vector<std::int64_t>&) const;

    void
    uint32 (std::vector<std::uint32_t>&) const;

    void
    uint64 (std::vector<std::uint64_t>&) const;

    void
    sint32 (std::vector<std::int32_t>&) const;

    void
    sint64 (std::vector<std::int64_t>&) const;

    void
    boolean (std::vector<bool>&) const;

    void
    enumeration (std::vector<std::int32_t>&) const;

    void
    fixed32 (std::vector<std::uint32_t>&) const;

    void
    fixed64 (std::vector<std::uint64_t>&) const;

    void
    sfixed32 (std::vector<std::int32_t>&) const;

    void
    sfixed64 (std::vector<std::int64_t>&) const;

    void
    float32 (std::vector<float>&) const;

    void
    float64 (std::vector<double>&) const;

  private:
    // Parse the input that starts at the position in the outermost input.
    //
    parser (std::string_view, std::size_t base) noexcept;

    // Throw invalid_input unless the current field has the wire type.
    //
    void
    expect (wire_type) const;

    // Return the position in input_ of the view into it.
    //
    std::size_t
    offset (std::string_view v) const noexcept
    {
      return static_cast<std::size_t> (v.data () - input_.data ());
    }

    std::string_view input_;
    std::size_t      base_;      // Position of input_ in the outermost input.
    std::size_t      next_ = 0;  // Position of the next field in input_.

    // The current field: the position of its key in input_, its number
    // (0 until the first field and after the last), its wire type, and its
    // value. The value of a len field and the contents of a group are a view
    // of the input. Of the other wire types it is the integer that the field
    // holds (the bits, for i32 and i64).
    //
    std::size_t      position_ = 0;
    std::uint32_t    field_ = 0;
    wire_type        type_ = wire_type::varint;
    std::uint64_t    integer_ = 0;
    std::string_view bytes_;
  };
}

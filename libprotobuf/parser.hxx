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
  // Call next() to move to the next field and then the function of its
  // schema type to get the value. Fields that are not read are skipped,
  // which is how unknown fields are handled. For example:
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
  // The schema type functions correspond to those of the serializer (see
  // serializer for the wire type mapping) and throw invalid_input if the
  // field's wire type does not match. A varint value that does not fit the
  // schema type is truncated the same as by a C++ conversion, as required by
  // the encoding. This allows changing a field between int32, uint32, int64,
  // and uint64 in a compatible manner. Similarly, any non-zero bool value is
  // true.
  //
  // The field itself is parsed by next(), which throws invalid_input if it
  // is malformed. The position in this and other invalid_input exceptions is
  // that of the field's key. A len value is only parsed by the function that
  // reads it and so a nested message or packed field that is skipped is
  // never examined. A group is scanned by next() up to its end since that
  // is the only way to step over it.
  //
  // Note that the parser keeps a reference to the input, which should
  // therefore outlive it as well as the parsers and views that it returns.
  //
  class LIBPROTOBUF_SYMEXPORT parser
  {
  public:
    explicit
    parser (std::string_view) noexcept;

    // Move to the next field returning true or return false if there are no
    // more fields (including on subsequent calls). Throw invalid_input if the
    // field is malformed.
    //
    bool
    next ();

    // Return the current field's number, wire type, and key position (the
    // latter is useful in the caller's own diagnostics). Must be called after
    // next() returned true.
    //
    // Note that the position of a nested message or group field is in the
    // outermost input, the same as in invalid_input.
    //
    std::uint32_t
    field () const noexcept;

    wire_type
    type () const noexcept;

    std::size_t
    position () const noexcept;

    // Scalar fields.
    //
    // Return the current field's value as the corresponding schema type.
    // Throw invalid_input if the wire type does not match. Must be called
    // after next() returned true.
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

    // Return the value as a view into the input.
    //
    std::string_view
    bytes () const;

    // As bytes() but also throw invalid_input if the value is not valid
    // UTF-8, as required for the string type.
    //
    std::string_view
    string () const;

    // Return the parser of the nested message or group fields.
    //
    parser
    message () const;

    parser
    group () const;

    // Repeated fields.
    //
    // Append the current field's value to the vector. The writer may pack a
    // repeated scalar field (other than string and bytes) and the reader must
    // accept both forms. So if the field is len, then append all its
    // elements. Throw invalid_input if the wire type is neither the schema
    // type's nor len or if a packed element is malformed, in which case the
    // preceding elements may have been appended. Must be called after next()
    // returned true.
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
    // Parse the input that is at the base position in the outermost input.
    //
    parser (std::string_view, std::size_t base) noexcept;

    // Throw invalid_input unless the current field has the wire type.
    //
    void
    expect (wire_type) const;

    // Return the offset of the view into input_.
    //
    std::size_t
    offset (std::string_view v) const noexcept
    {
      return static_cast<std::size_t> (v.data () - input_.data ());
    }

    std::string_view input_;
    std::size_t      base_;      // Position of input_ in the outermost input.
    std::size_t      next_ = 0;  // Position of the next field in input_.

    // The current field: key position in input_, number (0 before the first
    // field and after the last), wire type, and value. The value of a len
    // field and the contents of a group are a view into the input while for
    // the other wire types it is the integer (the raw bits for i32 and i64).
    //
    std::size_t      position_ = 0;
    std::uint32_t    field_ = 0;
    wire_type        type_ = wire_type::varint;
    std::uint64_t    integer_ = 0;
    std::string_view bytes_;
  };
}

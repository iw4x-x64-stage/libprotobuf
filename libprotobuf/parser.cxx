// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/parser.hxx>

#include <bit>      // bit_cast()
#include <limits>   // numeric_limits
#include <concepts> // integral, floating_point

#include <libprotobuf/utf8.hxx>
#include <libprotobuf/varint.hxx>
#include <libprotobuf/contract.hxx>

using namespace std;

namespace protobuf
{
  namespace
  {
    // We read floating point values as their IEEE 754 bits.
    //
    static_assert (numeric_limits<float>::is_iec559 &&
                   numeric_limits<double>::is_iec559,
                   "float and double must be IEEE 754 binary32 and binary64");

    // Value that a packed field can hold, which is any scalar except for
    // string and bytes.
    //
    template <typename T>
    concept packable = integral<T> || floating_point<T>;

    // Read the n little-endian bytes at the position and move the position
    // past them. Return false if there are fewer than n bytes left.
    //
    bool
    read_fixed (string_view s, size_t& p, size_t n, uint64_t& v) noexcept
    {
      LIBPROTOBUF_PRE (n == 4 || n == 8);

      if (s.size () - p < n)
        return false;

      v = 0;
      for (size_t i (0); i != n; ++i)
        v |= static_cast<uint64_t> (static_cast<uint8_t> (s[p + i])) << (8 * i);

      p += n;
      return true;
    }

    // Read the value of the varint, i32, or i64 field at the position and
    // move the position past it. Throw invalid_input with the key position
    // and the field number if the value is malformed.
    //
    uint64_t
    read_value (string_view s,
                size_t& p,
                wire_type t,
                size_t position,
                uint32_t f)
    {
      uint64_t r;

      switch (t)
      {
        case wire_type::varint:
        {
          switch (read_varint (s, p, r))
          {
            case varint_status::complete:
              break;
            case varint_status::incomplete:
              throw invalid_input (position,
                                   "incomplete varint value in field {}", f);
            case varint_status::overflow:
              throw invalid_input (position,
                                   "invalid varint value in field {}", f);
          }

          break;
        }
        case wire_type::i32:
        case wire_type::i64:
        {
          if (!read_fixed (s, p, t == wire_type::i32 ? 4 : 8, r))
            throw invalid_input (position,
                                 "incomplete {} value in field {}", t, f);

          break;
        }
        default:
          LIBPROTOBUF_UNREACHABLE ();
      }

      return r;
    }

    // Field as read from the input. The value of a len field is the view of
    // its bytes and of a varint, i32, or i64 field the integer. A group key
    // has no value.
    //
    struct field_value
    {
      size_t      position; // Of the key.
      uint32_t    number;
      wire_type   type;
      uint64_t    integer = 0;
      string_view bytes;
    };

    // Read the field at the position and move the position past it. The
    // positions in the exceptions are offset by the base. Throw
    // invalid_input if the field is malformed.
    //
    field_value
    read_field (string_view s, size_t& p, size_t base)
    {
      field_value r {p, 0, wire_type::varint};
      const size_t kp (base + p);

      // Read the key and split it into the field number and the wire type,
      // which are 29 and 3 bits of a 32-bit integer.
      //
      uint64_t k;
      switch (read_varint (s, p, k))
      {
        case varint_status::complete:
          break;
        case varint_status::incomplete:
          throw invalid_input (kp, "incomplete field key");
        case varint_status::overflow:
          throw invalid_input (kp, "invalid field key");
      }

      if (k > numeric_limits<uint32_t>::max ())
        throw invalid_input (kp, "invalid field key");

      r.number = static_cast<uint32_t> (k >> 3);

      if (r.number == 0)
        throw invalid_input (kp, "invalid field number 0");

      const uint64_t t (k & 0x7);

      if (t > 5)
        throw invalid_input (kp, "invalid wire type {} in field {}",
                             t, r.number);

      r.type = static_cast<wire_type> (t);

      // Read the value.
      //
      switch (r.type)
      {
        case wire_type::varint:
        case wire_type::i32:
        case wire_type::i64:
        {
          r.integer = read_value (s, p, r.type, kp, r.number);
          break;
        }
        case wire_type::len:
        {
          // A length that the input cannot hold is incomplete, however
          // large, which also takes care of the lengths beyond size_t.
          //
          uint64_t n;
          switch (read_varint (s, p, n))
          {
            case varint_status::complete:
              break;
            case varint_status::incomplete:
              throw invalid_input (kp, "incomplete len value in field {}",
                                   r.number);
            case varint_status::overflow:
              throw invalid_input (kp, "invalid len value in field {}",
                                   r.number);
          }

          if (n > s.size () - p)
            throw invalid_input (kp, "incomplete len value in field {}",
                                 r.number);

          r.bytes = s.substr (p, static_cast<size_t> (n));
          p += static_cast<size_t> (n);
          break;
        }
        case wire_type::sgroup:
        case wire_type::egroup:
          break;
      }

      return r;
    }

    // Integer on the wire as the value of each schema type. Note that the
    // conversions to a narrower or signed type are modular and so keep the
    // lower bits, which is the truncation that the encoding requires.
    //
    int32_t
    decode_int32 (uint64_t v)
    {
      return static_cast<int32_t> (v);
    }

    int64_t
    decode_int64 (uint64_t v)
    {
      return static_cast<int64_t> (v);
    }

    uint32_t
    decode_uint32 (uint64_t v)
    {
      return static_cast<uint32_t> (v);
    }

    uint64_t
    decode_uint64 (uint64_t v)
    {
      return v;
    }

    // Undo the ZigZag mapping (see the serializer for details): the low bit
    // is the sign and the rest the magnitude, inverted for a negative value.
    //
    int32_t
    decode_sint32 (uint64_t v)
    {
      const uint32_t n (static_cast<uint32_t> (v));
      return static_cast<int32_t> ((n >> 1) ^ (0u - (n & 1)));
    }

    int64_t
    decode_sint64 (uint64_t v)
    {
      return static_cast<int64_t> ((v >> 1) ^ (0ull - (v & 1)));
    }

    bool
    decode_boolean (uint64_t v)
    {
      return v != 0;
    }

    uint32_t
    decode_fixed32 (uint64_t v)
    {
      return static_cast<uint32_t> (v);
    }

    uint64_t
    decode_fixed64 (uint64_t v)
    {
      return v;
    }

    int32_t
    decode_sfixed32 (uint64_t v)
    {
      return static_cast<int32_t> (v);
    }

    int64_t
    decode_sfixed64 (uint64_t v)
    {
      return static_cast<int64_t> (v);
    }

    float
    decode_float32 (uint64_t v)
    {
      return bit_cast<float> (static_cast<uint32_t> (v));
    }

    double
    decode_float64 (uint64_t v)
    {
      return bit_cast<double> (v);
    }

    // Append the value of the current field of the repeated scalar type to
    // the vector (see parser for details). The type's elements are of the
    // wire type, read with the function from the parser and decoded with the
    // function from a packed field.
    //
    template <packable T>
    void
    append_repeated (const parser& p,
                     vector<T>& v,
                     wire_type t,
                     T (parser::*read) () const,
                     T (*decode) (uint64_t))
    {
      if (p.type () != wire_type::len)
      {
        if (p.type () != t)
          throw invalid_input (p.position (),
                               "expected {} or len instead of {} in field {}",
                               t, p.type (), p.field ());

        v.push_back ((p.*read) ());
        return;
      }

      // Read the packed elements.
      //
      const string_view s (p.bytes ());

      for (size_t i (0); i != s.size (); )
        v.push_back (decode (read_value (s, i, t, p.position (), p.field ())));
    }
  }

  parser::
  parser (string_view s) noexcept
    : parser (s, 0)
  {
  }

  parser::
  parser (string_view s, size_t b) noexcept
    : input_ (s), base_ (b)
  {
  }

  bool parser::
  next ()
  {
    if (next_ == input_.size ())
    {
      field_ = 0;
      return false;
    }

    field_value f (read_field (input_, next_, base_));

    // Find the end of a group, which is the egroup key that brings the
    // nesting back to where the group began. Each nested group is checked
    // as it is parsed, if ever, so here we only count the levels.
    //
    switch (f.type)
    {
      case wire_type::sgroup:
      {
        const size_t b (next_);

        for (size_t depth (1);; )
        {
          if (next_ == input_.size ())
            throw invalid_input (base_ + f.position,
                                 "incomplete group in field {}", f.number);

          const size_t e (next_);
          const field_value x (read_field (input_, next_, base_));

          if (x.type == wire_type::sgroup)
            ++depth;
          else if (x.type == wire_type::egroup && --depth == 0)
          {
            if (x.number != f.number)
              throw invalid_input (
                base_ + x.position,
                "expected egroup of field {} instead of field {}",
                f.number, x.number);

            f.bytes = input_.substr (b, e - b);
            break;
          }
        }

        break;
      }
      case wire_type::egroup:
        throw invalid_input (base_ + f.position,
                             "unexpected egroup in field {}", f.number);
      default:
        break;
    }

    position_ = f.position;
    field_    = f.number;
    type_     = f.type;
    integer_  = f.integer;
    bytes_    = f.bytes;

    return true;
  }

  uint32_t parser::
  field () const noexcept
  {
    LIBPROTOBUF_PRE (field_ != 0);
    return field_;
  }

  wire_type parser::
  type () const noexcept
  {
    LIBPROTOBUF_PRE (field_ != 0);
    return type_;
  }

  size_t parser::
  position () const noexcept
  {
    LIBPROTOBUF_PRE (field_ != 0);
    return base_ + position_;
  }

  void parser::
  expect (wire_type t) const
  {
    LIBPROTOBUF_PRE (field_ != 0);

    if (type_ != t)
      throw invalid_input (base_ + position_,
                           "expected {} instead of {} in field {}",
                           t, type_, field_);
  }

  int32_t parser::
  int32 () const
  {
    expect (wire_type::varint);
    return decode_int32 (integer_);
  }

  int64_t parser::
  int64 () const
  {
    expect (wire_type::varint);
    return decode_int64 (integer_);
  }

  uint32_t parser::
  uint32 () const
  {
    expect (wire_type::varint);
    return decode_uint32 (integer_);
  }

  uint64_t parser::
  uint64 () const
  {
    expect (wire_type::varint);
    return decode_uint64 (integer_);
  }

  int32_t parser::
  sint32 () const
  {
    expect (wire_type::varint);
    return decode_sint32 (integer_);
  }

  int64_t parser::
  sint64 () const
  {
    expect (wire_type::varint);
    return decode_sint64 (integer_);
  }

  bool parser::
  boolean () const
  {
    expect (wire_type::varint);
    return decode_boolean (integer_);
  }

  int32_t parser::
  enumeration () const
  {
    expect (wire_type::varint);
    return decode_int32 (integer_);
  }

  uint32_t parser::
  fixed32 () const
  {
    expect (wire_type::i32);
    return decode_fixed32 (integer_);
  }

  uint64_t parser::
  fixed64 () const
  {
    expect (wire_type::i64);
    return decode_fixed64 (integer_);
  }

  int32_t parser::
  sfixed32 () const
  {
    expect (wire_type::i32);
    return decode_sfixed32 (integer_);
  }

  int64_t parser::
  sfixed64 () const
  {
    expect (wire_type::i64);
    return decode_sfixed64 (integer_);
  }

  float parser::
  float32 () const
  {
    expect (wire_type::i32);
    return decode_float32 (integer_);
  }

  double parser::
  float64 () const
  {
    expect (wire_type::i64);
    return decode_float64 (integer_);
  }

  string_view parser::
  bytes () const
  {
    expect (wire_type::len);
    return bytes_;
  }

  string_view parser::
  string () const
  {
    expect (wire_type::len);

    if (!valid_utf8 (bytes_))
      throw invalid_input (base_ + position_,
                           "invalid UTF-8 in string field {}", field_);

    return bytes_;
  }

  parser parser::
  message () const
  {
    expect (wire_type::len);
    return parser (bytes_, base_ + offset (bytes_));
  }

  parser parser::
  group () const
  {
    expect (wire_type::sgroup);
    return parser (bytes_, base_ + offset (bytes_));
  }

  void parser::
  int32 (vector<int32_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::int32, &decode_int32);
  }

  void parser::
  int64 (vector<int64_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::int64, &decode_int64);
  }

  void parser::
  uint32 (vector<uint32_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::uint32, &decode_uint32);
  }

  void parser::
  uint64 (vector<uint64_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::uint64, &decode_uint64);
  }

  void parser::
  sint32 (vector<int32_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::sint32, &decode_sint32);
  }

  void parser::
  sint64 (vector<int64_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::sint64, &decode_sint64);
  }

  void parser::
  boolean (vector<bool>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::boolean, &decode_boolean);
  }

  void parser::
  enumeration (vector<int32_t>& v) const
  {
    append_repeated (*this, v, wire_type::varint,
                     &parser::enumeration, &decode_int32);
  }

  void parser::
  fixed32 (vector<uint32_t>& v) const
  {
    append_repeated (*this, v, wire_type::i32,
                     &parser::fixed32, &decode_fixed32);
  }

  void parser::
  fixed64 (vector<uint64_t>& v) const
  {
    append_repeated (*this, v, wire_type::i64,
                     &parser::fixed64, &decode_fixed64);
  }

  void parser::
  sfixed32 (vector<int32_t>& v) const
  {
    append_repeated (*this, v, wire_type::i32,
                     &parser::sfixed32, &decode_sfixed32);
  }

  void parser::
  sfixed64 (vector<int64_t>& v) const
  {
    append_repeated (*this, v, wire_type::i64,
                     &parser::sfixed64, &decode_sfixed64);
  }

  void parser::
  float32 (vector<float>& v) const
  {
    append_repeated (*this, v, wire_type::i32,
                     &parser::float32, &decode_float32);
  }

  void parser::
  float64 (vector<double>& v) const
  {
    append_repeated (*this, v, wire_type::i64,
                     &parser::float64, &decode_float64);
  }
}

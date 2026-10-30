// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/serializer.hxx>

#include <bit>       // bit_cast()
#include <limits>    // numeric_limits
#include <concepts>  // integral, floating_point
#include <algorithm> // ranges::is_sorted()

#include <libprotobuf/utf8.hxx>
#include <libprotobuf/contract.hxx>

using namespace std;

namespace protobuf
{
  namespace
  {
    // We write floating point values as their IEEE 754 bits.
    //
    static_assert (numeric_limits<float>::is_iec559 &&
                   numeric_limits<double>::is_iec559,
                   "float and double must be IEEE 754 binary32 and binary64");

    // Value that a packed field can hold, which is any scalar except for
    // string and bytes.
    //
    template <typename T>
    concept packable = integral<T> || floating_point<T>;

    // Append the value as a varint.
    //
    void
    append_varint (string& s, uint64_t v)
    {
      char b[10];
      size_t n (0);

      for (; v >= 0x80; v >>= 7)
        b[n++] = static_cast<char> ((v & 0x7f) | 0x80);

      b[n++] = static_cast<char> (v);
      s.append (b, n);
    }

    // Append the lower n bytes of the value, least significant first.
    //
    void
    append_fixed (string& s, uint64_t v, size_t n)
    {
      LIBPROTOBUF_PRE (n == 4 || n == 8);

      char b[8];

      for (size_t i (0); i != n; ++i, v >>= 8)
        b[i] = static_cast<char> (v & 0xff);

      s.append (b, n);
    }

    // Append the key of the field.
    //
    void
    append_key (string& s, uint32_t f, wire_type t)
    {
      LIBPROTOBUF_PRE (f >= 1 && f <= max_field);

      append_varint (s, (static_cast<uint64_t> (f) << 3) |
                        static_cast<uint64_t> (t));
    }

    // Append the value as the wire type says, which must be varint, i32, or
    // i64. For the latter two the value holds the bits of the field.
    //
    void
    append_value (string& s, wire_type t, uint64_t v)
    {
      switch (t)
      {
        case wire_type::varint: append_varint (s, v);   break;
        case wire_type::i32:    append_fixed (s, v, 4); break;
        case wire_type::i64:    append_fixed (s, v, 8); break;
        default:                LIBPROTOBUF_UNREACHABLE ();
      }
    }

    // Write the length of everything after the byte at the position into
    // that byte, which was set aside for it. If the length doesn't fit into
    // a single byte, then insert the bytes that it takes in addition.
    //
    void
    patch_length (string& s, size_t p)
    {
      LIBPROTOBUF_PRE (p < s.size ());

      string l;
      append_varint (l, s.size () - p - 1);
      s.replace (p, 1, l);
    }

    // Value of each schema type as the integer that goes on the wire. Note
    // that the conversions of negative values to unsigned are modular and so
    // keep the two's complement bits.
    //
    uint64_t
    encode_int32 (int32_t v)
    {
      // Sign-extend to 64 bits, as the encoding requires, so that a parser
      // that reads the field as int64 gets the same value.
      //
      return static_cast<uint64_t> (static_cast<int64_t> (v));
    }

    uint64_t
    encode_int64 (int64_t v)
    {
      return static_cast<uint64_t> (v);
    }

    uint64_t
    encode_uint32 (uint32_t v)
    {
      return v;
    }

    uint64_t
    encode_uint64 (uint64_t v)
    {
      return v;
    }

    // ZigZag maps the signed values onto the unsigned ones alternately (0
    // to 0, -1 to 1, 1 to 2, -2 to 3, etc), so that small magnitudes of
    // either sign make small varints. The right shift of a signed value is
    // arithmetic and so yields all ones for a negative value.
    //
    uint64_t
    encode_sint32 (int32_t v)
    {
      return (static_cast<uint32_t> (v) << 1) ^ static_cast<uint32_t> (v >> 31);
    }

    uint64_t
    encode_sint64 (int64_t v)
    {
      return (static_cast<uint64_t> (v) << 1) ^ static_cast<uint64_t> (v >> 63);
    }

    uint64_t
    encode_boolean (bool v)
    {
      return v ? 1 : 0;
    }

    uint64_t
    encode_fixed32 (uint32_t v)
    {
      return v;
    }

    uint64_t
    encode_fixed64 (uint64_t v)
    {
      return v;
    }

    uint64_t
    encode_sfixed32 (int32_t v)
    {
      return static_cast<uint32_t> (v);
    }

    uint64_t
    encode_sfixed64 (int64_t v)
    {
      return static_cast<uint64_t> (v);
    }

    uint64_t
    encode_float32 (float v)
    {
      return bit_cast<uint32_t> (v);
    }

    uint64_t
    encode_float64 (double v)
    {
      return bit_cast<uint64_t> (v);
    }

    // Append the packed field with the elements, each encoded with the
    // function and written as the wire type.
    //
    template <packable T>
    void
    append_packed (string& s,
                   uint32_t f,
                   span<const T> vs,
                   wire_type t,
                   uint64_t (*encode) (T))
    {
      if (vs.empty ())
        return;

      append_key (s, f, wire_type::len);

      const size_t p (s.size ());
      s += '\0';

      for (const T& v: vs)
        append_value (s, t, encode (v));

      patch_length (s, p);
    }
  }

  serializer::
  serializer (std::string& s)
    : out_ (s)
  {
    LIBPROTOBUF_ASSERT (invariant ());
  }

  bool serializer::
  invariant () const noexcept
  {
    // The scopes are nested and so begin in the order of the output, with
    // the innermost one inside what has been written so far.
    //
    return ranges::is_sorted (scopes_, {}, &scope::position) &&
           (scopes_.empty () || scopes_.back ().position <= out_.size ());
  }

  void serializer::
  int32 (uint32_t f, int32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_int32 (v));
  }

  void serializer::
  int64 (uint32_t f, int64_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_int64 (v));
  }

  void serializer::
  uint32 (uint32_t f, uint32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_uint32 (v));
  }

  void serializer::
  uint64 (uint32_t f, uint64_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_uint64 (v));
  }

  void serializer::
  sint32 (uint32_t f, int32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_sint32 (v));
  }

  void serializer::
  sint64 (uint32_t f, int64_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_sint64 (v));
  }

  void serializer::
  boolean (uint32_t f, bool v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_boolean (v));
  }

  void serializer::
  enumeration (uint32_t f, int32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::varint);
    append_varint (out_, encode_int32 (v));
  }

  void serializer::
  fixed32 (uint32_t f, uint32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i32);
    append_fixed (out_, encode_fixed32 (v), 4);
  }

  void serializer::
  fixed64 (uint32_t f, uint64_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i64);
    append_fixed (out_, encode_fixed64 (v), 8);
  }

  void serializer::
  sfixed32 (uint32_t f, int32_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i32);
    append_fixed (out_, encode_sfixed32 (v), 4);
  }

  void serializer::
  sfixed64 (uint32_t f, int64_t v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i64);
    append_fixed (out_, encode_sfixed64 (v), 8);
  }

  void serializer::
  float32 (uint32_t f, float v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i32);
    append_fixed (out_, encode_float32 (v), 4);
  }

  void serializer::
  float64 (uint32_t f, double v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::i64);
    append_fixed (out_, encode_float64 (v), 8);
  }

  void serializer::
  bytes (uint32_t f, string_view v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::len);
    append_varint (out_, v.size ());
    out_ += v;
  }

  void serializer::
  string (uint32_t f, string_view v)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    if (!valid_utf8 (v))
      throw invalid_output ("invalid UTF-8 in string field {}", f);

    bytes (f, v);
  }

  void serializer::
  packed_int32 (uint32_t f, span<const int32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_int32);
  }

  void serializer::
  packed_int64 (uint32_t f, span<const int64_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_int64);
  }

  void serializer::
  packed_uint32 (uint32_t f, span<const uint32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_uint32);
  }

  void serializer::
  packed_uint64 (uint32_t f, span<const uint64_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_uint64);
  }

  void serializer::
  packed_sint32 (uint32_t f, span<const int32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_sint32);
  }

  void serializer::
  packed_sint64 (uint32_t f, span<const int64_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_sint64);
  }

  void serializer::
  packed_boolean (uint32_t f, span<const bool> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_boolean);
  }

  void serializer::
  packed_enumeration (uint32_t f, span<const int32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::varint, &encode_int32);
  }

  void serializer::
  packed_fixed32 (uint32_t f, span<const uint32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i32, &encode_fixed32);
  }

  void serializer::
  packed_fixed64 (uint32_t f, span<const uint64_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i64, &encode_fixed64);
  }

  void serializer::
  packed_sfixed32 (uint32_t f, span<const int32_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i32, &encode_sfixed32);
  }

  void serializer::
  packed_sfixed64 (uint32_t f, span<const int64_t> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i64, &encode_sfixed64);
  }

  void serializer::
  packed_float32 (uint32_t f, span<const float> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i32, &encode_float32);
  }

  void serializer::
  packed_float64 (uint32_t f, span<const double> vs)
  {
    LIBPROTOBUF_INVARIANT (invariant ());
    append_packed (out_, f, vs, wire_type::i64, &encode_float64);
  }

  void serializer::
  begin_message (uint32_t f)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::len);

    // Set the length byte aside (see patch_length() for details).
    //
    scopes_.push_back (scope {wire_type::len, f, out_.size ()});
    out_ += '\0';
  }

  void serializer::
  end_message ()
  {
    LIBPROTOBUF_PRE (!scopes_.empty () &&
                     scopes_.back ().type == wire_type::len);
    LIBPROTOBUF_INVARIANT (invariant ());

    patch_length (out_, scopes_.back ().position);
    scopes_.pop_back ();
  }

  void serializer::
  begin_group (uint32_t f)
  {
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, f, wire_type::sgroup);
    scopes_.push_back (scope {wire_type::sgroup, f, out_.size ()});
  }

  void serializer::
  end_group ()
  {
    LIBPROTOBUF_PRE (!scopes_.empty () &&
                     scopes_.back ().type == wire_type::sgroup);
    LIBPROTOBUF_INVARIANT (invariant ());

    append_key (out_, scopes_.back ().field, wire_type::egroup);
    scopes_.pop_back ();
  }
}

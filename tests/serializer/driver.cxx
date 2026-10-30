// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <span>
#include <print>
#include <format>
#include <memory>       // unique_ptr
#include <string>
#include <vector>
#include <algorithm>    // min()
#include <charconv>     // from_chars()
#include <concepts>     // integral, floating_point, same_as
#include <cstddef>      // size_t
#include <cstdint>      // int*_t, uint*_t
#include <cstdio>       // stdout
#include <iostream>
#include <string_view>
#include <system_error> // errc

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libprotobuf/types.hxx>
#include <libprotobuf/serializer.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace protobuf;

namespace
{
  // Scalar value type that the driver parses.
  //
  template <typename T>
  concept scalar = integral<T> || floating_point<T>;

  // Parse the text as a value of the type (booleans are spelled true and
  // false).
  //
  template <scalar T>
  T
  parse_value (string_view s)
  {
    if constexpr (same_as<T, bool>)
    {
      assert (s == "true" || s == "false");
      return s == "true";
    }
    else
    {
      const char* e (s.data () + s.size ());

      T r {};
      const from_chars_result c (from_chars (s.data (), e, r));
      assert (c.ec == errc () && c.ptr == e);

      return r;
    }
  }

  // Decode the hex digits, repeating the bytes n times if the digits are
  // followed by *<n>.
  //
  string
  parse_hex (string_view s)
  {
    size_t n (1);

    if (size_t p = s.find ('*'); p != string_view::npos)
    {
      n = parse_value<size_t> (s.substr (p + 1));
      s = s.substr (0, p);
    }

    assert (s.size () % 2 == 0);

    string b;
    for (size_t p (0); p != s.size (); p += 2)
    {
      const char* e (s.data () + p + 2);

      uint8_t v;
      const from_chars_result c (from_chars (e - 2, e, v, 16));
      assert (c.ec == errc () && c.ptr == e);

      b += static_cast<char> (v);
    }

    string r;
    for (size_t i (0); i != n; ++i)
      r += b;

    return r;
  }

  // Return the whitespace-separated words of the line.
  //
  vector<string_view>
  split (string_view l)
  {
    vector<string_view> r;

    for (size_t b (0);;)
    {
      b = l.find_first_not_of (' ', b);

      if (b == string_view::npos)
        break;

      const size_t e (min (l.find (' ', b), l.size ()));
      r.push_back (l.substr (b, e - b));
      b = e;
    }

    return r;
  }

  // Write the field (or packed field), parsing the value(s) and calling the
  // serializer function of the type.
  //
  template <scalar T, void (serializer::*F) (uint32_t, T)>
  void
  write (serializer& s, uint32_t f, span<const string_view> vs)
  {
    assert (vs.size () == 1);
    (s.*F) (f, parse_value<T> (vs[0]));
  }

  template <scalar T, void (serializer::*F) (uint32_t, span<const T>)>
  void
  pack (serializer& s, uint32_t f, span<const string_view> vs)
  {
    // Note that vector<bool> has no contiguous storage to make a span of.
    //
    unique_ptr<T[]> a (new T[vs.size ()]);

    for (size_t i (0); i != vs.size (); ++i)
      a[i] = parse_value<T> (vs[i]);

    (s.*F) (f, span<const T> (a.get (), vs.size ()));
  }

  template <void (serializer::*F) (uint32_t, string_view)>
  void
  write_bytes (serializer& s, uint32_t f, span<const string_view> vs)
  {
    assert (vs.size () <= 1);
    (s.*F) (f, vs.empty () ? string () : parse_hex (vs[0]));
  }

  using write_function = void (serializer&, uint32_t, span<const string_view>);

  struct field_type
  {
    const char*     name;
    write_function* write;
  };

  const field_type field_types[]
  {
    {"int32",              &write<int32_t, &serializer::int32>},
    {"int64",              &write<int64_t, &serializer::int64>},
    {"uint32",             &write<uint32_t, &serializer::uint32>},
    {"uint64",             &write<uint64_t, &serializer::uint64>},
    {"sint32",             &write<int32_t, &serializer::sint32>},
    {"sint64",             &write<int64_t, &serializer::sint64>},
    {"boolean",            &write<bool, &serializer::boolean>},
    {"enumeration",        &write<int32_t, &serializer::enumeration>},
    {"fixed32",            &write<uint32_t, &serializer::fixed32>},
    {"fixed64",            &write<uint64_t, &serializer::fixed64>},
    {"sfixed32",           &write<int32_t, &serializer::sfixed32>},
    {"sfixed64",           &write<int64_t, &serializer::sfixed64>},
    {"float32",            &write<float, &serializer::float32>},
    {"float64",            &write<double, &serializer::float64>},
    {"bytes",              &write_bytes<&serializer::bytes>},
    {"string",             &write_bytes<&serializer::string>},
    {"packed-int32",       &pack<int32_t, &serializer::packed_int32>},
    {"packed-int64",       &pack<int64_t, &serializer::packed_int64>},
    {"packed-uint32",      &pack<uint32_t, &serializer::packed_uint32>},
    {"packed-uint64",      &pack<uint64_t, &serializer::packed_uint64>},
    {"packed-sint32",      &pack<int32_t, &serializer::packed_sint32>},
    {"packed-sint64",      &pack<int64_t, &serializer::packed_sint64>},
    {"packed-boolean",     &pack<bool, &serializer::packed_boolean>},
    {"packed-enumeration", &pack<int32_t, &serializer::packed_enumeration>},
    {"packed-fixed32",     &pack<uint32_t, &serializer::packed_fixed32>},
    {"packed-fixed64",     &pack<uint64_t, &serializer::packed_fixed64>},
    {"packed-sfixed32",    &pack<int32_t, &serializer::packed_sfixed32>},
    {"packed-sfixed64",    &pack<int64_t, &serializer::packed_sfixed64>},
    {"packed-float32",     &pack<float, &serializer::packed_float32>},
    {"packed-float64",     &pack<double, &serializer::packed_float64>}
  };
}

// Usage: argv[0] [--prefix <hex>]
//
// Read the fields of a message from stdin, one per line, serialize them,
// and print the result in hex, two lowercase digits for each byte. If the
// prefix is specified, then serialize into a string that holds its bytes.
// The lines are:
//
// <field> <type> [<value>...]
// <field> message
// <field> group
// end
//
// Where <type> is the name of a serializer function, with packed_ spelled
// packed-. The value of the scalar types is a number, or true or false for
// boolean, and of bytes and string the hex digits of the bytes (none for
// empty), optionally followed by *<n> to repeat them n times. The packed
// types take any number of values. A message or a group line begins the
// field, and an end line ends the innermost one that has not been ended
// yet.
//
int
main (int argc, char* argv[])
{
  // Switch stdout to binary mode since on Windows we would otherwise get
  // CRLF line endings, which the expected output doesn't have.
  //
#ifdef _WIN32
  _setmode (_fileno (stdout), _O_BINARY);
#endif

  string r;

  if (argc == 3)
  {
    assert (string_view (argv[1]) == "--prefix");
    r = parse_hex (argv[2]);
  }
  else
    assert (argc == 1);

  try
  {
    serializer s (r);
    vector<bool> groups; // Whether each open field is a group.

    for (string l; getline (cin, l); )
    {
      const vector<string_view> ws (split (l));
      assert (!ws.empty ());

      if (ws[0] == "end")
      {
        assert (ws.size () == 1 && !groups.empty ());

        if (groups.back ())
          s.end_group ();
        else
          s.end_message ();

        groups.pop_back ();
        continue;
      }

      assert (ws.size () >= 2);

      const uint32_t f (parse_value<uint32_t> (ws[0]));
      const string_view t (ws[1]);

      if (t == "message" || t == "group")
      {
        assert (ws.size () == 2);

        if (t == "group")
          s.begin_group (f);
        else
          s.begin_message (f);

        groups.push_back (t == "group");
        continue;
      }

      const field_type* ft (nullptr);
      for (const field_type& x: field_types)
      {
        if (t == x.name)
        {
          ft = &x;
          break;
        }
      }

      assert (ft != nullptr);
      ft->write (s, f, span<const string_view> (ws).subspan (2));
    }

    assert (groups.empty ());
  }
  catch (const invalid_output& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }

  string h;
  for (const char c: r)
    h += format ("{:02x}", static_cast<uint8_t> (c));

  println ("{}", h);
}

// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <map>
#include <print>
#include <format>
#include <string>
#include <vector>
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
#include <libprotobuf/parser.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace protobuf;

namespace
{
  // Scalar value type that the driver prints.
  //
  template <typename T>
  concept scalar = integral<T> || floating_point<T>;

  // Return the value as text, with booleans spelled true and false and
  // floating point values in their shortest form.
  //
  template <scalar T>
  string
  to_text (T v)
  {
    if constexpr (same_as<T, bool>)
      return v ? "true" : "false";
    else
      return format ("{}", v);
  }

  // Return the bytes in hex, two lowercase digits for each byte.
  //
  string
  to_hex (string_view s)
  {
    string r;
    for (const char c: s)
      r += format ("{:02x}", static_cast<uint8_t> (c));

    return r;
  }

  // Decode the hex digits.
  //
  string
  from_hex (string_view s)
  {
    assert (s.size () % 2 == 0);

    string r;
    for (size_t p (0); p != s.size (); p += 2)
    {
      const char* e (s.data () + p + 2);

      uint8_t v;
      const from_chars_result c (from_chars (e - 2, e, v, 16));
      assert (c.ec == errc () && c.ptr == e);

      r += static_cast<char> (v);
    }

    return r;
  }

  // Return the current field's value (or the values that it appends to a
  // repeated field) as text, reading it with the parser function of the
  // type.
  //
  template <scalar T, T (parser::*F) () const>
  string
  read (const parser& p)
  {
    return to_text ((p.*F) ());
  }

  template <scalar T, void (parser::*F) (vector<T>&) const>
  string
  read_repeated (const parser& p)
  {
    vector<T> vs;
    (p.*F) (vs);

    string r;
    for (const T v: vs)
    {
      r += r.empty () ? "" : " ";
      r += to_text (v);
    }

    return r;
  }

  template <string_view (parser::*F) () const>
  string
  read_bytes (const parser& p)
  {
    return to_hex ((p.*F) ());
  }

  using read_function = string (const parser&);

  struct field_type
  {
    const char*    name;
    read_function* read;
  };

  const field_type field_types[]
  {
    {"int32",                &read<int32_t, &parser::int32>},
    {"int64",                &read<int64_t, &parser::int64>},
    {"uint32",               &read<uint32_t, &parser::uint32>},
    {"uint64",               &read<uint64_t, &parser::uint64>},
    {"sint32",               &read<int32_t, &parser::sint32>},
    {"sint64",               &read<int64_t, &parser::sint64>},
    {"boolean",              &read<bool, &parser::boolean>},
    {"enumeration",          &read<int32_t, &parser::enumeration>},
    {"fixed32",              &read<uint32_t, &parser::fixed32>},
    {"fixed64",              &read<uint64_t, &parser::fixed64>},
    {"sfixed32",             &read<int32_t, &parser::sfixed32>},
    {"sfixed64",             &read<int64_t, &parser::sfixed64>},
    {"float32",              &read<float, &parser::float32>},
    {"float64",              &read<double, &parser::float64>},
    {"bytes",                &read_bytes<&parser::bytes>},
    {"string",               &read_bytes<&parser::string>},
    {"repeated-int32",       &read_repeated<int32_t, &parser::int32>},
    {"repeated-int64",       &read_repeated<int64_t, &parser::int64>},
    {"repeated-uint32",      &read_repeated<uint32_t, &parser::uint32>},
    {"repeated-uint64",      &read_repeated<uint64_t, &parser::uint64>},
    {"repeated-sint32",      &read_repeated<int32_t, &parser::sint32>},
    {"repeated-sint64",      &read_repeated<int64_t, &parser::sint64>},
    {"repeated-boolean",     &read_repeated<bool, &parser::boolean>},
    {"repeated-enumeration", &read_repeated<int32_t, &parser::enumeration>},
    {"repeated-fixed32",     &read_repeated<uint32_t, &parser::fixed32>},
    {"repeated-fixed64",     &read_repeated<uint64_t, &parser::fixed64>},
    {"repeated-sfixed32",    &read_repeated<int32_t, &parser::sfixed32>},
    {"repeated-sfixed64",    &read_repeated<int64_t, &parser::sfixed64>},
    {"repeated-float32",     &read_repeated<float, &parser::float32>},
    {"repeated-float64",     &read_repeated<double, &parser::float64>}
  };

  // Map of field numbers to type names.
  //
  using schema = map<uint32_t, string_view>;

  // Append the fields of the message to the result in the form described
  // in main(), indented by the number of spaces.
  //
  void
  dump (parser& p, const schema& s, string& r, size_t indent)
  {
    while (p.next ())
    {
      const uint32_t f (p.field ());
      r.append (indent, ' ');

      auto i (s.find (f));

      if (i == s.end ())
      {
        r += format ("{} {} at {}\n", f, p.type (), p.position ());
        continue;
      }

      const string_view t (i->second);

      if (t == "message" || t == "group")
      {
        r += format ("{} {}\n", f, t);

        parser n (t == "message" ? p.message () : p.group ());
        dump (n, s, r, indent + 2);
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
      r += format ("{} {} {}\n", f, t, ft->read (p));
    }
  }
}

// Usage: argv[0] [<field>=<type>...]
//
// Read a message in hex from stdin, parse it, and print its fields to
// stdout, one per line, as:
//
// <field> <type> <value>
//
// Where <type> is the name of a parser function that the arguments map the
// field number onto, with the vector overloads spelled repeated-<name>. The
// value of the scalar types is a number, or true or false for boolean, and
// of bytes and string the bytes in hex. The repeated types print all the
// values that the field appends. The fields of the message and group types
// follow on the lines after theirs, indented by two spaces, and use the
// same mapping. A field that the arguments don't map is skipped and printed
// as:
//
// <field> <wire-type> at <position>
//
// If the message is invalid, then print the description and the position
// to stderr and exit with status 1.
//
int
main (int argc, char* argv[])
{
  // Switch stdout to the binary mode, since on Windows the text mode would
  // turn the line endings into CRLF.
  //
#ifdef _WIN32
  _setmode (_fileno (stdout), _O_BINARY);
#endif

  schema s;
  for (int i (1); i != argc; ++i)
  {
    const string_view a (argv[i]);
    const size_t p (a.find ('='));
    assert (p != string_view::npos);

    uint32_t f;
    const from_chars_result c (from_chars (a.data (), a.data () + p, f));
    assert (c.ec == errc () && c.ptr == a.data () + p);

    s[f] = a.substr (p + 1);
  }

  string h;
  getline (cin, h);

  const string m (from_hex (h));

  // Only print the fields once the whole message is parsed so that on
  // failure the output only contains the diagnostics.
  //
  string r;

  try
  {
    parser p (m);
    dump (p, s, r, 0);
  }
  catch (const invalid_input& e)
  {
    println (cerr,
             "error: {}\n"
             "  info: at offset {}",
             e.what (),
             e.position);
    return 1;
  }

  print ("{}", r);
}

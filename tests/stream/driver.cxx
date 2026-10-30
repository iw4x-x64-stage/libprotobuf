// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <print>
#include <format>
#include <string>
#include <sstream>
#include <charconv>     // from_chars()
#include <cstddef>      // size_t
#include <cstdint>      // uint8_t
#include <cstdio>       // stdout
#include <iostream>
#include <iterator>     // istreambuf_iterator
#include <optional>
#include <string_view>
#include <system_error> // errc

#ifdef _WIN32
#  include <io.h>    // _setmode(), _fileno()
#  include <fcntl.h> // _O_BINARY
#endif

#include <libprotobuf/types.hxx>
#include <libprotobuf/stream.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace protobuf;

namespace
{
  // Parse the text as a size.
  //
  size_t
  parse_size (string_view s)
  {
    const char* e (s.data () + s.size ());

    size_t r;
    const from_chars_result c (from_chars (s.data (), e, r));
    assert (c.ec == errc () && c.ptr == e);

    return r;
  }

  // Decode the whitespace-separated words of hex digits, repeating a word's
  // bytes n times if it is followed by *<n>.
  //
  string
  from_hex (string_view s)
  {
    string r;

    for (size_t b (0);;)
    {
      b = s.find_first_not_of (' ', b);

      if (b == string_view::npos)
        break;

      size_t e (s.find (' ', b));
      e = e == string_view::npos ? s.size () : e;

      string_view w (s.substr (b, e - b));
      size_t n (1);

      if (size_t p = w.find ('*'); p != string_view::npos)
      {
        n = parse_size (w.substr (p + 1));
        w = w.substr (0, p);
      }

      assert (w.size () % 2 == 0);

      string bs;
      for (size_t p (0); p != w.size (); p += 2)
      {
        const char* x (w.data () + p + 2);

        uint8_t v;
        const from_chars_result c (from_chars (x - 2, x, v, 16));
        assert (c.ec == errc () && c.ptr == x);

        bs += static_cast<char> (v);
      }

      for (size_t i (0); i != n; ++i)
        r += bs;

      b = e;
    }

    return r;
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
}

// Usage: argv[0] -w
//        argv[0] -r [--size] [--first <n>] [--max <n>]
//
// -w  read messages in hex from stdin, one per line, write them to an
//     ostream, and print what it writes to the other stream in hex
//
// -r  read a stream in hex from stdin, read the messages from an istream
//     until it ends, and print each in hex on its own line
//
// --size
//
//     print the size of each message read in place of its bytes
//
// --first <n>
//
//     read only the first n bytes of each message, leaving the rest for
//     next() to skip
//
// --max <n>
//
//     limit the message size to n bytes
//
// The hex input is words of hex digits, each optionally followed by *<n> to
// repeat its bytes n times. If the stream is invalid, then print the
// description and the position to stderr and exit with status 1.
//
int
main (int argc, char* argv[])
{
  assert (argc >= 2);

  // Switch stdout to the binary mode, since on Windows the text mode would
  // turn the line endings into CRLF.
  //
#ifdef _WIN32
  _setmode (_fileno (stdout), _O_BINARY);
#endif

  const string_view m (argv[1]);

  if (m == "-w")
  {
    assert (argc == 2);

    ostringstream os;
    os.exceptions (ostringstream::badbit | ostringstream::failbit);

    protobuf::ostream ps (os);

    for (string l; getline (cin, l); )
    {
      ps.next ();
      ps << from_hex (l);
    }

    ps.close ();

    println ("{}", to_hex (os.str ()));
    return 0;
  }

  assert (m == "-r");

  bool size (false);
  optional<size_t> first;
  size_t max (default_max_message);

  for (int i (2); i != argc; ++i)
  {
    const string_view a (argv[i]);

    if (a == "--size")
      size = true;
    else if (a == "--first" && i + 1 != argc)
      first = parse_size (argv[++i]);
    else if (a == "--max" && i + 1 != argc)
      max = parse_size (argv[++i]);
    else
      assert (false);
  }

  string h;
  getline (cin, h);

  istringstream is (from_hex (h));
  is.exceptions (istringstream::badbit);

  // Only print the messages once the whole stream is read so that on
  // failure the output only contains the diagnostics.
  //
  string r;

  try
  {
    protobuf::istream ps (is, max);

    while (ps.next ())
    {
      string x;

      if (first)
      {
        x.resize (*first);
        ps.read (x.data (), static_cast<streamsize> (x.size ()));
      }
      else
        x.assign (istreambuf_iterator<char> (ps), {});

      r += (size ? to_string (x.size ()) : to_hex (x)) + '\n';
    }
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

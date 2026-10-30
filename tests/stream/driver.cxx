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
  // Return the number that the text spells.
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

  // Return the bytes that the whitespace-separated words of hex digits make
  // up, each word optionally followed by *<n> to repeat its bytes n times.
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
//        argv[0] -r [--size] [--max <n>]
//
// -w  read messages in hex from stdin, one per line, write them to a stream
//     with write_delimited(), and print the stream in hex
//
// -r  read a stream in hex from stdin, read the messages from it with
//     read_delimited() until it ends, and print each in hex on its own line
//
// --size
//
//     print the size of each message read in place of its bytes
//
// --max <n>
//
//     limit the message size to n bytes
//
// The hex input is words of hex digits, each optionally followed by *<n> to
// repeat its bytes n times. If a message is invalid, then print the
// description to stderr and exit with status 1.
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

    for (string l; getline (cin, l); )
      write_delimited (os, from_hex (l));

    println ("{}", to_hex (os.str ()));
    return 0;
  }

  assert (m == "-r");

  bool size (false);
  size_t max (default_max_message);

  for (int i (2); i != argc; ++i)
  {
    const string_view a (argv[i]);

    if (a == "--size")
      size = true;
    else if (a == "--max" && i + 1 != argc)
      max = parse_size (argv[++i]);
    else
      assert (false);
  }

  string h;
  getline (cin, h);

  istringstream is (from_hex (h));
  is.exceptions (istringstream::badbit);

  // Print the messages only once the whole stream is read, so that a
  // failure only has the diagnostics.
  //
  string r;

  try
  {
    while (optional<string> x = read_delimited (is, max))
      r += (size ? to_string (x->size ()) : to_hex (*x)) + '\n';
  }
  catch (const invalid_input& e)
  {
    println (cerr, "error: {}", e.what ());
    return 1;
  }

  print ("{}", r);
}

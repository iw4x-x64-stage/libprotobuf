// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/stream.hxx>

#include <cstdint>   // uint64_t
#include <algorithm> // min()

#include <libprotobuf/varint.hxx>
#include <libprotobuf/contract.hxx>

using namespace std;

namespace protobuf
{
  void
  write_delimited (ostream& os, string_view m)
  {
    LIBPROTOBUF_PRE ((os.exceptions () & (ostream::badbit |
                                          ostream::failbit)) ==
                     (ostream::badbit | ostream::failbit));

    string s;
    append_varint (s, m.size ());

    os.write (s.data (), static_cast<streamsize> (s.size ()));
    os.write (m.data (), static_cast<streamsize> (m.size ()));
  }

  optional<string>
  read_delimited (istream& is, size_t max)
  {
    LIBPROTOBUF_PRE ((is.exceptions () & (istream::badbit |
                                          istream::failbit)) ==
                     istream::badbit);

    // Read the size. We cannot tell its length up front, so we take a byte
    // at a time up to the last one or to the longest that a varint can be.
    //
    string b;

    for (;;)
    {
      const istream::int_type c (is.get ());

      if (c == istream::traits_type::eof ())
      {
        if (b.empty ())
          return nullopt;

        throw invalid_input (0, "incomplete message size");
      }

      b += static_cast<char> (c);

      if ((c & 0x80) == 0 || b.size () == max_varint)
        break;
    }

    size_t p (0);
    uint64_t n;

    if (read_varint (b, p, n) != varint_status::complete)
      throw invalid_input (0, "invalid message size");

    if (n > max)
      throw invalid_input (0, "message size {} greater than limit {}", n, max);

    // Read the message. We grow the string a chunk at a time, so that a
    // size that the stream doesn't back up with data costs no more memory
    // than the data that is there.
    //
    string r;

    while (r.size () != n)
    {
      const size_t o (r.size ());
      const size_t k (min<uint64_t> (n - o, 64 * 1024));

      r.resize (o + k);
      is.read (r.data () + o, static_cast<streamsize> (k));

      if (static_cast<size_t> (is.gcount ()) != k)
        throw invalid_input (0, "incomplete message");
    }

    return r;
  }
}

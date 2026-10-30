// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/types.hxx>

#include <libprotobuf/contract.hxx>

using namespace std;

namespace protobuf
{
  const char*
  to_string (wire_type t) noexcept
  {
    switch (t)
    {
      case wire_type::varint: return "varint";
      case wire_type::i64:    return "i64";
      case wire_type::len:    return "len";
      case wire_type::sgroup: return "sgroup";
      case wire_type::egroup: return "egroup";
      case wire_type::i32:    return "i32";
    }

    LIBPROTOBUF_UNREACHABLE ();
  }
}

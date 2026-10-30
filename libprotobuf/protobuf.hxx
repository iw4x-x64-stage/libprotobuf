#pragma once

#include <iosfwd>
#include <string>

#include <libprotobuf/export.hxx>

namespace protobuf
{
  // Print a greeting for the specified name into the specified
  // stream. Throw std::invalid_argument if the name is empty.
  //
  LIBPROTOBUF_SYMEXPORT void
  say_hello (std::ostream&, const std::string& name);
}

// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#include <libprotobuf/stream.hxx>

#include <cstdint>   // uint64_t
#include <algorithm> // min()
#include <exception> // uncaught_exceptions()

#include <libprotobuf/varint.hxx>
#include <libprotobuf/contract.hxx>

using namespace std;

namespace protobuf
{
  // istreambuf
  //

  void istreambuf::
  open (std::istream& is, size_t max)
  {
    if (buf_ == nullptr)
      buf_ = make_unique_for_overwrite<char[]> (buffer_size);

    is_       = &is;
    max_      = max;
    read_     = 0;
    position_ = 0;
    left_     = 0;

    setg (buf_.get (), buf_.get (), buf_.get ());
  }

  void istreambuf::
  close () noexcept
  {
    is_ = nullptr;
    setg (nullptr, nullptr, nullptr);
  }

  bool istreambuf::
  next ()
  {
    LIBPROTOBUF_PRE (is_open ());

    // Skip the rest of the current message, loading and discarding the part
    // that is still in the other stream.
    //
    while (left_ != 0)
      load (min (left_, buffer_size));

    setg (buf_.get (), buf_.get (), buf_.get ());

    // Read the size a byte at a time since we don't know its length in
    // advance. Stop at the last byte or at the maximum varint length,
    // leaving it to read_varint() to diagnose the latter.
    //
    position_ = read_;

    string b;

    for (;;)
    {
      const int_type c (is_->rdbuf ()->sbumpc ());

      if (traits_type::eq_int_type (c, traits_type::eof ()))
      {
        if (b.empty ())
          return false;

        throw invalid_input (position_, "incomplete message size");
      }

      ++read_;
      b += traits_type::to_char_type (c);

      if ((c & 0x80) == 0 || b.size () == max_varint)
        break;
    }

    size_t p (0);
    uint64_t n;

    if (read_varint (b, p, n) != varint_status::complete)
      throw invalid_input (position_, "invalid message size");

    if (n > max_)
      throw invalid_input (position_,
                           "message size {} greater than limit {}",
                           n, max_);

    left_ = static_cast<size_t> (n);
    return true;
  }

  istreambuf::int_type istreambuf::
  underflow ()
  {
    if (gptr () != egptr ())
      return traits_type::to_int_type (*gptr ());

    if (!is_open () || left_ == 0)
      return traits_type::eof ();

    load (min (left_, buffer_size));
    return traits_type::to_int_type (*gptr ());
  }

  void istreambuf::
  load (size_t n)
  {
    LIBPROTOBUF_PRE (n != 0 && n <= left_ && n <= buffer_size);

    // Note that we must not read past the current message since the caller
    // may stop after any message and continue reading the other stream.
    //
    const size_t r (static_cast<size_t> (
      is_->rdbuf ()->sgetn (buf_.get (), static_cast<streamsize> (n))));

    read_ += r;

    if (r != n)
      throw invalid_input (position_, "incomplete message");

    left_ -= n;
    setg (buf_.get (), buf_.get (), buf_.get () + n);
  }

  // istream
  //

  istream::
  istream (iostate e)
    : std::istream (&buf_)
  {
    LIBPROTOBUF_PRE ((e & badbit) != 0);
    exceptions (e);
  }

  istream::
  istream (std::istream& is, size_t max, iostate e)
    : istream (e)
  {
    open (is, max);
  }

  bool istream::
  next ()
  {
    const bool r (buf_.next ());
    clear ();
    return r;
  }

  // ostreambuf
  //

  void ostreambuf::
  open (std::ostream& os)
  {
    LIBPROTOBUF_PRE (!is_open ());
    LIBPROTOBUF_PRE ((os.exceptions () & (std::ostream::badbit |
                                          std::ostream::failbit)) ==
                     (std::ostream::badbit | std::ostream::failbit));

    if (buf_ == nullptr)
      buf_ = make_unique_for_overwrite<char[]> (buffer_size);

    os_    = &os;
    begun_ = false;
    message_.clear ();

    // Leave the put area empty until next() begins a message (see
    // overflow()).
    //
    setp (nullptr, nullptr);
  }

  void ostreambuf::
  next ()
  {
    LIBPROTOBUF_PRE (is_open ());

    if (begun_)
      save ();

    begun_ = true;
    message_.clear ();
    setp (buf_.get (), buf_.get () + buffer_size);
  }

  void ostreambuf::
  close ()
  {
    if (!is_open ())
      return;

    // If writing the message fails, then the buffer cannot be brought back
    // to a consistent state, so close it regardless.
    //
    try
    {
      if (begun_)
        save ();
    }
    catch (...)
    {
      os_ = nullptr;
      setp (nullptr, nullptr);
      throw;
    }

    os_ = nullptr;
    setp (nullptr, nullptr);
  }

  ostreambuf::int_type ostreambuf::
  overflow (int_type c)
  {
    if (!is_open () || !begun_)
      return traits_type::eof ();

    // Append the put area contents to the message and reset it.
    //
    message_.append (pbase (), static_cast<size_t> (pptr () - pbase ()));
    setp (buf_.get (), buf_.get () + buffer_size);

    if (!traits_type::eq_int_type (c, traits_type::eof ()))
    {
      *pptr () = traits_type::to_char_type (c);
      pbump (1);
    }

    return traits_type::not_eof (c);
  }

  void ostreambuf::
  save ()
  {
    message_.append (pbase (), static_cast<size_t> (pptr () - pbase ()));
    setp (buf_.get (), buf_.get () + buffer_size);

    string s;
    append_varint (s, message_.size ());

    os_->write (s.data (), static_cast<streamsize> (s.size ()));
    os_->write (message_.data (), static_cast<streamsize> (message_.size ()));

    message_.clear ();
  }

  // ostream
  //

  ostream::
  ostream (iostate e)
    : std::ostream (&buf_)
  {
    LIBPROTOBUF_PRE ((e & badbit) != 0);
    exceptions (e);
  }

  ostream::
  ostream (std::ostream& os, iostate e)
    : ostream (e)
  {
    open (os);
  }

  ostream::
  ~ostream ()
  {
    // Make sure close() was called (see the class for details).
    //
    LIBPROTOBUF_PRE (!is_open () || !good () || uncaught_exceptions () != 0);
  }
}

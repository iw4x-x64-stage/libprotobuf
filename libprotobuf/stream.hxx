// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

#pragma once

#include <memory>   // unique_ptr
#include <string>
#include <cstddef>  // size_t
#include <istream>
#include <ostream>
#include <streambuf>

#include <libprotobuf/types.hxx>

#include <libprotobuf/export.hxx>

// Delimited messages in a stream.
//
// A message carries no length of its own and so takes up the rest of its
// input. To keep several messages in one stream, we write each one preceded
// by its size as a varint. This is the delimited form that the Protocol
// Buffers libraries write and read as well, with writeDelimitedTo() and
// parseDelimitedFrom() in Java, say.
//
// The stream buffers below carry the messages of such a stream one at a
// time: next() begins the next message, whose bytes are then what the
// buffer reads or writes.
//
namespace protobuf
{
  // Default limit of the message size in istreambuf. The size comes from
  // the stream, so we limit it to keep a corrupt or hostile one from making
  // us wait for, and store, more than we expect.
  //
  inline constexpr std::size_t default_max_message (64 * 1024 * 1024);

  // Stream buffer that reads the delimited messages of another stream.
  //
  // After next() has begun a message, reading gets the bytes of that
  // message and then the end of the stream. The faults in the other stream
  // are reported with invalid_input, whose position is the offset of the
  // message's size in what the buffer has read from the other stream.
  //
  // Note that the buffer reads the other stream's buffer directly, so the
  // other stream's state stays as it is.
  //
  class LIBPROTOBUF_SYMEXPORT istreambuf: public std::streambuf
  {
  public:
    // Start reading the messages from the stream, limiting their size to max
    // bytes.
    //
    void
    open (std::istream&, std::size_t max = default_max_message);

    bool
    is_open () const noexcept {return is_ != nullptr;}

    // Stop reading. Reading then gets nothing until the buffer is opened
    // again.
    //
    void
    close () noexcept;

    // Move to the next message, skipping over what is left of the current
    // one, and return true, or return false if the other stream ends before
    // the next message. Throw invalid_input if the size is malformed or
    // greater than the limit, or if the other stream ends inside the
    // message being skipped. The buffer must be open.
    //
    bool
    next ();

  protected:
    // Read more of the current message. Throw invalid_input if the other
    // stream ends inside it.
    //
    virtual int_type
    underflow () override;

  private:
    // Read n bytes of the current message from the other stream into the
    // get area. Throw invalid_input if the other stream ends first.
    //
    void
    load (std::size_t n);

  private:
    static constexpr std::size_t buffer_size = 16384;

    std::istream*           is_ = nullptr;
    std::size_t             max_;
    std::size_t             read_;     // Bytes read from the other stream.
    std::size_t             position_; // Of the current message's size.
    std::size_t             left_;     // Bytes of the message not loaded.
    std::unique_ptr<char[]> buf_;      // Get area.
  };

  // Input stream of the delimited messages in another stream. For example:
  //
  // try
  // {
  //   ifstream ifs (..., ios::binary);
  //   ifs.exceptions (ifstream::badbit);
  //
  //   protobuf::istream is (ifs);
  //
  //   while (is.next ())
  //   {
  //     const string m (istreambuf_iterator<char> (is), {});
  //     ... // Parse m.
  //   }
  // }
  // catch (const protobuf::invalid_input& e)
  // {
  //   ... // Invalid stream, see e.what () for the description.
  // }
  //
  // The badbit exception must be enabled, since the faults in the other
  // stream and the errors of its buffer come through as exceptions that the
  // stream only rethrows with badbit enabled.
  //
  class LIBPROTOBUF_SYMEXPORT istream: public std::istream
  {
  public:
    explicit
    istream (iostate = badbit | failbit);

    explicit
    istream (std::istream&,
             std::size_t max = default_max_message,
             iostate = badbit | failbit);

    // See istreambuf::open() for details.
    //
    void
    open (std::istream& is, std::size_t max = default_max_message)
    {
      buf_.open (is, max);
    }

    bool
    is_open () const noexcept {return buf_.is_open ();}

    void
    close () noexcept {buf_.close ();}

    // Move to the next message and clear the stream state, which reading
    // the previous message to its end has set (see istreambuf::next() for
    // details).
    //
    bool
    next ();

  private:
    istreambuf buf_;
  };

  // Stream buffer that writes delimited messages to another stream.
  //
  // After next() has begun a message, what is written to the buffer makes
  // up that message, which is written to the other stream preceded by its
  // size once the next message begins or the buffer is closed. So we keep
  // the whole message in memory until then.
  //
  // Note that syncing the buffer does nothing, since the size of a message
  // must be written before it.
  //
  class LIBPROTOBUF_SYMEXPORT ostreambuf: public std::streambuf
  {
  public:
    // Start writing the messages to the stream.
    //
    // The buffer must not be open, since we would otherwise abandon the
    // message that it is writing. And the stream must have the badbit and
    // failbit exceptions enabled, since that is how we learn that a write
    // failed.
    //
    void
    open (std::ostream&);

    bool
    is_open () const noexcept {return os_ != nullptr;}

    // Write the current message, if any, to the other stream and begin the
    // next one. The buffer must be open.
    //
    void
    next ();

    // Write the current message, if any, to the other stream and stop
    // writing. Close the buffer even if this fails, since the message cannot
    // be written then.
    //
    void
    close ();

  protected:
    // Fail unless a message has begun, since what is written outside of a
    // message would be lost.
    //
    virtual int_type
    overflow (int_type) override;

  private:
    // Write the current message preceded by its size to the other stream.
    //
    void
    save ();

  private:
    static constexpr std::size_t buffer_size = 16384;

    std::ostream*           os_ = nullptr;
    bool                    begun_;   // Whether a message has begun.
    std::string             message_; // Current message without put area.
    std::unique_ptr<char[]> buf_;     // Put area.
  };

  // Output stream of delimited messages to another stream. For example:
  //
  // ofstream ofs (..., ios::binary);
  // ofs.exceptions (ofstream::badbit | ofstream::failbit);
  //
  // protobuf::ostream os (ofs);
  //
  // for (const string& m: messages)
  // {
  //   os.next ();
  //   os << m;
  // }
  //
  // os.close ();
  // ofs.close ();
  //
  // Note that close() must be called explicitly, since the last message is
  // only written once it has been. So the stream must not be open when it
  // is destroyed, unless it is no longer good or an exception is in flight,
  // in which case the message is lost anyway.
  //
  class LIBPROTOBUF_SYMEXPORT ostream: public std::ostream
  {
  public:
    explicit
    ostream (iostate = badbit | failbit);

    explicit
    ostream (std::ostream&, iostate = badbit | failbit);

    ~ostream () override;

    // See ostreambuf::open() for details.
    //
    void
    open (std::ostream& os) {buf_.open (os);}

    bool
    is_open () const noexcept {return buf_.is_open ();}

    // See ostreambuf::next() for details.
    //
    void
    next () {buf_.next ();}

    // Write the last message (see ostreambuf::close() for details).
    //
    void
    close () {buf_.close ();}

  private:
    ostreambuf buf_;
  };
}

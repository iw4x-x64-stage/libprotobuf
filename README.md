# libprotobuf - Protocol Buffers wire format for IW4x projects.

The `libprotobuf` library serializes and parses messages in the Protocol
Buffers wire format. It works at the level of the fields of a message: the
caller writes and reads each field with the type that the message schema
gives it, which lets a program speak a protocol from its schema alone.

## Usage

To start using `libprotobuf` in your project, add the following `depends`
value to your `manifest`, adjusting the version constraint as appropriate:

```
depends: libprotobuf ^0.1.0
```

Then import the library in your `buildfile`:

```
import libs = libprotobuf%lib{protobuf}
```

## Development

The development setup for `libprotobuf` uses the standard `bdep`-based
workflow. On Linux we build natively with GCC and also cross-compile for
Windows with MinGW-w64 GCC, running the tests under Wine with `WINEPATH`
pointing to the directory that contains the MinGW runtime DLLs
(`/usr/x86_64-w64-mingw32/sys-root/mingw/bin` on Fedora). The `libicuuc`
dependency builds its data with the ICU tools, which go into a configuration
for the host that `--create-host-config` creates. For example:

```
git clone https://github.com/iw4x-x64-stage/libprotobuf.git
cd libprotobuf

bdep init -C @gcc --create-host-config cc config.cxx=g++
bdep init -C @mingw --create-host-config cc \
  config.cxx=x86_64-w64-mingw32-g++ \
  config.test.runner="env WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine"
bdep update -a
bdep test -a
```

On Windows we build with MSVC from the x64 Native Tools Command Prompt or the
matching PowerShell, which builds 64-bit binaries. For example:

```
git clone https://github.com/iw4x-x64-stage/libprotobuf.git
cd libprotobuf

bdep init -C -@msvc --create-host-config cc config.cxx=cl
bdep update
bdep test
```

## Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).

## License

`libprotobuf` is licensed under the GNU General Public License, version 3,
subject to the additional permissions described in version 1.1 of the IW4x
Linking Exception.

See [LICENSE](LICENSE.md), [LICENSE-EXCEPTION](LICENSE-EXCEPTION.md), and
[AUTHORS](AUTHORS).

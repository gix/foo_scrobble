# foo_scrobble

## Overview

foo_scrobble is a foobar2000 component for scrobbling to [https://www.last.fm/](https://www.last.fm/).

## Building

Requires:

- Visual C++ 2017,
- [CPP REST SDK](https://github.com/Microsoft/cpprestsdk)

  The SDK is not explicitly referenced. You can use `https://github.com/Microsoft/vcpkg` to build the SDK and make it available transparently.

- foobar2000 SDK

  Copy `src\foobar2000_sdk.user.props.example` to `src\foobar2000_sdk.user.props` and modify the path to the foobar2000 SDK as needed.
  
## License

Code licensed under the [MIT License](LICENSE.txt).

# foo_scrobble

## Overview

foo_scrobble is a foobar2000 component for scrobbling to [https://www.last.fm/](https://www.last.fm/).

- Uses the [Scrobbling 2.0 API](https://www.last.fm/api/scrobbling). You authorize the component with last.fm instead of entering your login credentials into foobar2000.
- Supports "Now Playing" notifications.
- Handles intermittent network outages or reconnects well. No "waiting for handshake" issue.
- Manages the scrobble cache automatically. There is no need to manually submit the cache.
- Allows custom tags for scrobbled details.

To get started, open foobar2000's preferences, navigate to `Tools > Last.fm Scrobbling` and use the top button to authorize your client. The button has a helpful tooltip with detailed instructions.


## Prerequisites

You may have to install [Visual C++ Redistributable for Visual Studio 2017](https://go.microsoft.com/fwlink/?LinkId=746571). Windows 7 also requires [Update for Windows 7 (KB2999226)](https://www.microsoft.com/en-us/download/details.aspx?id=49077) which usually is already installed via Windows Update.


## Building

Requires:

- Visual C++ 2017,
- [CPP REST SDK](https://github.com/Microsoft/cpprestsdk)

  The SDK is not explicitly referenced. You can use `https://github.com/Microsoft/vcpkg` to build the SDK and make it available transparently.

- foobar2000 SDK

  Copy `src\foobar2000_sdk.user.props.example` to `src\foobar2000_sdk.user.props` and modify the path to the foobar2000 SDK as needed.
  
## License

Code licensed under the [MIT License](LICENSE.txt).

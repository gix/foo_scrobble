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

You may have to install [Visual C++ Redistributable for Visual Studio 2015-2019](https://aka.ms/vs/16/release/vc_redist.x86.exe). Windows 7 also requires [Update for Windows 7 (KB2999226)](https://www.microsoft.com/en-us/download/details.aspx?id=49077) which usually is already installed via Windows Update.


## Building

Requires Visual Studio 2019 with:
- Workload: Desktop development for C++
- Component: MSVC v142 - VS 2019 C++ x64/x86 build tools (v14.28)

External dependencies are consumed using the `foo_scrobble-deps` NuGet package.
Run the `eng\Build-Dependencies.ps1` script to build and install the package to the solution.
The foobar2000 SDK is already contained in the repository.

To build a release version run the following from a Visual Studio Developer Command Prompt:
```
msbuild -m build.proj
```
This creates the `foo_scrobble.fb2k-component` and a `foo_scrobble-<VERSION>.zip` archive (containing both DLL and symbols) in the `build\publish` directory.

## License

Code licensed under the [MIT License](LICENSE.txt).

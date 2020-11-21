# Changelog
All notable changes to this project will be documented in this file.

## [] - 0000-00-00
### Fixed
- If authorization with last.fm is rejected, the unauthorized state is now
  immediately visible in the settings dialog, and the session key is discarded.
- The preferences page now correctly handles re-authorization (#17).

## [1.3.1] - 2018-08-18
### Fixed
- Disabling Now Playing notifications is now correctly honored.


## [1.3.0] - 2018-07-29
### Fixed
- Automatic proxy usage on Windows 8.1 and later would fail with
  "Setting proxy options: 12011: The option value cannot be set"


## [1.2.0] - 2018-07-21
### Changed
- Improved error reporting
- Uses foobar2000 SDK 2018-03-06
- Session key revocation properly suspends scrobbling


## [1.1.0] - 2017-11-21
### Added
- Scrobble command in the playback menu
- Submissions can be skipped based on a titleformat script
- Log when and why a track is skipped

### Fixed
- Drop invalid requests instead of attempting to submit them again.


## [1.0.0] - 2017-11-19

Initial release.

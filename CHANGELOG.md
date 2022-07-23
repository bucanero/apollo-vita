# Changelog

All notable changes to the `apollo-vita` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.0.0](https://github.com/bucanero/apollo-vita/releases/tag/v1.0.0) - 2022-07-24

### Added

* PSP save-game management
  - Decryption/Encryption (**Game Key required**)
  - `PARAM.SFO` hashing
  - Cheat code patching
  - Backup/Restore/Export to .Zip
  - Built-in Web server support
* Added External Storage selection (Settings)
* New cheat codes
  - Rainbow Moon (USA)
  - Crazy Market (USA)
  - Dragon Ball Z: Battle of Z (USA)
  - Terraria (USA)
  - Mind 0 (USA)
  - Disgaea 4 - A Promise Revisited (USA)
  - Titan Attacks (USA)
  - Ultratron (USA)
  - Tales of Hearts R (USA)
  - Phantom Breaker: Battlegrounds (USA)
  - Senran Kagura: Shinovi Versus (USA)
  - Dead Nation (AUS)
  - Looney Tunes Galactic Sports (AUS)
  - Eiyuu Senki (JPN)
  - Gundam Breaker 3 (JPN)
  - Genkai Tokki: Moero Chronicle (ASN)
  - Mobile Suit Gundam Extreme Vs Force (ASN)
  - Digimon: Next Order (ASN)
  - BlazBlue: Continuum Shift Extend
  - Criminal Girls: Invite Only
  - BlazBlue: Chronophantasma Extend
  - Under Night In-Birth Exe:Late(st)
* Custom checksum support
  - BlazBlue: Continuum Shift Extend
  - BlazBlue: Chronophantasma Extend

### Misc

* Changed background music
* Updated UI with proper storage names
* Updated networking code to `libcurl` with TLS 1.2 support

### Fixed

* Improved save-game detailed information
* Improved `Settings` menu
* Fixed "new version check" download

## [v0.8.0](https://github.com/bucanero/apollo-vita/releases/tag/v0.8.0) - 2022-06-25

First public release.

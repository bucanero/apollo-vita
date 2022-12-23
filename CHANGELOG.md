# Changelog

All notable changes to the `apollo-vita` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.1.2](https://github.com/bucanero/apollo-vita/releases/tag/v1.1.2) - 2022-12-24

_dedicated to Leon ~ in loving memory (2009 - 2022)_ :heart:

### Added

* Export noNpDRM licenses to zRIF (`User Tools`)
* New save-game sorting options (`Settings`)
  * by Name, by Title ID

### Misc

* Improved UI pad controls
* Download application data updates from `apollo-patches` repository

## [v1.1.0](https://github.com/bucanero/apollo-vita/releases/tag/v1.1.0) - 2022-10-23

### Added

* Support PSP save-game encryption `mode 3`
* Bulk management for PSP saves
* `VMP` PS1 memcard resigning
* Export `VMP` PS1 memcard to `MCR`
* Import `MCR` PS1 memcard to `VMP`
* Trophy Set data backup
  - Copy trophy folders to External storage
  - Export trophy files to `.zip`
* Online DB: new PSP saves for +300 games
* New PS Vita save patch codes
  - Metal Gear Solid 2 HD: custom encryption
  - Metal Gear Solid 3 HD: custom encryption
  - Resident Evil Revelations 2: unpack
* Custom checksum support
  - Metal Gear Solid 2 HD
  - Metal Gear Solid 3 HD
  - Resident Evil Revelations 2

### Patch Engine

* Updated Apollo patch engine v0.3.0
* Improve patch error handling
* Save Wizard / Game Genie
  * Improve SW code types 9, A
  * Add SW code types 3, 7, B, C, D
* BSD scripts
  * New commands: `copy`, `endian_swap`, `msgbox`
  * New custom hash: `force_crc32`, `mgspw_checksum`
  * Support initial value for `add/wadd/dwadd/wsub`
  * Fix `md5_xor` custom hash
  * Fix little-endian support for decrypters/hashes

## [v1.0.2](https://github.com/bucanero/apollo-vita/releases/tag/v1.0.2) - 2022-07-30

### Added

* Download Online DB saves to `ux0`
* New PSP cheat codes
  - BlazBlue: Calamity Trigger Portable
  - Criminal Girls
  - Summon Night 3
  - Summon Night 4
  - Genso Suikoden: Tsumugareshi Hyakunen no Toki
  - Grand Theft Auto: Vice City Stories
  - The 3rd Birthday
  - Goku Makai-Mura Kai
  - Growlanser IV: Over Reloaded
  - Samurai Dou 2 Portable
  - Grand Knights History
  - Kenka Bancho: Badass Rumble
  - Grand Theft Auto: Liberty City Stories
  - Grand Theft Auto: Chinatown Wars
* Custom checksum support
  - BlazBlue: Calamity Trigger Portable

### Fixed

* Fix PSP save copying to User Storage (`ux0:pspemu/`)
* Fix bug with "Raw Patch file" view
* Fix PSP cheat code values

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
* PSP cheat codes
  - R-TYPE Tactics II
  - Ys vs. Sora no Kiseki
  - Gundam Memories - Memories of Battle
  - Mobile Suit Gundam New Gillen's Ambition
  - Akiba's Trip
  - Mobile Suit Gundam AGE Cosmic Drive
  - Armored Core 3 Portable
  - Armored Core Last Raven Portable
  - Armored Core Silent Line Portable
  - Valhalla Knights 2 Battle Stance
  - ClaDun x2
  - The Legend of Heroes: Trails in the Sky
  - Crisis Core: FINAL FANTASY VII
  - Ace Combat X2 Joint Assault
  - The Legend of Heroes: Zero no Kiseki
  - Gundam Assault Survive
  - Another Century's Episode Portable
  - SD Gundam G GENERATION WORLD
  - Onechanbara SPECIAL
  - Valhalla Knights 2
  - Students of Round: The Eternal Legend
  - Ace Combat X Skies of Deception
  - Castlevania Dracula X Chronicles
  - Assassin's Creed Bloodline
  - Kingdom hearts Birth by sleep
  - R-TYPE Tactics
  - Ys: The Oath in Felghana
  - Ys VII

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

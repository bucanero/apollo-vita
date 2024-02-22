# Changelog

All notable changes to the `apollo-vita` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.4.0](https://github.com/bucanero/apollo-vita/releases/tag/v1.4.0) - 2024-02-24

### Added

* Manage PS1 Virtual Memory Card images (VMC)
  - Supports `.VMP` and external formats (`.MCR`, `.VM1`, `.BIN`, `.VMC`, `.GME`, `.VGS`, `.SRM`, `.MCD`)
  - List, import, and export PS1 saves inside VMC images
  - Import - Supported formats: `.MCS`, `.PSV`, `.PSX`, `.PS1`, `.MCB`, `.PDA`
  - Export - Supported formats: `.MCS`, `.PSV`, `.PSX`
* Proper PSP save resigning using KIRK engine CMD5
  - Uses unique per-console Fuse ID
  - Fixes save ownership in games like Gran Turismo
* Added PSP FuseID dumper tool installer
* Save sort option by Type (Vita/PSP/PS1)
* Online DB: added PS1 saves listing

### Misc

* Updated Apollo Patch Engine to v0.7.0
  - Add `jenkins_oaat`, `lookup3_little2` hash functions
  - Add `camellia_ecb` encryption
  - Add RGG Studio decryption (PS4)
  - Add Dead Rising checksum

## [v1.2.8](https://github.com/bucanero/apollo-vita/releases/tag/v1.2.8) - 2023-11-12

### Added

* Auto-detect `X`/`O` button settings
* Compress `.ISO` files to `.CSO`
* Decompress `.CSO` files to `.ISO`
* Added Save-game Key database for PSP games
* New Vita cheat codes
  - Odin Sphere Leifthrasir (PCSE00899, PCSB00986)
* New PSP cheat codes
  - BlazBlue: Continuum Shift II (ULUS10579, ULES01526)
  - Persona (ULUS10432)
  - Monster Hunter Freedom Unite (ULES01213, ULUS10391)
  - Monster Hunter Portable 2nd G (ULJM05500)
* Custom decryption support
  - Patapon 3 (UCUS98751, UCES01421)
  - Monster Hunter Freedom Unite (ULES01213, ULUS10391)
  - Monster Hunter Portable 2nd G (ULJM05500)
  - Monster Hunter Portable 3rd (ULJM05800)
* New PSP copy-unlock patches
  - InviZimals: Shadow Zone (UCES01411, UCES01581, UCUS98760)
  - InviZimals: The Lost Tribes (UCES01525)
  - SOCOM: Fire Team Bravo 2 (UCUS98645)
* Custom checksum support
  - InviZimals: Shadow Zone (UCES01411, UCES01581, UCUS98760)
  - InviZimals: The Lost Tribes (UCES01525)
  - Monster Hunter Freedom Unite (ULES01213, ULUS10391)
  - Monster Hunter Portable 2nd G (ULJM05500)
  - Monster Hunter Portable 3rd (ULJM05800)

### Fixed

* Fixed PSP save-game Key dumper plugin installation

### Misc

* Network HTTP proxy settings support
* Updated [`apollo-lib`](https://github.com/bucanero/apollo-lib) Patch Engine to v0.6.0
  - Add host callbacks (username, wlan mac, psid, account id)
  - Add `murmu3_32`, `jhash` hash functions
  - Add Patapon 3 PSP decryption
  - Add MGS5 decryption (PS3/PS4)
  - Add Monster Hunter 2G/3rd PSP decryption
  - Add Castlevania:LoS checksum
  - Add Rockstar checksum
  - Fix SaveWizard Code Type C
  - Fix `right()` on little-endian platforms

## [v1.2.4](https://github.com/bucanero/apollo-vita/releases/tag/v1.2.4) - 2023-04-12

### Added

* Hex Editor for save-data files
* Improved internal Web Server (Online DB support)
* User-defined Online DB URL (`Settings`)
* Tool to install/disable PSP key dumper plugin (`User Tools`)

### Fixed

* Fixed bug when applying cheats to PSP save files
* Fixed possible freeze when using the on-screen keyboard

### Misc

* Updated Apollo patch engine v0.4.1
  * Skip search if the pattern was not found
  * Improve code types 9, B, D
  * Add value subtraction support (BSD)

## [v1.2.0](https://github.com/bucanero/apollo-vita/releases/tag/v1.2.0) - 2023-02-04

### Added

* Import Trophies from USB
* Network Tools
  * URL downloader tool (download http/https/ftp/ftps links)
  * Simple local Web Server (full access to console drives)
* Improve External storage selection (`uma0`, `imc0`, `ux0`)
* On-screen Keyboard (for text input)
* Save account owner details to `owners.xml`
* Support PSP keys dumped with SGKeyDumper v1.5+

### Fixed

* Fixed a bug when importing decrypted data files

## [v1.1.2](https://github.com/bucanero/apollo-vita/releases/tag/v1.1.2) - 2022-12-24

_dedicated to Leon ~ in loving memory (2009 - 2022)_ :heart:

### Added

* Export noNpDRM licenses to zRIF (`User Tools`)
* New save-game sorting options (`Settings`)
  * by Name, by Title ID
* Show Vita IP address when running Apollo's Web Server

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

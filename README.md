# Apollo Save Tool (PS Vita)

[![Downloads][img_downloads]][app_downloads] [![Release][img_latest]][app_latest] [![License][img_license]][app_license]
[![Build app package](https://github.com/bucanero/apollo-vita/actions/workflows/build.yml/badge.svg)](https://github.com/bucanero/apollo-vita/actions/workflows/build.yml)
![PSV](https://img.shields.io/badge/-PS%20Vita-003791?style=flat&logo=PlayStation)
[![Twitter](https://img.shields.io/twitter/follow/dparrino?label=Follow)](https://twitter.com/dparrino)

**Apollo Save Tool** is an application to manage save-game files on the PlayStation Vita.

This homebrew app allows you to download, unlock, patch and resign save-game files directly on your Vita.

![image](./docs/screenshots/screenshot-main.jpg)

**Comments, ideas, suggestions?** You can [contact me](https://github.com/bucanero/) on [Twitter](https://twitter.com/dparrino) and on [my website](http://www.bucanero.com.ar/).

# Features

* **Easy to use:** no advanced setup needed.
* **Standalone:** no computer required, everything happens on the PS Vita.
* **Automatic settings:** auto-detection of User ID, and Account-ID settings.
* **Multi-format:** supports PS Vita saves, PSP saves (Adrenaline), and PS1 saves.

## Vita & PSP Save Management

* **Save files listing:** quick access to all the save files on USB and the internal PS Vita memory (+ file details)
* **Save param.sfo updating:** allows the user to update the `param.sfo` User ID and Account ID information.
* **Save files patching:** complete support for Save Wizard and [Bruteforce Save Data](https://bruteforcesavedata.forumms.net/) cheat patches to enhance your save-games.
* **Save import/export:** allows the user to decrypt and export save files, and import decrypted saves from other consoles.
* **Save downloading:** easy access to an Online Database of save-game files to download straight to your Vita.

## PS1 Virtual Memory Card Management

* **VMC saves listing:** quick access to all save files on Virtual Memory Cards images.
  - Supported VMC formats: `.VMP`, `.MCR`, `.VM1`, `.BIN`, `.VMC`, `.GME`, `.VGS`, `.SRM`, `.MCD`
* **Import saves to VMC:** enables importing saves (`.MCS`, `.PSV`, `.PSX`, `.PS1`, `.MCB`, `.PDA` formats) to VMCs from other tools and consoles.
* **Export VMC saves:** allows the user to export saves on VMC images to `.MCS`/`.PSV`/`.PSX` formats.
* **Delete VMC saves:** remove any PS1 save file stored on VMC images.

# Download

Get the [latest version here][app_latest].

## Changelog

See the [latest changes here][changelog].

# Donations

My GitHub projects are open to a [sponsor program](https://patreon.com/dparrino). If you feel that my tools helped you in some way or you would like to support it, you can consider a [PayPal donation](https://www.paypal.me/bucanerodev).

# Setup instructions

No special setup is needed. Just download the latest [`apollo-vita.vpk`](https://github.com/bucanero/apollo-vita/releases/latest/download/apollo-vita.vpk) package and install it on your PlayStation Vita.
On first run, the application will detect and setup the required user settings.

## Data folders

### PS Vita

| PS Vita | Folder |
|-----|--------|
| **External Storage saves** | your saves must be stored on `<uma0/imc0/xmc0/ux0>:data/savegames/`. |
| **User Storage saves** | save-games will be scanned from `ux0:user/00/savedata/`. |

### PSP

| PSP | Folder |
|-----|--------|
| **External Storage saves** | your saves must be stored on `<uma0/imc0/xmc0/ux0>:data/savegames/`. |
| **User Storage saves** | save-games will be scanned from `ux0:pspemu/PSP/SAVEDATA/`. |

### PS1

| PS1 | Folder |
|-----|--------|
| **External saves** | your saves must be stored on `<uma0/imc0/xmc0/ux0>:data/PS1/SAVEDATA/`. |
| **Virtual Memory Cards** | VMC images will be scanned from `<uma0/imc0/xmc0/ux0>:data/PS1/VMC/`. |

## PSP Saves Requirements

If you want to properly hash and resign PSP saves, you need to dump the `FuseID` from Adrenaline.
You can install the [FuseID dumper tool](https://github.com/bucanero/psp-fuseid-dumper/) using Apollo:
1. `Tools` :arrow_right: `PSP Key Dumper tools` :arrow_right: `Install PSP FuseID Dumper` option
2. Open Adrenaline and execute the _**FuseID dumper**_ application.
3. Once the `FuseID.bin` has been dumped, Apollo will detect and import the file to use it when needed.

### Save-game Key dumper

To decrypt PSP save files, game-specific save keys are required.
You can use Apollo to install and enable the PSP [save-game key dumper plugin](https://github.com/bucanero/psptools/releases/download/20220719/pspsgkey13.zip) on Adrenaline:
- `Tools` :arrow_right: `PSP Key Dumper tools` :arrow_right: `Install Save-game Key Dumper` option

**Note:** You can also dump the required keys using PSP plugins, such as:
- [SGKeyDumper](https://github.com/bucanero/psptools/releases/download/20220719/pspsgkey13.zip)
- [SGDeemer](https://github.com/bucanero/psptools/releases/download/20220719/SGDeemer111.rar)

1. Install the plugin on Adrenaline (`ux0:pspemu/seplugins`)
2. Enable it using the recovery menu. 
3. Start your PSP game and let it load/save so the plugin can dump the key.
4. Once the key has been dumped, Apollo will detect it, and use it as needed to decrypt, encrypt, apply patches, or rehash the PSP save.

**Tip:** if you have PSP save keys, use Apollo's `Export Save-game Key` option on your PSP save, and then share the `gamekeys.txt` file so all these keys can be added to the next release.

# Usage

Using the application is simple and straight-forward: 

 - Move <kbd>UP</kbd>/<kbd>DOWN</kbd> to select the save-game file you want to patch, and press ![X button](https://github.com/bucanero/pkgi-ps3/raw/master/data/CROSS.png). The patch screen will show the available fixes for the file. Select the patches and click `Apply`.
 - To view the item's details, press ![Triangle](https://github.com/bucanero/pkgi-ps3/raw/master/data/TRIANGLE.png).
It will open the context menu on the screen. Press ![O button](https://github.com/bucanero/pkgi-ps3/raw/master/data/CIRCLE.png) to return to the list.
 - To reload the list, press ![Square](https://github.com/bucanero/pkgi-ps3/raw/master/data/SQUARE.png).
 - Press <kbd>L1</kbd>/<kbd>L2</kbd> or <kbd>R1</kbd>/<kbd>R2</kbd> trigger buttons to move pages up or down.

# Online Database

The application also provides direct access to the [Apollo online database](https://github.com/bucanero/apollo-saves) of save-game files for PlayStation Vita and PSP games.
These usually offer additional features such as completed games that can save you many hours of playing.

The Online Database project aims to [add more save-games](https://github.com/bucanero/apollo-saves/issues/new/choose) shared by the community.

**Note:** Downloaded save files **must be resigned** using Apollo before loading them in your games.

# FAQs

 1. Where I can get a save-game for *XYZ game*?
    
    You can check sites like [Brewology.com](https://ps3.brewology.com/gamesaves/savedgames.php?page=savedgames&system=ps4), and [GameFAQs](https://gamefaqs.gamespot.com/ps4/). Also, searching on [Google](http://www.google.com) might help.
 1. I have a save-game file that I want to share. How can I upload it?
    
    If you have a save file that is not currently available on the Online Database and want to share it, please check [this link](https://github.com/bucanero/apollo-saves) for instructions.
 1. Why is it called **Apollo**?
    
    [Apollo](https://en.wikipedia.org/wiki/Apollo) was the twin brother of [Artemis](https://en.wikipedia.org/wiki/Artemis), goddess of the hunt. Since this project was born using the [Artemis-GUI](https://github.com/Dnawrkshp/ArtemisPS3/) codebase, I decided to respect that heritage by calling it Apollo.

# Credits

* [Bucanero](http://www.bucanero.com.ar/): [Project developer](https://github.com/bucanero)

## Acknowledgments

* [Dnawrkshp](https://github.com/Dnawrkshp/): [Artemis PS3](https://github.com/Dnawrkshp/ArtemisPS3)
* [Berion](https://www.psx-place.com/members/berion.1431/): GUI design
* [flatz](https://github.com/flatz): [SFO tools](https://github.com/bucanero/pfd_sfo_tools/)
* Draan/[Proxima](https://github.com/ProximaV): [KIRK engine](https://github.com/ProximaV/kirk-engine-full)
* [ShendoXT](https://github.com/ShendoXT): [MemcardRex](https://github.com/ShendoXT/memcardrex)
* [aldostools](https://aldostools.org/): [Bruteforce Save Data](https://bruteforcesavedata.forumms.net/)
* [Nobody/Wild Light](https://github.com/nobodo): [Background music track](https://github.com/bucanero/apollo-vita/blob/main/data/haiku.s3m)

# Building

You need to have installed:

- [Vita SDK](https://github.com/vitasdk/)
- [Apollo](https://github.com/bucanero/apollo-lib) library
- [polarSSL](https://github.com/vitasdk/packages/tree/master/polarssl) library
- [cURL](https://github.com/vitasdk/packages/tree/master/curl) library
- [libxmp-lite](https://github.com/vitasdk/packages/tree/master/libxmp-lite) library
- [libZip](https://github.com/vitasdk/packages/tree/master/libzip) library
- [dbglogger](https://github.com/bucanero/dbglogger) library

Run `cmake . && make` to create a release build. If you want to include the [latest save patches](https://github.com/bucanero/apollo-patches) in your `.vpk` file, run `make createzip`.

To enable debug logging, pass `-DAPOLLO_ENABLE_LOGGING=ON` argument to cmake. The application will send debug messages to
UDP multicast address `239.255.0.100:30000`. To receive them you can use [socat][] on your computer:

    $ socat udp4-recv:30000,ip-add-membership=239.255.0.100:0.0.0.0 -

You can also set the `PSVITAIP` environment variable to your Vita's IP address, and use `make send` to upload `eboot.bin` directly to the `ux0:app/NP0APOLLO` folder.

# License

[Apollo Save Tool](https://github.com/bucanero/apollo-vita/) (PS Vita) - Copyright (C) 2020-2025 [Damian Parrino](https://twitter.com/dparrino)

This program is free software: you can redistribute it and/or modify
it under the terms of the [GNU General Public License][app_license] as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

[socat]: http://www.dest-unreach.org/socat/
[app_downloads]: https://github.com/bucanero/apollo-vita/releases
[app_latest]: https://github.com/bucanero/apollo-vita/releases/latest
[app_license]: https://github.com/bucanero/apollo-vita/blob/main/LICENSE
[changelog]: https://github.com/bucanero/apollo-vita/blob/main/CHANGELOG.md
[img_downloads]: https://img.shields.io/github/downloads/bucanero/apollo-vita/total.svg?maxAge=3600
[img_latest]: https://img.shields.io/github/release/bucanero/apollo-vita.svg?maxAge=3600
[img_license]: https://img.shields.io/github/license/bucanero/apollo-vita.svg?maxAge=2592000

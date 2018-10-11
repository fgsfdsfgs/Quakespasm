# QuakespasmNX
Switch port of [Quakespasm](http://quakespasm.sourceforge.net/).
Uses SDL2 as the backend, supports OGG and MP3 music and most other features of Quakespasm.

## Building
You need:
* latest versions of devkitA64 and libnx;
* switch-sdl2, switch-zlib, switch-mesa, switch-libdrm_nouveau.

You can install the libraries with (dkp-)pacman.

Run make -f Makefile.nx in the `quakespasm` directory to build quakespasmnx.nro.

## Running
Place the NRO into `/switch/quakespasm/` on your SD card.

If you only have the [shareware version of Quake](ftp://ftp.idsoftware.com/idstuff/quake/quake106.zip), copy `pak0.pak` to `/switch/quakespasm/id1/`.
If you have the full version, copy both `pak0.pak` and `pak1.pak` to `/switch/quakespasm/id1/`.

If you want to play Scourge of Armagon, copy `pak0.pak` from `hipnotic` to `/switch/quakespasm/hipnotic/`.
If you want to play Dissolution of Eternity, copy `pak0.pak` from `rogue` to `/switch/quakespasm/rogue/`.

Make sure the pak files and the `id1` directory have all-lowercase names, just in case.

You can run QuakespasmNX using Homebrew Launcher.

If it crashes, look for `error.log` in `/switch/quakespasm/`.

Mission Packs and mods are supported. If you have more game folders than just `id1` in `/switch/quakespasm/`, a mod select menu will pop up when you launch QuakespasmNX.

Music should be placed into a `music` subdirectory inside the appropriate game directory (e.g. Quake music in `id1/music/`, Mission Pack 1 music in `hipnotic/music/`).

For more information see [the other README](https://github.com/fgsfdsfgs/Quakespasm/blob/master/quakespasm/Quakespasm.txt).

## Credits
* Quakespasm was made by:
  * Ozkan
  * Eric
  * Sander
  * Stevenaaus
  * based on FitzQuake by Fitzgibbons
  * and probably others;
* fincs, Armada651, Subv and probably others for their work on the OpenGL/mesa/libdrm stuff;
* id Software for Quake.

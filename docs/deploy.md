# Deploy: ds-patcher

## Player route (v2.0.0 and later)

Put `OpenPak-DS-Patcher.nds` on the flashcart or DSi/3DS SD card (any folder; TWiLight Menu++ lists it
like a game), put your own dumped `.nds` games on the same card, start the patcher, choose a game and
select **OpenPak**. Select **Nintendo** to go back. The untouched game stays next to the patched one as
`<game>.nds.original` and `<game>.nds.openpak` records what was written. Choosing OpenPak again, or a newer
patcher, asks `OpenPak update available. Install it?`; No asks `Switch back to the official network?`.

Patched games talk to `*.openpak.org` through the console's normal DNS (no DNS change on the DS) and use
plain `http://`, because the DS cannot verify TLS.

## What it patches (developer appendix)

Port of `Program.cs` (AdmiralCurtiss WfcPatcher, GPL-3.0). In ARM9, ARM7 and both overlay tables it
replaces `https://` with `http://` and `nintendowifi.net` with `openpak.org` (at most 16 characters, the length
of the original). References for the container format: the cartridge header and ARM9/overlay layout on
[DSiBrew](http://dsibrew.org/wiki/DSi_Cartridge_Header) and the
[DS ROM structure notes](http://www.crackerscrap.com/docs/dsromstructure.html), as cited in `Program.cs`.
Compressed ARM9 and overlays use bottom-LZ (CUE's `blz.c`, ported to `nds/source/blz.cpp`); a patched binary
is recompressed to its original size, the new size is written back into the ARM9 size field and the secure
area, and the secure-area CRC16 (0x6C) and header CRC16 (0x15E) are recomputed. The encoder finds matches through a hash
chain instead of the brute-force scan, but takes the same match, so its output is byte-identical to the PC
tool (checked on synthetic ROMs with compressed and plain ARM9, compressed and plain overlays).

How DS homebrew gets at ROMs on-console: flashcart and DSi SD homebrew read the card with libfat
([libnds](https://libnds.devkitpro.org/)), and TWiLight Menu++ launches `.nds` files from the same card
([TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu)). No on-console WFC patcher exists upstream
(nds-bootstrap has none), and a runtime hook is not possible without a Wi-Fi driver there
([nds-bootstrap issue 628](https://github.com/DS-Homebrew/nds-bootstrap/issues/628)), so the ROM file itself is patched.

## Build and test

`nds/`: `docker.io/devkitpro/devkitarm`, `make -C nds APP_VERSION=x.y.z` gives `OpenPak-DS-Patcher.nds`.
Host test of the patch / backup / restore flow on synthetic ROMs: `nds/tests/run.sh` (g++, python3).
Differential check against the PC tool: build the C# sources with a modern `dotnet`, patch a copy of a ROM
with each and `cmp` the results.

## PC tool (reference)

`WfcPatcher.exe -d openpak.org game.nds` writes `game (openpak.org).nds` next to the original. The PC tool
stays as the reference implementation and a developer route; players use the on-console app.

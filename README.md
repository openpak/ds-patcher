# DS Patcher

OpenPak's DS Patcher.

| | |
| --- | --- |
| Console | Nintendo DS |
| Kind | On-console DS app (`OpenPak-DS-Patcher.nds`) plus the original PC tool (.NET Framework 3.5) |
| Based on | [AdmiralCurtiss/WfcPatcher](https://github.com/AdmiralCurtiss/WfcPatcher) (GPL-3.0-or-later) |
| Licence | GPL-3.0-or-later, see [LICENSE](LICENSE) |

## What it does

- `OpenPak-DS-Patcher.nds` runs on the DS (flashcart, DSi SD, TWiLight Menu++): pick a dumped game, select OpenPak or Nintendo, the original is kept as `<name>.nds.original` and restored exactly.

- Patches dumped Nintendo DS game files to connect to a custom server, such as AltWfc.
- Drag and drop dumped `.nds` files onto the executable to patch them.
- `-d, --domain` points the NWFC URLs to a different domain (up to 16 characters, the length of `nintendowifi.net`).

## Install

Copy `OpenPak-DS-Patcher.nds` and your own game dumps to the card and run it (see [deploy](docs/deploy.md)). Developer route with the PC tool:

Drag and drop your dumped NDS games onto the executable, or run:

```
WfcPatcher [options] game1.nds [game2.nds] [...]

  -d, --domain example.com
```

## Build

See [deploy](docs/deploy.md).

## Docs

- [Protocol](docs/protocol.md)
- [Deploy](docs/deploy.md)
- [Provenance](docs/provenance.md)

## Licence

See [LICENSE](LICENSE).

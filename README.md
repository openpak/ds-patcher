# DS Patcher

OpenPak's DS Patcher.

| | |
| --- | --- |
| Console | Nintendo DS |
| Kind | Desktop tool (.NET Framework 3.5 executable) |
| Based on | [AdmiralCurtiss/WfcPatcher](https://github.com/AdmiralCurtiss/WfcPatcher) (GPL-3.0-or-later) |
| Licence | GPL-3.0-or-later, see [LICENSE](LICENSE) |

## What it does

- Patches dumped Nintendo DS game files to connect to a custom server, such as AltWfc.
- Drag and drop dumped `.nds` files onto the executable to patch them.
- `-d, --domain` points the NWFC URLs to a different domain (up to 16 characters, the length of `nintendowifi.net`).

## Install

Drag and drop your dumped NDS games onto the executable, or run:

```
WfcPatcher [options] game1.nds [game2.nds] [...]

  -d, --domain example.com
```

## Build

See [deploy](docs/deploy.md).

## Docs

- [Protocol](docs/protocol.md)
- [Evidence](docs/evidence.md)
- [Deploy](docs/deploy.md)
- [Notes](docs/notes.md)
- [Upstream README](docs/upstream.md)

## Licence

See [LICENSE](LICENSE).

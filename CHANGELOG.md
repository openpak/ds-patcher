# Changelog — OpenPak DS patcher

Fork of the upstream DS patcher; only OpenPak's changes are listed. `git log` stays the
source of truth.

## v2.0.0 — 2026-11-05

- On-console OpenPak DS Patcher (`nds/`, devkitARM): the Switch Patcher's flow and wording, status pill,
  `<name>.nds.original` backup, exact restore, update prompt. Port of the PC tool's patch logic; output matched
  the PC tool byte for byte on synthetic ROMs. Not hardware tested.
- `docs/deploy.md` written (player route and developer appendix); release workflow added.

## v1.6.1 — 2026-11-04

- Document the OpenPak DS patcher fork and its release scope
- The per-console patchers renamed; docs trimmed to README + protocol + deploy

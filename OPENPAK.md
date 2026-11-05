# OpenPak DS Patcher

OpenPak fork of [AdmiralCurtiss/WfcPatcher](https://github.com/AdmiralCurtiss/WfcPatcher),
licensed under GPL-3.0 (see LICENSE). Upstream history, attribution and source
are preserved. The origin repository is `openpak/ds-patcher`;
`upstream` tracks AdmiralCurtiss with pushing disabled locally.

The `nds/` directory is an on-console DS app that does the same patching on the card; the
root C# sources are the PC reference tool. Upstream accepts `--domain` to
select a replacement WFC domain. The existing project targets .NET Framework
3.5; see the upstream README and project file for its original interface.

Preserve the license and authors. Release validation covers OpenPak endpoint
rewriting, SSL handling, supported games, unchanged original-file backup
behavior and a repeatable build.

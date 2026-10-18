# OpenPak Nintendo DS WFC patcher

OpenPak fork of [AdmiralCurtiss/WfcPatcher](https://github.com/AdmiralCurtiss/WfcPatcher),
licensed under GPL-3.0 (see LICENSE). Upstream history, attribution and source
are preserved. The origin repository is `openpak/nn-wfc-patcher-ds`;
`upstream` tracks AdmiralCurtiss with pushing disabled locally.

This is a desktop tool that patches dumped Nintendo DS game files, rather
than an application installed on the DS. Upstream accepts `--domain` to
select a replacement WFC domain. The existing project targets .NET Framework
3.5; see the upstream README and project file for its original interface.

Fork created on 18 October 2026. No OpenPak build has been released and no
DS hardware/game compatibility is claimed. Before release, validate OpenPak
endpoint rewriting, SSL handling, supported games, unchanged original-file
backup behavior and a repeatable build. Preserve the license and authors.

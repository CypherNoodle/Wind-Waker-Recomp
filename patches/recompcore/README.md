# Historical RecompCore patches

These patches record BlueWake's RecompCore changes as they were made. They are history, not a build
input: the series starts at 0008 (0001-0007 were never exported), so it does not apply to the
upstream base 5c3611e, and the local head it led to (3476998) was never published.

The build uses the published fork instead: https://github.com/chrissotraidis/RecompCore, branch
`bluewake`, commit 2d6063614a9bc899f6b4d11c7e7b3cd66e4d96f3. It contains the changes here (through 0097;
some were revised by later ones), the files that were never committed on the development Mac,
and the DolRecomp submodule
pointing at https://github.com/chrissotraidis/DolRecomp (5c91d6e). The Builder fetches it at the commit pinned in
`scripts/builder/profiles/bluewake.sh`; see docs/status/DEVICE_BUILD.md.

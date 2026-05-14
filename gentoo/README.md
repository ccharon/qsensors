# Gentoo Packaging Staging

This directory keeps the Gentoo packaging files in-tree until they are copied
to an overlay.

## Layout

The package files follow the normal Portage path layout:

`util-misc/qsensors/qsensors-0.80.1.ebuild`

## Usage

1. Copy `gentoo/util-misc/qsensors/` into your overlay at:
   `/var/db/repos/<overlay>/util-misc/qsensors/`
2. In that overlay directory, generate Manifest:
   `ebuild qsensors-0.80.1.ebuild manifest`
3. Emerge:
   `emerge -av =util-misc/qsensors-0.80.1`

## Notes

- `SRC_URI` in the ebuild is set to a release tarball URL placeholder.
- Adjust `SRC_URI`, `KEYWORDS`, and package metadata as needed before copying.

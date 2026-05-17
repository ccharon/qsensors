# Gentoo Packaging Staging

This directory keeps the Gentoo packaging files in-tree until they are copied
to an overlay.

## Layout

The package files follow the normal Portage path layout:

`sys-apps/qsensors/qsensors-0.80.7.ebuild`

## Usage

1. Copy `gentoo/sys-apps/qsensors/` into your overlay at:
   `/var/db/repos/<overlay>/sys-apps/qsensors/`
   - Important: this must include the `files/` subdirectory, especially
     `files/qsensors.desktop`.
2. In that overlay directory, generate Manifest:
   `ebuild qsensors-0.80.7.ebuild manifest`
3. Emerge:
   `emerge -av =sys-apps/qsensors-0.80.7`

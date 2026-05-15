# Copyright 1999-2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit cmake desktop xdg

DESCRIPTION="Qt6/Wayland-oriented sensor monitor with xsensors-inspired UI"
HOMEPAGE="https://github.com/ccharon/qsensors"
SRC_URI="https://github.com/ccharon/qsensors/archive/refs/tags/${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="GPL-2+"
SLOT="0"
KEYWORDS="~amd64"
IUSE=""
RESTRICT="mirror"

DEPEND="
	dev-qt/qtbase:6[gui,widgets]
	sys-apps/lm-sensors
"
RDEPEND="${DEPEND}"
BDEPEND="
	dev-build/cmake
	dev-qt/qttools:6[linguist]
	virtual/pkgconfig
"

src_install() {
	cmake_src_install

	newicon -s 256 resources/icons/xsensors.png qsensors.png
	domenu "${FILESDIR}"/qsensors.desktop
}

pkg_postinst() {
	xdg_pkg_postinst
}

pkg_postrm() {
	xdg_pkg_postrm
}

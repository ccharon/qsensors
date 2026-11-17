# Copyright 1999-2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit cmake xdg

DESCRIPTION="Qt6/Wayland-oriented sensor monitor with xsensors-inspired UI"
HOMEPAGE="https://github.com/ccharon/qsensors"
SRC_URI="https://github.com/ccharon/qsensors/archive/refs/tags/v${PV}.tar.gz -> ${P}.tar.gz"

LICENSE="GPL-2+"
SLOT="0"
KEYWORDS="~amd64 ~arm64"
IUSE="test"
RESTRICT="mirror !test? ( test )"

DEPEND="
	>=dev-qt/qtbase-6.8:6[gui,widgets]
	sys-apps/lm-sensors
"
RDEPEND="${DEPEND}"
BDEPEND="
	dev-qt/qttools:6[linguist]
	virtual/pkgconfig
"

src_configure() {
	local mycmakeargs=(
		-DBUILD_TESTING=$(usex test)
		# The toolchain sets the hardening flags (PIE, SSP, _FORTIFY_SOURCE, RELRO).
		-DQSENSORS_HARDENING=OFF
	)
	cmake_src_configure
}

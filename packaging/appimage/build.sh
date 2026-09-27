#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds dist/qsensors-<VERSION>-<arch>.AppImage inside a release container.
# The container's glibc is the minimum glibc of the AppImage, see release.yml.
#
# Usage (from the repository root): VERSION=0.80.10 packaging/appimage/build.sh
set -euo pipefail

: "${VERSION:?VERSION must be set, e.g. VERSION=0.80.10}"
QT_VERSION="6.8.3"
AQTINSTALL_VERSION="3.3.0"
CMAKE_SPEC="cmake>=4.2,<4.3"
LINUXDEPLOY_VERSION="1-alpha-20251107-1"
LINUXDEPLOY_PLUGIN_QT_VERSION="1-alpha-20250213-1"
LINUXDEPLOY_PLUGIN_APPIMAGE_VERSION="1-alpha-20250213-1"

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)
        QT_HOST="linux"; QT_ARCH="linux_gcc_64"
        LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
        PLUGIN_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
        PLUGIN_APPIMAGE_SHA256="992d502a248e14ab185448ddf6f6e7d25558cb84d4623c354c3af350c25fccb3"
        ;;
    aarch64)
        QT_HOST="linux_arm64"; QT_ARCH="linux_gcc_arm64"
        LINUXDEPLOY_SHA256="620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff"
        PLUGIN_QT_SHA256="bf1c24aff6d749b5cf423afad6f15abd4440f81dec1aab95706b25f6667cdcf1"
        PLUGIN_APPIMAGE_SHA256="83c292149274965a865dcd44c135cfca8ba28c6b7de3eb628d4b8b5f248af17c"
        ;;
    *)
        echo "unsupported architecture: $ARCH" >&2
        exit 1
        ;;
esac

ROOT="$(pwd)"
WORK="$ROOT/.appimage-work"
# aqtinstall drops the "linux_" prefix for the directory: gcc_64, gcc_arm64.
QT_DIR="$WORK/qt/$QT_VERSION/${QT_ARCH#linux_}"
mkdir -p "$WORK" "$ROOT/dist"

# Runtime libraries are installed too: linuxdeploy bundles what the Qt plugins
# (xcb, wayland) load, taking them from the build system.
install_system_packages() {
    if command -v dnf >/dev/null; then
        dnf install -y epel-release
        dnf install -y --enablerepo=powertools \
            lm_sensors-devel mesa-libGL-devel mesa-libEGL-devel libxkbcommon-devel libxkbcommon-x11 \
            libxcb xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm \
            libX11 libSM libICE libwayland-client libwayland-cursor libwayland-egl fontconfig freetype dbus-libs glib2 \
            dejavu-sans-fonts desktop-file-utils file patchelf curl
        PYTHON="/opt/python/cp312-cp312/bin/python3"
    elif command -v apt-get >/dev/null; then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends \
            build-essential python3 python3-venv curl ca-certificates file patchelf desktop-file-utils \
            libsensors-dev libgl-dev libegl-dev libxkbcommon-dev libxkbcommon-x11-0 \
            libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 \
            libxcb-xinerama0 libxcb-randr0 libxcb-xkb1 libx11-xcb1 libsm6 libice6 \
            libwayland-client0 libwayland-cursor0 libwayland-egl1 libfontconfig1 libfreetype6 libdbus-1-3 \
            libglib2.0-0 fonts-dejavu-core
        PYTHON="python3"
    else
        echo "neither dnf nor apt-get found" >&2
        exit 1
    fi
}

install_build_tools() {
    "$PYTHON" -m venv "$WORK/venv"
    "$WORK/venv/bin/pip" install --quiet "$CMAKE_SPEC" ninja "aqtinstall==$AQTINSTALL_VERSION"
    export PATH="$WORK/venv/bin:$PATH"
}

install_qt() {
    # CI restores $WORK/qt from its cache.
    if [ -x "$QT_DIR/bin/qmake" ]; then
        echo "using cached Qt in $QT_DIR"
        return
    fi
    aqt install-qt "$QT_HOST" desktop "$QT_VERSION" "$QT_ARCH" \
        --archives qtbase qtwayland qtsvg qttools qttranslations icu \
        --outputdir "$WORK/qt"
}

download_checked() {
    local url="$1" file="$2" sha="$3"
    curl --fail --location --silent --show-error --retry 3 --retry-delay 2 -o "$file" "$url"
    echo "$sha  $file" | sha256sum -c -
    chmod +x "$file"
}

install_linuxdeploy() {
    cd "$WORK"
    download_checked "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-${ARCH}.AppImage" \
        "linuxdeploy-${ARCH}.AppImage" "$LINUXDEPLOY_SHA256"
    download_checked "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_PLUGIN_QT_VERSION}/linuxdeploy-plugin-qt-${ARCH}.AppImage" \
        "linuxdeploy-plugin-qt-${ARCH}.AppImage" "$PLUGIN_QT_SHA256"
    download_checked "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/${LINUXDEPLOY_PLUGIN_APPIMAGE_VERSION}/linuxdeploy-plugin-appimage-${ARCH}.AppImage" \
        "linuxdeploy-plugin-appimage-${ARCH}.AppImage" "$PLUGIN_APPIMAGE_SHA256"
    cd "$ROOT"
}

build_and_test() {
    cmake -S "$ROOT" -B "$WORK/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_PREFIX_PATH="$QT_DIR" -DBUILD_TESTING=ON
    cmake --build "$WORK/build" -j"$(nproc)"
    ctest --test-dir "$WORK/build" --output-on-failure
    rm -rf "$WORK/AppDir"
    DESTDIR="$WORK/AppDir" cmake --install "$WORK/build" --strip
}

package_appimage() {
    cd "$WORK"
    # Containers have no FUSE; the tools unpack themselves instead of mounting.
    export APPIMAGE_EXTRACT_AND_RUN=1
    export QMAKE="$QT_DIR/bin/qmake"
    export LD_LIBRARY_PATH="$QT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    # Bundle Wayland next to the default xcb plugin; Qt picks one at startup.
    export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so"
    local linuxdeploy="./linuxdeploy-${ARCH}.AppImage"

    "$linuxdeploy" --appdir AppDir \
        --desktop-file AppDir/usr/share/applications/qsensors.desktop \
        --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/qsensors.png \
        --plugin qt

    # linuxdeploy-plugin-qt skips the Wayland helper plugins; without xdg-shell no
    # window can be opened under Wayland. Their RPATH ($ORIGIN/../../lib) fits AppDir.
    local dir
    for dir in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
        cp -r "$QT_DIR/plugins/$dir" AppDir/usr/plugins/
        "$linuxdeploy" --appdir AppDir --deploy-deps-only "AppDir/usr/plugins/$dir"
    done

    LDAI_OUTPUT="$ROOT/dist/qsensors-${VERSION}-${ARCH}.AppImage" "$linuxdeploy" --appdir AppDir --output appimage
    cd "$ROOT"
}

verify_bundle() {
    local appdir="$WORK/AppDir"
    for pattern in 'libsensors.so*' 'libqxcb.so' 'libqwayland-generic.so' 'libxdg-shell.so'; do
        if ! find "$appdir" -name "$pattern" | grep -q .; then
            echo "missing in AppDir: $pattern" >&2
            exit 1
        fi
    done
    test -s "$ROOT/dist/qsensors-${VERSION}-${ARCH}.AppImage"
}

install_system_packages
install_build_tools
install_qt
install_linuxdeploy
build_and_test
package_appimage
verify_bundle
echo "built dist/qsensors-${VERSION}-${ARCH}.AppImage"

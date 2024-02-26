#!/bin/bash

########################################################################
# Install build-time and run-time dependencies
########################################################################

export DEBIAN_FRONTEND=noninteractive
export APPIMAGE_EXTRACT_AND_RUN=1

########################################################################
# Build Inkscape and install to appdir/
########################################################################

mkdir -p build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_BINRELOC=ON \
-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_C_COMPILER_LAUNCHER=ccache \
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ninja
DESTDIR="$PWD/appdir" ninja install ; find appdir/
cp ./appdir/usr/share/icons/hicolor/256x256/apps/org.inkscape.Inkscape.png ./appdir/
sed -i -e 's|^Icon=.*|Icon=org.inkscape.Inkscape|g' ./appdir/usr/share/applications/org.inkscape.Inkscape.desktop # FIXME

########################################################################
# Generate AppImage
########################################################################

goappimage="appimagetool-817-x86_64.AppImage"
wget "https://github.com/probonopd/go-appimage/releases/download/continuous/$goappimage"
echo 9e105293cfcea52d11b3e541ce78eb5998c96dca63e486f23da18adce04d6c95 "$goappimage" | sha256sum -c
chmod +x "$goappimage"

# Can't use goappimage for second step since internal copy of appstreamcli is too old
appimagekit="appimagetool-x86_64.AppImage"
wget "https://github.com/AppImage/AppImageKit/releases/download/continuous/$appimagekit"
echo b90f4a8b18967545fda78a445b27680a1642f1ef9488ced28b65398f2be7add2 "$appimagekit" | sha256sum -c
chmod +x "$appimagekit"

"./$goappimage" -s deploy ./appdir/usr/share/applications/org.inkscape.Inkscape.desktop
sed -i -e 's|/usr/lib/x86_64-linux-gnu/gdk-pixbuf-.*/.*/loaders/||g' ./appdir/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders.cache
cp ./appdir/usr/share/icons/hicolor/256x256/apps/org.inkscape.Inkscape.png ./appdir
ARCH=x86_64 "./$appimagekit" ./appdir

sha="$(git rev-parse --short HEAD)"
mv Inkscape*.AppImage* "../Inkscape-$sha.AppImage"

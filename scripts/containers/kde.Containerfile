# Test environment: KDE Plasma 6 (KWin Wayland) on Fedora, run nested in Xvfb.
FROM registry.fedoraproject.org/fedora:44
RUN dnf -y install --setopt=install_weak_deps=False \
        cmake ninja-build gcc-c++ pkgconf-pkg-config \
        qt6-qtbase-devel qt6-qtwayland qt6-qtsvg glib2-devel libxcb-devel xcb-util-keysyms-devel \
        gstreamer1-devel gstreamer1-plugins-base-devel gstreamer1-plugins-good pipewire-gstreamer \
        kwin-wayland kglobalacceld xdg-desktop-portal xdg-desktop-portal-kde plasma-workspace-libs \
        pipewire wireplumber \
        xorg-x11-server-Xvfb xdpyinfo xdotool ImageMagick dbus-daemon dbus-tools wl-clipboard \
        mesa-dri-drivers mesa-libEGL mesa-libgbm dejavu-sans-fonts procps-ng which \
    && dnf clean all

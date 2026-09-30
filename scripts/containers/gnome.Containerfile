# Test environment: GNOME 50 (the Ubuntu 26.04 generation) on Fedora, with
# gnome-shell's devkit viewer shown in Xvfb.
FROM registry.fedoraproject.org/fedora:44
RUN dnf -y install --setopt=install_weak_deps=False \
        cmake ninja-build gcc-c++ pkgconf-pkg-config \
        qt6-qtbase-devel qt6-qtwayland qt6-qtsvg glib2-devel libxcb-devel xcb-util-keysyms-devel \
        gstreamer1-devel gstreamer1-plugins-base-devel gstreamer1-plugins-good pipewire-gstreamer \
        gnome-shell mutter mutter-devkit gnome-settings-daemon gsettings-desktop-schemas \
        xdg-desktop-portal xdg-desktop-portal-gnome pipewire wireplumber \
        xorg-x11-server-Xvfb xdpyinfo xdotool ImageMagick dbus-daemon dbus-tools wl-clipboard \
        mesa-dri-drivers mesa-libEGL mesa-libgbm dejavu-sans-fonts procps-ng which \
    && dnf clean all

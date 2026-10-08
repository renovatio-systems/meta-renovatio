# Image installations
IMAGE_INSTALL:append = " \
    networkmanager \
    openssh \
    openssh-sftp-server \
    cjson \
    curl \
    openssl-dev \
    wpa-supplicant \
    nm-wifi-client \
    rfkill \
    waveshare-epaper-2in15g \
    waveshare-epaper-2in15g-examples \
    renovatio-waveshare-clock \
    finnhub-stock-tracker \
"

# Leave out the upstream test packagegroup
IMAGE_INSTALL:remove = "packagegroup-rpi-test"

# Remove connman dependency from packagegroup-rpi-test
IMAGE_INSTALL:remove = "connman"
PACKAGE_EXCLUDE:append = " connman connman-client"

# Install opkg and the package database on the target
IMAGE_FEATURES += "package-management"

# Disable wpa_supplicant standalone service
do_install:append() {
    rm -f ${D}${systemd_system_unitdir}/wpa_supplicant*.service
    rm -f ${D}${systemd_system_unitdir}/wpa_supplicant@*.service
}

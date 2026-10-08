SUMMARY = "WiFi client configuration for NetworkManager"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Set these in local.conf / site.conf (kept out of version control), e.g.:
#   RPI_WIFI_SSID = "MyNetwork"
#   RPI_WIFI_PSK = "supersecret"
# Do NOT hardcode real credentials here - this recipe is tracked in git.
RPI_WIFI_SSID ?= ""
RPI_WIFI_PSK ?= ""

python () {
    if not d.getVar("RPI_WIFI_SSID") or not d.getVar("RPI_WIFI_PSK"):
        bb.fatal("RPI_WIFI_SSID and RPI_WIFI_PSK must be set (e.g. in local.conf) to build nm-wifi-client")
}

do_install() {
    # Create main NetworkManager config (disable ifupdown plugin that interferes)
    install -d ${D}${sysconfdir}/NetworkManager
    cat > ${D}${sysconfdir}/NetworkManager/NetworkManager.conf << 'EOF'
[main]
plugins=keyfile
dhcp=dhclient

[device]
wifi.scan-rand-mac-address=no
EOF

    # Create WiFi connection file
    install -d ${D}${sysconfdir}/NetworkManager/system-connections
    cat > ${D}${sysconfdir}/NetworkManager/system-connections/${RPI_WIFI_SSID}.nmconnection << EOF
[connection]
id=${RPI_WIFI_SSID}
type=wifi
interface-name=wlan0
autoconnect=yes

[wifi]
mode=infrastructure
ssid=${RPI_WIFI_SSID}

[wifi-security]
key-mgmt=wpa-psk
psk=${RPI_WIFI_PSK}

[ipv4]
method=auto

[ipv6]
method=auto
EOF
    chmod 600 ${D}${sysconfdir}/NetworkManager/system-connections/${RPI_WIFI_SSID}.nmconnection
}

FILES:${PN} = "${sysconfdir}/NetworkManager/*"

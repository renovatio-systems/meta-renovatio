SUMMARY = "Command-line stock monitor using the Finnhub API"
DESCRIPTION = "Reads tickers, purchase price and share count from a file, \
fetches live quotes and company profiles from Finnhub and prints a \
gain/loss table. Runs on the HDMI console (tty1) in place of the login \
prompt once the network is up. Config lives in /etc/finnhub-stock-tracker; \
until api.txt is copied there the screen shows the board's IP address."
HOMEPAGE = "https://github.com/renovatio-systems/finnhub-stock-tracker"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=9d4d5d5794b703ddcb73ccbf66d34dc1"

# The upstream third_party submodules (curl, cJSON) are deliberately not
# fetched; the patch builds against the system libraries instead.
SRC_URI = "git://github.com/renovatio-systems/finnhub-stock-tracker.git;branch=main;protocol=https \
           file://0001-CMakeLists-use-system-libcurl-and-cJSON-via-pkg-conf.patch \
           file://0002-monitor-allow-default-ticker-and-API-key-paths-at-build.patch \
           file://finnhub-stock-tracker-launcher \
           file://finnhub-stock-tracker.service \
           file://finnhub-stock-tracker.default \
           "
SRCREV = "3ea0e7b056b2f5921ba1c09a04a5fc082f352614"
PV = "1.0+git"

S = "${WORKDIR}/git"

DEPENDS = "curl cjson"

inherit cmake pkgconfig systemd

EXTRA_OECMAKE = "-DSTOCK_MONITOR_CONFIG_DIR=${sysconfdir}/finnhub-stock-tracker"

do_install:append() {
    install -d ${D}${sysconfdir}/finnhub-stock-tracker
    install -m 0644 ${S}/tickers.txt ${D}${sysconfdir}/finnhub-stock-tracker/

    install -d ${D}${sysconfdir}/default
    install -m 0644 ${WORKDIR}/finnhub-stock-tracker.default \
        ${D}${sysconfdir}/default/finnhub-stock-tracker

    install -m 0755 ${WORKDIR}/finnhub-stock-tracker-launcher ${D}${bindir}/

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/finnhub-stock-tracker.service \
        ${D}${systemd_system_unitdir}/

    # The monitor owns tty1, so mask the login prompt there. Other VTs and
    # the serial console keep theirs.
    install -d ${D}${sysconfdir}/systemd/system
    ln -sf /dev/null ${D}${sysconfdir}/systemd/system/getty@tty1.service
    ln -sf /dev/null ${D}${sysconfdir}/systemd/system/autovt@tty1.service
}

SYSTEMD_SERVICE:${PN} = "finnhub-stock-tracker.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

CONFFILES:${PN} = " \
    ${sysconfdir}/finnhub-stock-tracker/tickers.txt \
    ${sysconfdir}/default/finnhub-stock-tracker \
"

# HTTPS to finnhub.io needs CA certificates. The launcher uses ip, awk and
# logger, which busybox provides.
RDEPENDS:${PN} = "ca-certificates busybox"

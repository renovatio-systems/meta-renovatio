SUMMARY = "e-Paper clock and weather display for Waveshare 4-colour panels"
DESCRIPTION = "Shows the time, date and current weather (from Open-Meteo) \
on a Waveshare 4-colour e-Paper panel. Display type, location and \
timezone are set in /etc/renovatio-waveshare/clock.json. Runs as a \
systemd service."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src \
           file://clock.json \
           file://renovatio-waveshare-clock.service \
           "

S = "${WORKDIR}/src"

DEPENDS = "renovatio-waveshare-lib cjson curl freetype"

COMPATIBLE_MACHINE = "^rpi$"

inherit pkgconfig systemd

EXTRA_OEMAKE = "PREFIX=${prefix} BINDIR=${bindir}"

do_install() {
    oe_runmake install DESTDIR=${D}

    install -d ${D}${sysconfdir}/renovatio-waveshare
    install -m 0644 ${WORKDIR}/clock.json ${D}${sysconfdir}/renovatio-waveshare/

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/renovatio-waveshare-clock.service \
        ${D}${systemd_system_unitdir}/
}

SYSTEMD_SERVICE:${PN} = "renovatio-waveshare-clock.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

CONFFILES:${PN} = "${sysconfdir}/renovatio-waveshare/clock.json"

# HTTPS needs CA certificates, named timezones need tzdata, and the
# default fonts are DejaVu Sans (regular and bold).
RDEPENDS:${PN} = "ca-certificates tzdata ttf-dejavu-sans"

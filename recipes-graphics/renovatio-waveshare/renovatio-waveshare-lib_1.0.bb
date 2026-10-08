SUMMARY = "Display library for Waveshare 4-colour (G) e-Paper panels"
DESCRIPTION = "Static C library providing a framebuffer, drawing, bitmap \
and FreeType text API on top of the upstream Waveshare e-Paper drivers. \
The panel type (e.g. 2in15g) is chosen at runtime."
HOMEPAGE = "https://github.com/waveshare/e-Paper"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302 \
                    file://waveshare/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in15g.c;beginline=1;endline=29;md5=3f2ec85f1b6c13aa4e9102fd03d04364"

SRC_URI = "file://src \
           git://github.com/waveshare/e-Paper;branch=master;protocol=https;destsuffix=src/waveshare \
           file://0001-DEV_Config-dont-hard-fail-on-unrecognized-etc-issue.patch;patchdir=waveshare \
           "
SRCREV = "a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc"

S = "${WORKDIR}/src"

DEPENDS = "bcm2835 freetype"

inherit pkgconfig

COMPATIBLE_MACHINE = "^rpi$"

EXTRA_OEMAKE = " \
    WAVESHARE_DIR=${S}/waveshare/RaspberryPi_JetsonNano/c \
    PREFIX=${prefix} \
    LIBDIR=${libdir} \
    INCLUDEDIR=${includedir} \
"

do_install() {
    oe_runmake install DESTDIR=${D}
}

# Static library only: headers, .pc and .a land in ${PN}-dev and
# ${PN}-staticdev. Programs link it in, so nothing is needed at runtime.
ALLOW_EMPTY:${PN} = "1"
RDEPENDS:${PN}-dev = ""
RDEPENDS:${PN}-staticdev += "bcm2835-staticdev freetype-dev"

SUMMARY = "Waveshare 2.15inch e-Paper HAT+ (G) driver library and examples"
DESCRIPTION = "C driver library and demo program for the Waveshare \
2.15inch e-Paper HAT+ (G) - a 4-color (black/white/yellow/red) \
e-paper display for the Raspberry Pi. Python examples from upstream \
are not built or packaged."
HOMEPAGE = "https://www.waveshare.com/2.15inch-e-paper-hat-plus-g.htm"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in15g.c;beginline=1;endline=29;md5=3f2ec85f1b6c13aa4e9102fd03d04364"

SRC_URI = "git://github.com/waveshare/e-Paper;branch=master;protocol=https \
           file://0001-DEV_Config-dont-hard-fail-on-unrecognized-etc-issue.patch \
           "
SRCREV = "a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc"

S = "${WORKDIR}/git"

DEPENDS = "bcm2835"

COMPATIBLE_MACHINE = "^rpi$"

EPDDIR = "${S}/RaspberryPi_JetsonNano/c"
EPDLIB = "libwaveshare-epd2in15g.a"

# Upstream's "RPI" target doesn't order its RPI_DEV/RPI_epd sub-targets
# against each other, so it isn't safe to build with -jN.
PARALLEL_MAKE = ""

EPDPICDIR = "${datadir}/waveshare-epaper/pic"

do_configure() {
    # The demo loads its image from "./pic/...", relative to the current
    # directory, which only works when run from the source tree. Point it
    # at the installed copy instead.
    sed -i 's|"\./pic/|"${EPDPICDIR}/|g' ${EPDDIR}/examples/EPD_2in15g_test.c
}

do_compile() {
    # CFLAGS must stay a plain exported env var, not a `make` command-line
    # argument: upstream's Makefile injects the display-model define via
    # "CFLAGS += ... -D $(EPD)", and GNU Make blocks *all* makefile-side
    # changes (including +=) to a variable that was set on the command
    # line. Only CC needs the command-line override, since the Makefile
    # plain-assigns "CC = gcc" outright rather than appending to it.
    export CFLAGS="${CFLAGS} ${LDFLAGS}"

    oe_runmake -C ${EPDDIR} RPI \
        EPD=epd2in15g \
        USELIB_RPI=USE_BCM2835_LIB \
        CC="${CC}"

    # Archive the driver/support objects into a reusable static library,
    # separate from the demo's own main()/test entry point and image data.
    ${AR} rcs ${EPDDIR}/${EPDLIB} \
        $(find ${EPDDIR}/bin -name '*.o' \
            ! -name 'main.o' ! -name 'EPD_2in15g_test.o' \
            ! -name 'ImageData.o' ! -name 'ImageData2.o')
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${EPDDIR}/epd ${D}${bindir}/epd2in15g-demo

    install -d ${D}${EPDPICDIR}
    install -m 0644 ${EPDDIR}/pic/2in15g.bmp ${D}${EPDPICDIR}/

    install -d ${D}${libdir}
    install -m 0644 ${EPDDIR}/${EPDLIB} ${D}${libdir}/${EPDLIB}

    install -d ${D}${includedir}/waveshare-epaper
    install -m 0644 ${EPDDIR}/lib/e-Paper/EPD_2in15g.h ${D}${includedir}/waveshare-epaper/
    install -m 0644 ${EPDDIR}/lib/Config/DEV_Config.h ${D}${includedir}/waveshare-epaper/
    install -m 0644 ${EPDDIR}/lib/GUI/GUI_Paint.h ${D}${includedir}/waveshare-epaper/
    install -m 0644 ${EPDDIR}/lib/GUI/GUI_BMPfile.h ${D}${includedir}/waveshare-epaper/
    install -m 0644 ${EPDDIR}/lib/Fonts/fonts.h ${D}${includedir}/waveshare-epaper/
}

# Keep this to just our two packages plus their debug counterparts -
# otherwise OE's default auto-splitting would claim the headers/*.a into
# -dev/-staticdev ahead of ${PN} before our FILES assignments below ever
# get a look in. The -dbg packages are still needed so split debug symbols
# (e.g. /usr/bin/.debug/epd2in15g-demo) have somewhere to go.
PACKAGES = "${PN}-examples-dbg ${PN}-examples ${PN}-dbg ${PN}"

FILES:${PN} = "${libdir}/${EPDLIB} ${includedir}/waveshare-epaper"
FILES:${PN}-examples = "${bindir}/epd2in15g-demo ${EPDPICDIR}"

RDEPENDS:${PN}-examples = "${PN}"

INSANE_SKIP:${PN} = "staticdev"

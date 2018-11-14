DESCRIPTION = "gstreamer dvbmediasink plugin"
SECTION = "multimedia"
PRIORITY = "optional"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=7fbc338309ac38fefcd64b04bb903e34"

inherit externalsrc

S = "${FILE_DIRNAME}"

PV = "1.0"
PR = "r0"

do_unpack[noexec] = "1"
do_populate_sysroot[noexec] = "1"
do_populate_lic[noexec] = "1"
do_packagedata[noexec] = "1"
do_package_write_ipk[noexec] = "1"
do_rm_work[noexec] = "1"
do_rm_work_all[noexec] = "1"

inherit autotools pkgconfig

FILES_${PN} = "${libdir}/gstreamer-1.0/*.so*"
FILES_${PN}-dev += "${libdir}/gstreamer-1.0/*.la ${libdir}/gstreamer-1.0/*.a"
FILES_${PN}-dbg += "${libdir}/gstreamer-1.0/.debug"

PACKAGE_ARCH = "${MACHINE_ARCH}"

EXTRA_OECONF = "--with-wma --with-wmv --with-pcm --with-dtsdownmix --with-h265 --with-vb9 --with-eac3"

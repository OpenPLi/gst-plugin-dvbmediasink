AC_INIT

dnl versions of gstreamer and plugins-base
AC_ARG_WITH(gstversion,
	AS_HELP_STRING([--with-gstversion],[use gstreamer version (major.minor)]),
	[GST_MAJORMINOR=$withval],[GST_MAJORMINOR=0.10])

GST_REQUIRED=0.10.0
GSTPB_REQUIRED=0.10.0

dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease

dnl when going to/from release please set the nano correctly !
dnl releases only do Wall, cvs and prerelease does Werror too
AS_VERSION(gst-plugin, GST_PLUGIN_VERSION, $GST_MAJORMINOR, 0, 1, GST_PLUGIN_CVS="no", GST_PLUGIN_CVS="yes")

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

dnl make aclocal work in maintainer mode
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_LIBTOOL

dnl check for libm
AC_CHECK_LIBM
AC_SUBST(LIBM)

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer


PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl append GST_ERROR cflags to GST_CFLAGS
GST_CFLAGS="$GST_CFLAGS $GST_ERROR"

dnl make GST_CFLAGS and GST_LIBS available
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the base class libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer base class libraries found (gstreamer-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-plugins-base libraries
PKG_CHECK_MODULES(GSTPB_BASE,
                  gstreamer-plugins-base-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTPB_BASE=yes, HAVE_GSTPB_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTPB_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Plugins Base libraries found (gstreamer-plugins-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTPB_BASE_CFLAGS)
AC_SUBST(GSTPB_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-controller libraries
PKG_CHECK_MODULES(GSTCTRL,
                  gstreamer-controller-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTCTRL=yes, HAVE_GSTCTRL=no)

dnl Give a warning if we don't have gstreamer-controller
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTCTRL" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Controller libraries found (gstreamer-controller-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTCTRL_CFLAGS)
AC_SUBST(GSTCTRL_LIBS)

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-$GST_MAJORMINOR/plugins"
else
  plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_ARG_WITH(mpeg4,
	AS_HELP_STRING([--with-mpeg4],[support mpeg4, yes or no]),
	[have_mpeg4=$withval],[have_mpeg4=yes])
if test "$have_mpeg4" = "yes"; then
	AC_DEFINE([HAVE_MPEG4],[1],[Define to 1 for mpeg4 support])
fi

AC_ARG_WITH(h264,
	AS_HELP_STRING([--with-h264],[support h264, yes or no]),
	[have_h264=$withval],[have_h264=yes])
if test "$have_h264" = "yes"; then
	AC_DEFINE([HAVE_H264],[1],[Define to 1 for h264 support])
fi

AC_ARG_WITH(h263,
	AS_HELP_STRING([--with-h263],[support h263, yes or no]),
	[have_h263=$withval],[have_h263=yes])
if test "$have_h263" = "yes"; then
	AC_DEFINE([HAVE_H263],[1],[Define to 1 for h263 support])
fi

AC_ARG_WITH(mpeg4v2,
	AS_HELP_STRING([--with-mpeg4v2],[support mpeg4v2 (divx, xvid, msmpeg, 3ivx) yes or no]),
	[have_mpeg4v2=$withval],[have_mpeg4v2=yes])
if test "$have_mpeg4v2" = "yes"; then
	AC_DEFINE([HAVE_MPEG4V2],[1],[Define to 1 for mpeg4v2 support])
fi

AC_ARG_WITH(limited-mpeg4v2,
	AS_HELP_STRING([--with-limited-mpeg4v2],[limited (800x600) mpeg4v2, yes or no]),
	[have_limited_mpeg4v2=$withval],[have_limited_mpeg4v2=no])
if test "$have_limited_mpeg4v2" = "yes"; then
	AC_DEFINE([HAVE_LIMITED_MPEG4V2],[1],[Define to 1 for limited (800x600) mpeg4v2 support])
fi

AC_ARG_WITH(wmv,
	AS_HELP_STRING([--with-wmv],[support wmv, yes or no]),
	[have_wmv=$withval],[have_wmv=no])
if test "$have_wmv" = "yes"; then
	AC_DEFINE([HAVE_WMV],[1],[Define to 1 for wmv support])
fi

AC_ARG_WITH(mp3,
	AS_HELP_STRING([--with-mp3],[support mp3, yes or no]),
	[have_mp3=$withval],[have_mp3=yes])
if test "$have_mp3" = "yes"; then
	AC_DEFINE([HAVE_MP3],[1],[Define to 1 for mp3 support])
fi

AC_ARG_WITH(dts,
	AS_HELP_STRING([--with-dts],[support dts, yes or no]),
	[have_dts=$withval],[have_dts=yes])
if test "$have_dts" = "yes"; then
	AC_DEFINE([HAVE_DTS],[1],[Define to 1 for dts support])
fi

AC_ARG_WITH(lpcm,
	AS_HELP_STRING([--with-lpcm],[support LPCM, yes or no]),
	[have_lpcm=$withval],[have_lpcm=yes])
if test "$have_lpcm" = "yes"; then
	AC_DEFINE([HAVE_LPCM],[1],[Define to 1 for LPCM support])
fi

AC_ARG_WITH(wma,
	AS_HELP_STRING([--with-wma],[support wma, yes or no]),
	[have_wma=$withval],[have_wma=no])
if test "$have_wma" = "yes"; then
	AC_DEFINE([HAVE_WMA],[1],[Define to 1 for wma support])
fi

AC_ARG_WITH(amr,
	AS_HELP_STRING([--with-amr],[support AMR, yes or no]),
	[have_amr=$withval],[have_amr=no])
if test "$have_amr" = "yes"; then
	AC_DEFINE([HAVE_AMR],[1],[Define to 1 for AMR support])
fi

AC_ARG_WITH(pcm,
	AS_HELP_STRING([--with-pcm],[support PCM, yes or no]),
	[have_pcm=$withval],[have_pcm=no])
if test "$have_pcm" = "yes"; then
	AC_DEFINE([HAVE_PCM],[1],[Define to 1 for PCM support])
fi

AC_ARG_WITH(eac3,
	AS_HELP_STRING([--with-eac3],[support E-AC3 / AC3+ / DDP, yes or no]),
	[have_eac3=$withval],[have_eac3=no])
if test "$have_eac3" = "yes"; then
	AC_DEFINE([HAVE_EAC3],[1],[Define to 1 for E-AC3 support])
fi

AC_ARG_WITH(dtsdownmix,
	AS_HELP_STRING([--with-dtsdownmix],[build dts downmix element, yes or no]),
	[have_dtsdownmix=$withval],[have_dtsdownmix=no])
if test "$have_dtsdownmix" = "yes"; then
	AC_DEFINE([HAVE_DTSDOWNMIX],[1],[Define to 1 for DTS downmix support])
fi
DTS_LIBS="-ldca $LIBM"
AC_SUBST(DTS_LIBS)
AM_CONDITIONAL(HAVE_DTSDOWNMIX, test "$have_dtsdownmix" = "yes")

AC_OUTPUT(Makefile)

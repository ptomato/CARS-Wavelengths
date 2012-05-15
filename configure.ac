dnl Process with autoconf to create a configure script

AC_PREREQ(2.61)
AC_INIT([CARS Wavelengths], [1.0], [philip.chimento@gmail.com])
AM_INIT_AUTOMAKE([-Wall foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([main.c])
AC_PROG_CC
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([PERL], [perl])
AC_PATH_PROG([GDK_PIXBUF_CSOURCE], [gdk-pixbuf-csource],
	AC_MSG_ERROR([gdk-pixbuf-csource not found.]))
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h])
AC_C_CONST
PKG_CHECK_MODULES([CARS_WAVELENGTHS], [gtk+-3.0])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT


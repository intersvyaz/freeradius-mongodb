AC_PREREQ([2.53])
AC_INIT(rlm_mongodb.c)
AC_REVISION($Revision$)
AC_DEFUN(modname,[rlm_mongodb])

m4_include([/usr/share/aclocal/pkg.m4])
PKG_PROG_PKG_CONFIG([0.25])

if test x$with_[]modname != xno; then

    AC_PROG_CC

    LIBMONGOC_CFLAGS=
    LIBMONGOC_LDFLAGS=
    PKG_CHECK_MODULES([LIBMONGOC], [libmongoc-1.0],[ fail=; ],[ fail="libmongoc-1.0" ])

    targetname=modname

else
    targetname=
    echo \*\*\* module modname is disabled.
fi

if test x"$fail" != x""; then
    if test x"${enable_strict_dependencies}" = x"yes"; then
        AC_MSG_ERROR([set --without-]modname[ to disable it explicitly.])
    else
        AC_MSG_WARN([silently not building ]modname[.])
        AC_MSG_ERROR([FAILURE: ]modname[ requires: $fail.]);
        targetname=""
    fi
fi

mod_ldflags="$LIBMONGOC_LIBS"
mod_cflags="$LIBMONGOC_CFLAGS"

AC_SUBST(mod_ldflags)
AC_SUBST(mod_cflags)
AC_SUBST(targetname)
AC_OUTPUT(all.mk)

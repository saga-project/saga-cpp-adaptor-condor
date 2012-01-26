
#
# SYNOPSIS
#
#   AX_SAGA_CHECK_CONDOR([MINIMUM-VERSION])
#
# DESCRIPTION
#
#   Test for the CONDOR libraries of a particular version (or newer)
#
#   If no path to the installed condor library is given,
#   the macro searchs under /usr, /usr/local, /opt, /usr/condor,
#   /usr/local/condor, /opt/condor, and /usr/local/package/condor-*
#   CONDOR_LOCATION and CONDOR_CONFIG are evaluated, in that order.
#
#   This macro calls:
#
#     AC_SUBST(SAGA_HAVE_CONDOR)
#     AC_SUBST(CONDOR_LOCATION)
#     AC_SUBST(CONDOR_VERSION) 
#     AC_SUBST(CONDOR_CONFIG) 
#     AC_SUBST(CONDOR_PATH) 
#     AC_SUBST(CONDOR_BIN_VERSION) 
#     AC_SUBST(CONDOR_BIN_Q) 
#     AC_SUBST(CONDOR_BIN_SUBMIT) 
#     AC_SUBST(CONDOR_BIN_RM) 
#
# LAST MODIFICATION
#
#   2008-09-26
#
# COPYLEFT
#
#   Copyright (c) 2007 Andre Merzky      <andre@merzky.net>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AX_SAGA_CHECK_CONDOR],
[
  AC_ARG_VAR([CONDOR_LOCATION],[condor installation directory])
  AC_ARG_VAR([CONDOR_CONFIG],[condor configuration file])

  SAGA_HAVE_CONDOR=no

  tmp_location=""
  AC_ARG_WITH([condor],
              AS_HELP_STRING([--with-condor=DIR],
              [use condor (default is YES) at DIR (optional)]),
              [
              if test "$withval" = "no"; then
                want_condor="no"
              elif test "$withval" = "yes"; then
                want_condor="yes"
                tmp_location=""
              else
                want_condor="yes"
                tmp_location="$withval"
              fi
              ],
              [want_condor="yes"])

  # use CONDOR_LOCATION and CONDOR_CONFIG if avaialble, and if not 
  # overwritten by --with-condor=<dir>

  if test "x$CONDOR_CONFIG" != "x"; then
    condor_config_root=`echo $CONDOR_CONFIG | sed -e 's/\/etc\/condor_config$//'`
  fi

  if test "x$want_condor" = "xyes"; then
    
    packages=`ls /usr/local/package/condor-* 2>>/dev/null`

    if test "$tmp_location-$CONDOR_LOCATION-$condor_config_root" = "--"; then
      paths="/usr /usr/local /opt /sw $packages /usr/condor /usr/local/condor /opt/condor"
    else
      paths="$tmp_location $CONDOR_LOCATION $condor_config_root"
    fi

    for tmp_path in $paths; do
      
      AC_MSG_CHECKING(for condor in $tmp_path)
    
      test -x $tmp_path/bin/condor_version && CONDOR_BIN_VERSION=$tmp_path/bin/condor_version
      test -x $tmp_path/bin/condor_q       && CONDOR_BIN_Q=$tmp_path/bin/condor_q
      test -x $tmp_path/bin/condor_submit  && CONDOR_BIN_SUBMIT=$tmp_path/bin/condor_submit
      test -x $tmp_path/bin/condor_rm      && CONDOR_BIN_RM=$tmp_path/bin/condor_rm

      if test "x$CONDOR_BIN_VERSION" != "x"; then
        CONDOR_VERSION=`$CONDOR_BIN_VERSION | head -1 | cut -f 2 -d ' '`
      fi

      if test "x$CONDOR_BIN_VERSION" != "x"; then
        if test "x$CONDOR_BIN_Q" != "x"; then
          if test "x$CONDOR_BIN_SUBMIT" != "x"; then
            if test "x$CONDOR_BIN_RM" != "x"; then
              AC_MSG_RESULT(yes)
              SAGA_HAVE_CONDOR=yes
              CONDOR_LOCATION=$tmp_path
              CONDOR_PATH=$tmp_path/bin
              CONDOR_CONFIG=$tmp_path/etc/condor_config
              break;
            fi
          fi
        fi
      fi

      AC_MSG_RESULT(no)

    done # foreach path

  fi # want_condor


  if  test "$SAGA_HAVE_CONDOR" != "yes"; then

    AC_MSG_WARN([no condor found])

  else

    export CONDOR_BIN_VERSION
    export CONDOR_BIN_Q
    export CONDOR_BIN_SUBMIT
    export CONDOR_BIN_RM

    AC_SUBST(CONDOR_BIN_VERSION)
    AC_SUBST(CONDOR_BIN_Q)
    AC_SUBST(CONDOR_BIN_SUBMIT)
    AC_SUBST(CONDOR_BIN_RM)

  fi


  AC_SUBST(SAGA_HAVE_CONDOR)
  AC_SUBST(CONDOR_LOCATION)
  AC_SUBST(CONDOR_VERSION)
  AC_SUBST(CONDOR_CONFIG)
  AC_SUBST(CONDOR_PATH)

])


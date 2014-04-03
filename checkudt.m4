AC_ARG_WITH(dummy, 
	[],
    dummy="$withval"
)     
AC_ARG_WITH(
    udt, 
    [  --with-udt=DIR   prefix where UDT staff is installed ],
    [
      udt_path="$withval";
      AC_SUBST([UDT_PATH], ["$udt_path"])
    ], 
    [udt_path=no]
)
     
# Checks for UDT library & headers.
AC_MSG_NOTICE()
AC_MSG_NOTICE(Checking for UDT:)
AC_MSG_NOTICE(-----------------)

if test "$udt_path" != "no"; then
      LDFLAGS="$LDFLAGS -L$udt_path/lib"
fi


AC_CHECK_LIB(
    [udt],[udtversion],
    , AC_MSG_ERROR([The UDT library <libudt> not found!]),
    [$LDFLAGS]
)

udt_include_ok="no"

if test "$udt_path" != "no"; then
   udt_include_ok="$udt_path/include"
fi

if test $udt_include_ok != "no" 
  then
    AC_CHECK_FILE(
      ["$udt_include_ok/udt.h"],
      [
        CPPFLAGS="$CPPFLAGS -I$udt_include_ok"
      ],
      [
        AC_MSG_RESULT(not found!)
        AC_MSG_ERROR(The UDT header file <udt.h> not found!)
      ]
    )
  else
    AC_CHECK_HEADERS(
      [udt.h],
      ,
      [AC_MSG_ERROR(The UDT header file <udt.h> not found!)]
    )
fi

#LDFLAGS="$LDFLAGS -static"

AC_MSG_NOTICE()
AC_MSG_NOTICE([UDT library looks like ok :)])
AC_MSG_NOTICE()

INCLUDES =

METASOURCES      = AUTO
bin_PROGRAMS     = udtrelay
udtrelay_SOURCES = udtrelay.cpp  udtrelay.h

udtrelay_DEPENDENCIES = ../lib/libudtgate.a
udtrelay_LDADD        = ../lib/libudtgate.a

../lib/libudtgate.a:
	cd ../lib; make; cd ../relay;

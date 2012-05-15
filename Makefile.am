AM_CFLAGS = $(CARS_WAVELENGTHS_CFLAGS)
bin_PROGRAMS = cars-wavelengths
cars_wavelengths_SOURCES = main.c quantity.c quantity.h oslogo.h interface.h
cars_wavelengths_LDADD = $(CARS_WAVELENGTHS_LIBS)
BUILT_SOURCES = oslogo.h interface.h

oslogo.h: oslogo.png oslogo16.png
	$(AM_V_GEN) $(GDK_PIXBUF_CSOURCE) --build-list \
		oslogo_data oslogo.png \
		oslogo_16_data oslogo16.png \
		>$@

interface.h: interface.xml convert.pl
	$(AM_V_GEN) $(PERL) convert.pl interface.xml >$@

EXTRA_DIST = oslogo.png oslogo16.png interface.xml convert.pl
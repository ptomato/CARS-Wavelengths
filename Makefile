CC=C:/MingW/bin/gcc
CFLAGS=-Wall -O3 -fexpensive-optimizations -mwindows -mms-bitfields \
	-IC:/GTK/include/gtk-2.0 -IC:/GTK/lib/gtk-2.0/include \
	-IC:/GTK/include/atk-1.0 -IC:/GTK/include/cairo \
	-IC:/GTK/include/pango-1.0 -IC:/GTK/include/glib-2.0 \
	-IC:/GTK/lib/glib-2.0/include -IC:/GTK/include/libpng12
LDFLAGS=-LC:/GTK/lib 
LIBS=-lgtk-win32-2.0 -latk-1.0 -lgio-2.0 -lgdk-win32-2.0 -lpangowin32-1.0 \
	-lgdi32 -lpangocairo-1.0 -lpango-1.0 -lcairo -lgdk_pixbuf-2.0 \
	-lgobject-2.0 -lgmodule-2.0 -lglib-2.0 -lintl
SRC=main.c quantity.c
	
all: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) $(LIBS) -o "CARS Wavelengths.exe"

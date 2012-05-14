#!/usr/bin/perl -w

print STDOUT <<END;
#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <glib.h>

G_BEGIN_DECLS

gchar interface_string[] = 
END

while(<>) {
	chomp;
	s/(["\\])/\\$1/g;
	print STDOUT "\"" . $_ . "\\n\"\n";
}

print STDOUT <<END;
;
G_END_DECLS

#endif /* __INTERFACE_H__ */
END

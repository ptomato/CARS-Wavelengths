#!/usr/bin/perl -w

print STDOUT <<END;
#ifndef INTERFACE_H
#define INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

char interface_string[] =
END

while(<>) {
	chomp;
	s/(["\\])/\\$1/g;
	print STDOUT "\"" . $_ . "\\n\"\n";
}

print STDOUT <<END;
;
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INTERFACE_H */
END

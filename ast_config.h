#ifndef AST_CONFIG_H
#define AST_CONFIG_H

/* If we want to include config.h, we need to import asterisk.h first
 * so we can fix the duplicate PACKAGE_* first.
 * Note that this bug did not show itself until trying to compile with
 * include files from *outside* /usr/include. Apparently gcc has a
 * -Wsystem-headers which is *not* enabled by default, causing the
 * difference. */
#include <asterisk.h>

#ifdef HAVE_CONFIG_H

/* For some reason, Asterisk exports PACKAGE_* variables which we don't want.
 * See also: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=733598
 *
 * Simply defining ASTERISK_AUTOCONFIG_H to skip importing of the autoconf.h
 * altogether does not work. It breaks the other Asterisk header files. */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION

#include "config.h"

#endif /* HAVE_CONFIG_H */

#endif /* AST_CONFIG_H */

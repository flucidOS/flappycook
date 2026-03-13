#ifndef PKGINFO_H
#define PKGINFO_H
#include "recipe.h"

/* Fix 2: changed return type from void to int */
int write_pkginfo(Recipe *r, const char *pkgdir);

#endif
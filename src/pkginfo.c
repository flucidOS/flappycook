#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pkginfo.h"

void write_pkginfo(Recipe *r, const char *pkgdir)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/.PKGINFO", pkgdir);

    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen");
        exit(1);
    }

    fprintf(f, "pkgname = %s\n", r->name);
    fprintf(f, "pkgver = %s\n", r->version);
    fprintf(f, "pkgrel = 1\n");
    fprintf(f, "arch = %s\n", r->arch);

    fclose(f);
}
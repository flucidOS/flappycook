#include <stdio.h>
#include <string.h>
#include "recipe.h"

int load_recipe(const char *path, Recipe *r)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("recipe open");
        return -1;
    }

    char line[512];

    while (fgets(line, sizeof(line), f)) {

        if (strncmp(line, "name=", 5) == 0)
            sscanf(line + 5, "%63s", r->name);

        else if (strncmp(line, "version=", 8) == 0)
            sscanf(line + 8, "%31s", r->version);

        else if (strncmp(line, "arch=", 5) == 0)
            sscanf(line + 5, "%31s", r->arch);

        else if (strncmp(line, "source=", 7) == 0)
            sscanf(line + 7, "%511s", r->source);

        else if (strncmp(line, "sha256=", 7) == 0)
            sscanf(line + 7, "%127s", r->sha256);
    }

    fclose(f);
    return 0;
}
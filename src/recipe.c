#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include "recipe.h"

static void trim(char *s)
{
    char *p = s;

    while (*p && (*p == ' ' || *p == '\n' || *p == '"'))
        p++;

    memmove(s, p, strlen(p) + 1);

    for (int i = strlen(s) - 1; i >= 0; i--)
    {
        if (s[i] == '"' || s[i] == '\n' || s[i] == ' ')
            s[i] = 0;
        else
            break;
    }
}

int parse_recipe(const char *path, Recipe *r)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        perror("recipe open");
        return -1;
    }

    memset(r, 0, sizeof(*r));

    char path_copy[512];
    strcpy(path_copy, path);
    strcpy(r->recipe_dir, dirname(path_copy));

    char line[2048];

    while (fgets(line, sizeof(line), f))
    {
        /* Skip comments and blank lines */
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (strncmp(line, "name=", 5) == 0)
        {
            strcpy(r->name, line + 5);
            trim(r->name);
        }
        else if (strncmp(line, "version=", 8) == 0)
        {
            strcpy(r->version, line + 8);
            trim(r->version);
        }
        else if (strncmp(line, "arch=", 5) == 0)
        {
            strcpy(r->arch, line + 5);
            trim(r->arch);
        }
        else if (strncmp(line, "source=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);
            if (strlen(val))
            {
                strcpy(r->sources[r->source_count++], val);
            }
        }
        else if (strncmp(line, "sha256=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);
            if (r->source_count > 0 && strlen(val))
            {
                strcpy(r->checksums[r->source_count - 1], val);
            }
        }
        /* Ignore function bodies - builder sources the recipe file directly */
    }

    fclose(f);
    return 0;
}
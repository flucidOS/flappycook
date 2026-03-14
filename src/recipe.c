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

    for (int i = (int)strlen(s) - 1; i >= 0; i--)
    {
        if (s[i] == '"' || s[i] == '\n' || s[i] == ' ')
            s[i] = '\0';
        else
            break;
    }
}

static int validate_field(const char *field, const char *value)
{
    for (const char *p = value; *p; p++)
    {
        char c = *p;
        if (!( (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               c == '.' || c == '-' || c == '_' ))
        {
            fprintf(stderr,
                "Error: field '%s' contains illegal character '%c'\n",
                field, c);
            return -1;
        }
    }
    return 0;
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
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (strncmp(line, "name=", 5) == 0)
        {
            char *val = line + 5;
            trim(val);
            if (snprintf(r->name, sizeof(r->name), "%s", val)
                    >= (int)sizeof(r->name))
            {
                fprintf(stderr, "Error: name too long\n");
                fclose(f);
                return -1;
            }
            if (validate_field("name", r->name) != 0)
            {
                fclose(f);
                return -1;
            }
        }
        else if (strncmp(line, "version=", 8) == 0)
        {
            char *val = line + 8;
            trim(val);
            if (snprintf(r->version, sizeof(r->version), "%s", val)
                    >= (int)sizeof(r->version))
            {
                fprintf(stderr, "Error: version too long\n");
                fclose(f);
                return -1;
            }
            if (validate_field("version", r->version) != 0)
            {
                fclose(f);
                return -1;
            }
        }
        else if (strncmp(line, "arch=", 5) == 0)
        {
            char *val = line + 5;
            trim(val);
            if (snprintf(r->arch, sizeof(r->arch), "%s", val)
                    >= (int)sizeof(r->arch))
            {
                fprintf(stderr, "Error: arch too long\n");
                fclose(f);
                return -1;
            }
            if (validate_field("arch", r->arch) != 0)
            {
                fclose(f);
                return -1;
            }
        }
        else if (strncmp(line, "source=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);
            if (strlen(val))
            {
                if (r->source_count >= MAX_SOURCES)
                {
                    fprintf(stderr, "Error: too many sources\n");
                    fclose(f);
                    return -1;
                }
                if (snprintf(r->sources[r->source_count],
                             sizeof(r->sources[r->source_count]),
                             "%s", val)
                        >= (int)sizeof(r->sources[r->source_count]))
                {
                    fprintf(stderr, "Error: source URL too long\n");
                    fclose(f);
                    return -1;
                }
                r->checksum_types[r->source_count] = CKSUM_NONE;
                r->subdirs[r->source_count][0] = '\0';
                r->source_count++;
            }
        }
        else if (strncmp(line, "sha256=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);

            if (strlen(val))
            {
                int assigned = 0;
                for (int i = r->source_count - 1; i >= 0; i--)
                {
                    if (r->checksum_types[i] == CKSUM_NONE)
                    {
                        if (snprintf(r->checksums[i],
                                     sizeof(r->checksums[i]),
                                     "%s", val)
                                >= (int)sizeof(r->checksums[i]))
                        {
                            fprintf(stderr,
                                "Error: sha256 value too long\n");
                            fclose(f);
                            return -1;
                        }
                        r->checksum_types[i] = (strcmp(val, "SKIP") == 0)
                                               ? CKSUM_SKIP : CKSUM_SHA256;
                        assigned = 1;
                        break;
                    }
                }
                if (!assigned)
                    fprintf(stderr,
                        "Warning: sha256= has no matching source,"
                        " ignoring\n");
            }
        }
        else if (strncmp(line, "subdir=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);

            if (strlen(val))
            {
                int assigned = 0;
                for (int i = r->source_count - 1; i >= 0; i--)
                {
                    if (strlen(r->subdirs[i]) == 0)
                    {
                        if (snprintf(r->subdirs[i],
                                     sizeof(r->subdirs[i]),
                                     "%s", val)
                                >= (int)sizeof(r->subdirs[i]))
                        {
                            fprintf(stderr,
                                "Error: subdir value too long\n");
                            fclose(f);
                            return -1;
                        }
                        if (validate_field("subdir", r->subdirs[i]) != 0)
                        {
                            fclose(f);
                            return -1;
                        }
                        assigned = 1;
                        break;
                    }
                }
                if (!assigned)
                    fprintf(stderr,
                        "Warning: subdir= has no matching source,"
                        " ignoring\n");
            }
        }
        else if (strncmp(line, "patch=", 6) == 0)
        {
            char *val = line + 6;
            trim(val);
            if (strlen(val))
            {
                if (r->patch_count >= MAX_PATCHES)
                {
                    fprintf(stderr, "Error: too many patches\n");
                    fclose(f);
                    return -1;
                }
                if (snprintf(r->patches[r->patch_count],
                             sizeof(r->patches[r->patch_count]),
                             "%s", val)
                        >= (int)sizeof(r->patches[r->patch_count]))
                {
                    fprintf(stderr, "Error: patch filename too long\n");
                    fclose(f);
                    return -1;
                }
                r->patch_count++;
            }
        }
        else if (strncmp(line, "depend=", 7) == 0)
        {
            char *val = line + 7;
            trim(val);
            if (strlen(val))
            {
                if (r->depend_count >= MAX_DEPENDS)
                {
                    fprintf(stderr, "Error: too many dependencies\n");
                    fclose(f);
                    return -1;
                }
                if (snprintf(r->depends[r->depend_count],
                             sizeof(r->depends[r->depend_count]),
                             "%s", val)
                        >= (int)sizeof(r->depends[r->depend_count]))
                {
                    fprintf(stderr, "Error: dependency name too long\n");
                    fclose(f);
                    return -1;
                }
                if (validate_field("depend", r->depends[r->depend_count]) != 0)
                {
                    fclose(f);
                    return -1;
                }
                r->depend_count++;
            }
        }
    }

    fclose(f);
    return 0;
}
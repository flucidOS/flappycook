#ifndef RECIPE_H
#define RECIPE_H

#define MAX_SOURCES 32

typedef struct {
    char name[64];
    char version[32];
    char arch[32];
    char sources[MAX_SOURCES][512];
    char checksums[MAX_SOURCES][128];
    int  source_count;
    char build[4096];
    char package[4096];
    char recipe_dir[512];
    char build_script[128];  /* Fix 8: bumped from 64 to fit full tmp path */
} Recipe;

int parse_recipe(const char *path, Recipe *r);

#endif
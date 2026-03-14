#ifndef RECIPE_H
#define RECIPE_H

#define MAX_SOURCES  32
#define MAX_PATCHES  16
#define MAX_DEPENDS  64

typedef enum {
    CKSUM_NONE   = 0,
    CKSUM_SHA256 = 1,
    CKSUM_SKIP   = 2,
} CksumType;

typedef struct {
    char      name[64];
    char      version[32];
    char      arch[32];

    char      sources[MAX_SOURCES][512];
    char      checksums[MAX_SOURCES][128];
    CksumType checksum_types[MAX_SOURCES];
    char      subdirs[MAX_SOURCES][128];
    int       source_count;

    char      patches[MAX_PATCHES][512];
    int       patch_count;

    char      depends[MAX_DEPENDS][64];
    int       depend_count;

    char      build[4096];
    char      package[4096];
    char      recipe_dir[512];
    char      build_script[128];
} Recipe;

int parse_recipe(const char *path, Recipe *r);

#endif
#ifndef RECIPE_H
#define RECIPE_H

typedef struct {
    char name[64];
    char version[32];
    char arch[32];
    char source[512];
} Recipe;

int load_recipe(const char *path, Recipe *r);

#endif

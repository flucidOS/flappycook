#include <stdio.h>
#include <string.h>
#include "recipe.h"

int build_package(Recipe *r);

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: flappycook build <recipe>\n");
        return 1;
    }

    if (strcmp(argv[1], "build") != 0) {
        printf("Unknown command\n");
        return 1;
    }

    Recipe r;

    if (load_recipe(argv[2], &r) != 0) {
        printf("Failed to load recipe\n");
        return 1;
    }

    build_package(&r);

    return 0;
}

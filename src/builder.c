#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "recipe.h"

int build_package(Recipe *r)
{
    char cmd[1024];
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    printf("Building %s %s\n", r->name, r->version);

    /* Create directories */
    sprintf(cmd, "mkdir -p sources/%s build/%s pkg/%s/root pkg/%s/.pkg output",
            r->name, r->name, r->name, r->name);

    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create directories\n");
        return 1;
    }

    /* Download source */
    printf("Downloading source...\n");

    sprintf(cmd,
        "curl -L %s -o sources/%s/source.tar.gz",
        r->source,
        r->name);

    if (system(cmd) != 0) {
        fprintf(stderr, "Download failed\n");
        return 1;
    }

    /* Extract source */
    printf("Extracting source...\n");

    sprintf(cmd,
        "tar -xf sources/%s/source.tar.gz -C sources/%s --strip-components=1",
        r->name,
        r->name);

    if (system(cmd) != 0) {
        fprintf(stderr, "Extraction failed\n");
        return 1;
    }

    /* Generate build script */
    printf("Running build() and package()...\n");

    FILE *script = fopen("/tmp/flappycook_build.sh", "w");
    if (!script) {
        perror("script");
        return 1;
    }

    fprintf(script,
        "#!/bin/bash\n"
        "set -e\n"
        "srcdir=\"%s/sources/%s\"\n"
        "builddir=\"%s/build/%s\"\n"
        "pkgdir=\"%s/pkg/%s/root\"\n"
        "\n"
        "mkdir -p \"$builddir\" \"$pkgdir\"\n"
        "cd \"$srcdir\"\n"
        "\n"
        "source \"%s/recipes/%s.recipe\"\n"
        "\n"
        "build\n"
        "package\n",
        cwd, r->name,
        cwd, r->name,
        cwd, r->name,
        cwd, r->name);

    fclose(script);

    if (system("chmod +x /tmp/flappycook_build.sh") != 0) {
        fprintf(stderr, "Failed to make build script executable\n");
        return 1;
    }

    if (system("bash /tmp/flappycook_build.sh") != 0) {
        fprintf(stderr, "Build failed\n");
        return 1;
    }

    /* Generate metadata */
    printf("Generating metadata...\n");

    sprintf(cmd,
        "echo \"name=%s\nversion=%s\narch=%s\" > pkg/%s/.pkg/meta",
        r->name,
        r->version,
        r->arch,
        r->name);

    if (system(cmd) != 0) {
        fprintf(stderr, "Metadata generation failed\n");
        return 1;
    }

    /* Generate file list */
    printf("Generating file list...\n");

    sprintf(cmd,
        "cd pkg/%s/root && find . -type f | sed 's|^.|/|' > ../.pkg/files",
        r->name);

    if (system(cmd) != 0) {
        fprintf(stderr, "File list generation failed\n");
        return 1;
    }

    /* Create final archive */
    printf("Creating package archive...\n");

    sprintf(cmd,
        "cd pkg/%s && tar -I zstd -cf ../../output/%s-%s.pkg.tar.zst .",
        r->name,
        r->name,
        r->version);

    if (system(cmd) != 0) {
        fprintf(stderr, "Package creation failed\n");
        return 1;
    }

    printf("Package created: output/%s-%s.pkg.tar.zst\n",
           r->name, r->version);

    return 0;
}

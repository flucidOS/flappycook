#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "recipe.h"
#include "pkginfo.h"

#define CMD_BUF 1024

/* ----------------------------- */
/* Command execution helper      */
/* ----------------------------- */

static int exec_cmd(const char *cmd)
{
    int status = system(cmd);

    if (status == -1) {
        perror("system");
        return 1;
    }

    if (WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
        return 1;
    }

    return 0;
}

/* ----------------------------- */
/* Extract filename from URL     */
/* ----------------------------- */

static const char *get_basename(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/* ----------------------------- */
/* Fetch all sources             */
/* ----------------------------- */

static int fetch_sources(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Fetching sources...\n");

    for (int i = 0; i < r->source_count; i++) {

        const char *src = r->sources[i];
        const char *base = get_basename(src);

        if (strncmp(src, "http://", 7) == 0 ||
            strncmp(src, "https://", 8) == 0) {

            printf("Downloading %s\n", src);

            snprintf(cmd, sizeof(cmd),
                     "curl -L %s -o sources/%s/%s",
                     src, r->name, base);

            if (exec_cmd(cmd))
                return 1;
        }
        else {

            printf("Copying local source %s\n", src);

            snprintf(cmd, sizeof(cmd),
                     "cp recipes/%s sources/%s/ 2>/dev/null",
                     src, r->name);

            if (exec_cmd(cmd)) {
                fprintf(stderr,
                        "Warning: optional source missing: %s\n",
                        src);
            }
        }
    }

    return 0;
}

/* ----------------------------- */
/* Verify checksums              */
/* ----------------------------- */

static int verify_checksums(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Verifying checksums...\n");

    for (int i = 0; i < r->source_count; i++) {

        if (strcmp(r->checksums[i], "SKIP") == 0)
            continue;

        const char *src = r->sources[i];
        const char *base = get_basename(src);

        snprintf(cmd, sizeof(cmd),
                 "echo \"%s  sources/%s/%s\" | sha256sum -c -",
                 r->checksums[i], r->name, base);

        if (exec_cmd(cmd))
            return 1;
    }

    return 0;
}

/* ----------------------------- */
/* Extract source archive        */
/* ----------------------------- */

static int extract_sources(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Extracting source archive...\n");

    for (int i = 0; i < r->source_count; i++) {

        const char *src = r->sources[i];
        const char *base = get_basename(src);

        if (strstr(base, ".tar.gz") ||
            strstr(base, ".tar.xz") ||
            strstr(base, ".tar.bz2") ||
            strstr(base, ".tgz")) {

            snprintf(cmd, sizeof(cmd),
                     "tar -xf sources/%s/%s -C sources/%s --strip-components=1",
                     r->name, base, r->name);

            return exec_cmd(cmd);
        }
    }

    fprintf(stderr, "No source archive found\n");
    return 1;
}

/* ----------------------------- */
/* Run build() and package()     */
/* ----------------------------- */

static int run_build_script(Recipe *r)
{
    FILE *script;
    char cwd[512];

    printf("Running build() and package()...\n");

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return 1;
    }

    script = fopen("/tmp/flappycook_build.sh", "w");

    if (!script) {
        perror("fopen");
        return 1;
    }

    fprintf(script,
        "#!/bin/bash\n"
        "set -e\n"
        "srcdir=\"%s/sources/%s\"\n"
        "pkgdir=\"%s/pkg/%s\"\n"
        "\n"
        "mkdir -p \"$pkgdir\"\n"
        "cd \"$srcdir\"\n"
        "source \"%s/recipes/%s.recipe\"\n"
        "build\n"
        "package\n",
        cwd, r->name,
        cwd, r->name,   /* no /root suffix — files land directly in pkg/<name>/ */
        cwd, r->name);

    fclose(script);

    if (exec_cmd("chmod +x /tmp/flappycook_build.sh"))
        return 1;

    if (exec_cmd("bash /tmp/flappycook_build.sh"))
        return 1;

    return 0;
}

/* ----------------------------- */
/* Write .PKGINFO                */
/* ----------------------------- */

static int generate_pkginfo(Recipe *r)
{
    char pkgdir[256];

    printf("Generating .PKGINFO...\n");

    snprintf(pkgdir, sizeof(pkgdir), "pkg/%s", r->name);

    write_pkginfo(r, pkgdir);

    return 0;
}

/* ----------------------------- */
/* Generate .FILES list          */
/* ----------------------------- */

static int generate_filelist(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Generating .FILES...\n");

    /*
     * Find all regular files inside pkg/<name>/,
     * strip the leading "./" from paths,
     * exclude .PKGINFO and .FILES themselves,
     * sort deterministically,
     * write to pkg/<name>/.FILES
     */
    snprintf(cmd, sizeof(cmd),
        "cd pkg/%s && "
        "find . -type f "
        "! -name '.PKGINFO' "
        "! -name '.FILES' "
        "| sed 's|^\\./||' "
        "| sort "
        "> .FILES",
        r->name);

    return exec_cmd(cmd);
}

/* ----------------------------- */
/* Create final archive          */
/* ----------------------------- */

static int create_package(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Creating package archive...\n");

    /*
     * Archive everything in pkg/<name>/ from inside that directory
     * so paths in the tarball are relative: usr/bin/hello, .PKGINFO, etc.
     */
    snprintf(cmd, sizeof(cmd),
        "cd pkg/%s && tar -I zstd -cf ../../output/%s-%s.pkg.tar.zst .",
        r->name,
        r->name,
        r->version);

    return exec_cmd(cmd);
}

/* ----------------------------- */
/* Main build pipeline           */
/* ----------------------------- */

int build_package(Recipe *r)
{
    char cmd[CMD_BUF];

    printf("Building %s %s\n", r->name, r->version);

    /* Clean workspace */

    snprintf(cmd, sizeof(cmd),
        "rm -rf sources/%s build/%s pkg/%s",
        r->name, r->name, r->name);

    if (exec_cmd(cmd))
        return 1;

    /* Create directories */

    snprintf(cmd, sizeof(cmd),
        "mkdir -p sources/%s build/%s pkg/%s output",
        r->name, r->name, r->name);

    if (exec_cmd(cmd))
        return 1;

    if (fetch_sources(r))
        return 1;

    if (verify_checksums(r))
        return 1;

    if (extract_sources(r))
        return 1;

    if (run_build_script(r))
        return 1;

    if (generate_pkginfo(r))
        return 1;

    if (generate_filelist(r))
        return 1;

    if (create_package(r))
        return 1;

    printf("Package created: output/%s-%s.pkg.tar.zst\n",
           r->name, r->version);

    return 0;
}
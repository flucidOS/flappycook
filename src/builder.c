#include <stdio.h>
#include <stdarg.h>   /* Fix: needed for va_start, va_end in build_cmd */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "recipe.h"
#include "pkginfo.h"

#define CMD_BUF 4096

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
/* snprintf truncation check     */
/* ----------------------------- */

static int build_cmd(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

static int build_cmd(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= size) {
        fprintf(stderr,
            "Error: command too long, would have been truncated "
            "(%d bytes needed, %zu available)\n", n, size);
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

/*
 * Fix 7: single-quote-escape a string for safe shell interpolation.
 */
static int shell_safe(const char *in, char *out, size_t outsz)
{
    size_t pos = 0;

#define PUTC(c)                          \
    do {                                 \
        if (pos + 1 >= outsz) return 1;  \
        out[pos++] = (c);                \
    } while (0)

    PUTC('\'');
    for (; *in; in++) {
        if (*in == '\'') {
            PUTC('\'');
            PUTC('\\');
            PUTC('\'');
            PUTC('\'');
        } else {
            PUTC(*in);
        }
    }
    PUTC('\'');
    out[pos] = '\0';
    return 0;

#undef PUTC
}

/* ----------------------------- */
/* Fetch all sources             */
/* ----------------------------- */

static int fetch_sources(Recipe *r)
{
    char cmd[CMD_BUF];
    char safe_src[CMD_BUF];
    char safe_name[256];
    char safe_base[512];

    printf("Fetching sources...\n");

    if (shell_safe(r->name, safe_name, sizeof(safe_name))) {
        fprintf(stderr, "Error: package name too long to escape\n");
        return 1;
    }

    for (int i = 0; i < r->source_count; i++) {

        const char *src  = r->sources[i];
        const char *base = get_basename(src);

        if (shell_safe(src,  safe_src,  sizeof(safe_src))  ||
            shell_safe(base, safe_base, sizeof(safe_base))) {
            fprintf(stderr, "Error: source path too long to escape\n");
            return 1;
        }

        if (strncmp(src, "http://", 7) == 0 ||
            strncmp(src, "https://", 8) == 0) {

            printf("Downloading %s\n", src);

            if (build_cmd(cmd, sizeof(cmd),
                          "curl -L %s -o sources/%s/%s",
                          safe_src, safe_name, safe_base))
                return 1;

            if (exec_cmd(cmd))
                return 1;
        }
        else {

            printf("Copying local source %s\n", src);

            if (build_cmd(cmd, sizeof(cmd),
                          "cp recipes/%s sources/%s/ 2>/dev/null",
                          safe_src, safe_name))
                return 1;

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
    char safe_cksum[512];
    char safe_name[256];
    char safe_base[512];

    printf("Verifying checksums...\n");

    if (shell_safe(r->name, safe_name, sizeof(safe_name))) {
        fprintf(stderr, "Error: package name too long to escape\n");
        return 1;
    }

    for (int i = 0; i < r->source_count; i++) {

        if (strcmp(r->checksums[i], "SKIP") == 0)
            continue;

        const char *src  = r->sources[i];
        const char *base = get_basename(src);

        if (shell_safe(r->checksums[i], safe_cksum, sizeof(safe_cksum)) ||
            shell_safe(base,            safe_base,  sizeof(safe_base))) {
            fprintf(stderr, "Error: value too long to escape\n");
            return 1;
        }

        if (build_cmd(cmd, sizeof(cmd),
                      "echo %s  sources/%s/%s | sha256sum -c -",
                      safe_cksum, safe_name, safe_base))
            return 1;

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
    char safe_name[256];
    char safe_base[512];
    int  extracted = 0;

    printf("Extracting source archive...\n");

    if (shell_safe(r->name, safe_name, sizeof(safe_name))) {
        fprintf(stderr, "Error: package name too long to escape\n");
        return 1;
    }

    for (int i = 0; i < r->source_count; i++) {

        const char *src  = r->sources[i];
        const char *base = get_basename(src);

        if (strstr(base, ".tar.gz")  ||
            strstr(base, ".tar.xz")  ||
            strstr(base, ".tar.bz2") ||
            strstr(base, ".tgz")) {

            if (shell_safe(base, safe_base, sizeof(safe_base))) {
                fprintf(stderr,
                    "Error: source name too long to escape\n");
                return 1;
            }

            if (build_cmd(cmd, sizeof(cmd),
                          "tar -xf sources/%s/%s -C sources/%s"
                          " --strip-components=1",
                          safe_name, safe_base, safe_name))
                return 1;

            if (exec_cmd(cmd))
                return 1;

            extracted = 1;
        }
    }

    if (!extracted) {
        fprintf(stderr, "No source archive found\n");
        return 1;
    }

    return 0;
}

/* ----------------------------- */
/* Run build() and package()     */
/* ----------------------------- */

static int run_build_script(Recipe *r)
{
    char cwd[512];

    printf("Running build() and package()...\n");

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return 1;
    }

    /*
     * Fix 8: unique temp script via mkstemp.
     * Use a fixed-length prefix only — avoids format-truncation
     * warning and keeps the path well within build_script[128].
     */
    snprintf(r->build_script, sizeof(r->build_script),
             "/tmp/flappycook_XXXXXX");

    int fd = mkstemp(r->build_script);
    if (fd == -1) {
        perror("mkstemp");
        return 1;
    }

    FILE *script = fdopen(fd, "w");
    if (!script) {
        perror("fdopen");
        close(fd);
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
        cwd, r->name,
        cwd, r->name);

    fclose(script);

    char cmd[CMD_BUF];

    if (build_cmd(cmd, sizeof(cmd),
                  "chmod +x %s", r->build_script))
        return 1;

    if (exec_cmd(cmd))
        return 1;

    if (build_cmd(cmd, sizeof(cmd),
                  "bash %s", r->build_script))
        return 1;

    if (exec_cmd(cmd))
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

    if (build_cmd(pkgdir, sizeof(pkgdir), "pkg/%s", r->name))
        return 1;

    if (write_pkginfo(r, pkgdir) != 0) {
        fprintf(stderr, "Failed to write .PKGINFO\n");
        return 1;
    }

    return 0;
}

/* ----------------------------- */
/* Generate .FILES list          */
/* ----------------------------- */

static int generate_filelist(Recipe *r)
{
    char cmd[CMD_BUF];
    char safe_name[256];

    printf("Generating .FILES...\n");

    if (shell_safe(r->name, safe_name, sizeof(safe_name))) {
        fprintf(stderr, "Error: package name too long to escape\n");
        return 1;
    }

    if (build_cmd(cmd, sizeof(cmd),
        "cd pkg/%s && "
        "find . -type f "
        "! -name '.PKGINFO' "
        "! -name '.FILES' "
        "! -name 'dir' "
        "| sed 's|^\\./||' "
        "| sort "
        "> .FILES",
        safe_name))
        return 1;

    return exec_cmd(cmd);
}

/* ----------------------------- */
/* Create final archive          */
/* ----------------------------- */

static int create_package(Recipe *r)
{
    char cmd[CMD_BUF];
    char safe_name[256];
    char safe_version[128];

    printf("Creating package archive...\n");

    if (shell_safe(r->name,    safe_name,    sizeof(safe_name))    ||
        shell_safe(r->version, safe_version, sizeof(safe_version))) {
        fprintf(stderr,
            "Error: package name/version too long to escape\n");
        return 1;
    }

    if (build_cmd(cmd, sizeof(cmd),
        "cd pkg/%s && tar -I zstd -cf ../../output/%s-%s.pkg.tar.zst .",
        safe_name, safe_name, safe_version))
        return 1;

    return exec_cmd(cmd);
}

/* ----------------------------- */
/* Main build pipeline           */
/* ----------------------------- */

int build_package(Recipe *r)
{
    char cmd[CMD_BUF];
    char safe_name[256];

    printf("Building %s %s\n", r->name, r->version);

    if (shell_safe(r->name, safe_name, sizeof(safe_name))) {
        fprintf(stderr, "Error: package name too long to escape\n");
        return 1;
    }

    if (build_cmd(cmd, sizeof(cmd),
        "rm -rf sources/%s build/%s pkg/%s",
        safe_name, safe_name, safe_name))
        return 1;

    if (exec_cmd(cmd))
        return 1;

    if (build_cmd(cmd, sizeof(cmd),
        "mkdir -p sources/%s build/%s pkg/%s output",
        safe_name, safe_name, safe_name))
        return 1;

    if (exec_cmd(cmd))
        return 1;

    if (fetch_sources(r))     return 1;
    if (verify_checksums(r))  return 1;
    if (extract_sources(r))   return 1;
    if (run_build_script(r))  return 1;
    if (generate_pkginfo(r))  return 1;
    if (generate_filelist(r)) return 1;
    if (create_package(r))    return 1;

    /* Fix 8: clean up per-build temp script */
    unlink(r->build_script);

    printf("Package created: output/%s-%s.pkg.tar.zst\n",
           r->name, r->version);

    return 0;
}
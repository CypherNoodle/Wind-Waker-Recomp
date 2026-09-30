// The executable of the Mac release app (scripts/mac/package_release.sh). The
// app carries the host and the recompiled game module, never game data: at
// the first launch it asks for the player's own disc image (GZLE01, USA
// revision 0), checks it, and prepares main.dol and the REL modules from it
// (apple/ios/src/disc_import.c, as the iPad app does) into
//
//   ~/Library/Application Support/Wind Waker Recomp/
//     disc.txt       where the disc image is (the game reads it while it runs)
//     game/          main.dol and rels/, prepared from that disc
//     GZLE01.card    the memory card; sram.bin, settings.ini, logs/
//
// then runs the host with the environment a double-clicked app does not get.
// A variable already set wins, so a terminal launch can override any of it.
#include "disc_import.h"

#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char kTitle[] = "Wind Waker Recomp";

static const char kDefaultSettings[] =
    "# Wind Waker Recomp settings, written by the options menu (Esc or F1 in the game).\n"
    "# KEY=VALUE, the host's environment settings.\n"
    "BLUEWAKE_ASPECT=16:10\n"
    "DOL_AURORA_FULLSCREEN=1\n"
    "DOL_AURORA_FRAME_INTERP=1\n"
    "DOL_AURORA_FRAME_INTERP_STEPS=1\n";

static int exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Text for an AppleScript string literal: quotes and backslashes dropped.
static void script_text(char* out, size_t size, const char* in) {
    size_t n = 0;
    for (; *in != '\0' && n + 1 < size; ++in)
        if (*in != '"' && *in != '\\')
            out[n++] = *in;
    out[n] = '\0';
}

static void run_script(const char* script, char* result, size_t size) {
    char command[4096];
    snprintf(command, sizeof command, "/usr/bin/osascript -e '%s' 2>/dev/null", script);
    FILE* pipe = popen(command, "r");
    if (result != NULL && size > 0)
        result[0] = '\0';
    if (pipe == NULL)
        return;
    if (result != NULL && fgets(result, (int)size, pipe) != NULL) {
        size_t len = strlen(result);
        while (len > 0 && (result[len - 1] == '\n' || result[len - 1] == '\r'))
            result[--len] = '\0';
    }
    pclose(pipe);
}

static void show_message(const char* message, int error) {
    char text[1024], script[2048];
    script_text(text, sizeof text, message);
    snprintf(script, sizeof script,
             "display dialog \"%s\" buttons {\"OK\"} default button 1 with title \"%s\"%s", text,
             kTitle, error ? " with icon stop" : "");
    run_script(script, NULL, 0);
}

// The player's disc image, chosen in a file dialog; false when cancelled.
static int choose_disc(char* path, size_t size) {
    run_script("POSIX path of (choose file with prompt \"Choose your disc image of The Legend of Zelda: "
               "The Wind Waker (USA, GZLE01). It stays where it is; the game reads it while it runs.\")",
               path, size);
    return path[0] != '\0';
}

static int read_line(const char* file, char* out, size_t size) {
    FILE* f = fopen(file, "r");
    if (f == NULL)
        return 0;
    const int ok = fgets(out, (int)size, f) != NULL;
    fclose(f);
    if (!ok)
        return 0;
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r'))
        out[--len] = '\0';
    return len > 0;
}

static void write_text(const char* file, const char* text) {
    FILE* f = fopen(file, "w");
    if (f == NULL)
        return;
    fputs(text, f);
    fclose(f);
}

static void progress(void* context, double fraction, const char* stage) {
    (void)context;
    fprintf(stderr, "[app] preparing the disc: %3.0f%% %s\n", fraction * 100.0, stage != NULL ? stage : "");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char exe[PATH_MAX], real[PATH_MAX];
    uint32_t exe_size = sizeof exe;
    if (_NSGetExecutablePath(exe, &exe_size) != 0 || realpath(exe, real) == NULL)
        return 1;
    // .../Wind Waker Recomp.app/Contents/MacOS/Wind Waker Recomp -> .../Contents
    char contents[PATH_MAX];
    snprintf(contents, sizeof contents, "%s", real);
    for (int i = 0; i < 2; ++i) {
        char* slash = strrchr(contents, '/');
        if (slash == NULL)
            return 1;
        *slash = '\0';
    }
    const char* home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return 1;
    char support[PATH_MAX], logs[PATH_MAX], game[PATH_MAX], path[PATH_MAX];
    snprintf(support, sizeof support, "%s/Library/Application Support/Wind Waker Recomp", home);
    mkdir(support, 0755);
    snprintf(logs, sizeof logs, "%s/logs", support);
    mkdir(logs, 0755);
    snprintf(game, sizeof game, "%s/game", support);

    // One log per session.
    char stamp[32];
    const time_t now = time(NULL);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&now));
    snprintf(path, sizeof path, "%s/session-%s.log", logs, stamp);
    if (freopen(path, "w", stderr) != NULL)
        dup2(fileno(stderr), fileno(stdout));
    setvbuf(stderr, NULL, _IOLBF, 0);

    snprintf(path, sizeof path, "%s/settings.ini", support);
    if (!exists(path))
        write_text(path, kDefaultSettings);

    // The disc: the one chosen before while it is still there and prepared,
    // else a new choice, checked and prepared.
    char disc_file[PATH_MAX], disc[PATH_MAX], dol[PATH_MAX], rels[PATH_MAX];
    snprintf(disc_file, sizeof disc_file, "%s/disc.txt", support);
    snprintf(dol, sizeof dol, "%s/main.dol", game);
    snprintf(rels, sizeof rels, "%s/rels", game);
    const char* disc_env = getenv("BLUEWAKE_DISC");
    if (disc_env != NULL && disc_env[0] != '\0')
        snprintf(disc, sizeof disc, "%s", disc_env);
    else if (!read_line(disc_file, disc, sizeof disc))
        disc[0] = '\0';
    int ready = disc[0] != '\0' && exists(disc) && exists(dol) && exists(rels);
    if (!ready && disc[0] != '\0' && !exists(disc))
        show_message("The disc image chosen before is no longer where it was. Choose it again.", 0);
    // A disc that is there but not prepared yet (BLUEWAKE_DISC from a terminal,
    // or game/ removed) is prepared without asking; one that fails is asked for.
    int ask = !(disc[0] != '\0' && exists(disc));
    while (!ready) {
        if (ask && !choose_disc(disc, sizeof disc))
            return 0;
        ask = 1;
        char error[512] = {0};
        mkdir(game, 0755);
        fprintf(stderr, "[app] preparing %s\n", disc);
        if (bluewake_disc_prepare(disc, game, progress, NULL, error, sizeof error) != 0) {
            fprintf(stderr, "[app] %s\n", error);
            show_message(error, 1);
            continue;
        }
        write_text(disc_file, disc);
        ready = 1;
    }

    char resources[PATH_MAX];
    snprintf(resources, sizeof resources, "%s/Resources", contents);
    setenv("BLUEWAKE_ROOT", resources, 0);
    setenv("BLUEWAKE_DISC", disc, 0);
    setenv("BLUEWAKE_DOL", dol, 0);
    setenv("BLUEWAKE_RELS_DIR", rels, 0);
    snprintf(path, sizeof path, "%s/GZLE01.card", support);
    setenv("BLUEWAKE_CARD_PATH", path, 0);
    snprintf(path, sizeof path, "%s/sram.bin", support);
    setenv("BLUEWAKE_SRAM", path, 0);
    setenv("BLUEWAKE_CLOCK", "now", 0);
    setenv("BLUEWAKE_RENDERER", "aurora", 0);
    setenv("BLUEWAKE_DSP_MODE", "hle", 0);
    setenv("BLUEWAKE_WALL_PACE", "1", 0);
    setenv("BLUEWAKE_CYCLE_CAP", "16384", 0);
    setenv("BLUEWAKE_MAX_BLOCKS", "100000000000", 0);
    setenv("BLUEWAKE_PERF_LOG", "1", 0);
    if (chdir(support) != 0)
        fprintf(stderr, "[app] chdir %s: %s\n", support, strerror(errno));

    char host[PATH_MAX], module[PATH_MAX];
    snprintf(host, sizeof host, "%s/MacOS/bluewake_host", contents);
    snprintf(module, sizeof module, "%s/Frameworks/gGZLE01_recomp.dylib", contents);
    char* host_argv[] = {host, module, NULL};
    execv(host, host_argv);
    fprintf(stderr, "[app] could not run %s: %s\n", host, strerror(errno));
    show_message("The game could not start. The session log is in ~/Library/Application Support/Wind Waker "
                 "Recomp/logs.",
                 1);
    return 1;
}

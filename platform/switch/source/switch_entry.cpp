// SPDX-License-Identifier: GPL-3.0-or-later

#include <switch.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int bluewake_host_main(int argc, char** argv);

namespace {
constexpr const char* kRoot = "sdmc:/switch/WindWakerRecomp";
constexpr const char* kLog = "sdmc:/switch/WindWakerRecomp/bluewake.log";

void launch_default(const char* name, const char* value) {
    if (std::getenv(name) == nullptr)
        setenv(name, value, 0);
}

FILE* open_log() {
    if (mkdir(kRoot, 0755) != 0 && errno != EEXIST)
        return nullptr;
    FILE* log = std::fopen(kLog, "w");
    if (log == nullptr)
        return nullptr;
    setvbuf(log, nullptr, _IOLBF, 0);
    dup2(fileno(log), STDOUT_FILENO);
    dup2(fileno(log), STDERR_FILENO);
    return log;
}
} // namespace

int main(int argc, char** argv) {
    FILE* log = open_log();
    std::printf("Wind Waker Recomp - static Switch host\n");

    // Only paths and platform policy live in the NRO. main.dol and the disc
    // image must be extracted by the owner from the supported GZLE01 disc and
    // copied to this directory; they are never embedded in a release package.
    launch_default("BLUEWAKE_ROOT", kRoot);
    launch_default("BLUEWAKE_DOL",
                   "sdmc:/switch/WindWakerRecomp/game/main.dol");
    launch_default("BLUEWAKE_RELS_DIR",
                   "sdmc:/switch/WindWakerRecomp/game/rels");
    launch_default("BLUEWAKE_DISC",
                   "sdmc:/switch/WindWakerRecomp/game/GZLE01.iso");
    launch_default("BLUEWAKE_CARD_PATH",
                   "sdmc:/switch/WindWakerRecomp/save/GZLE01.card");
    launch_default("BLUEWAKE_STATE_DIR",
                   "sdmc:/switch/WindWakerRecomp/states");
    launch_default("BLUEWAKE_SETTINGS", "none");
    launch_default("BLUEWAKE_RENDERER", "aurora");
    launch_default("BLUEWAKE_WALL_PACE", "1");
    launch_default("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1");

    const int result = bluewake_host_main(argc, argv);
    std::printf("Switch host exited with status %d\n", result);
    if (log != nullptr)
        std::fclose(log);
    return result;
}

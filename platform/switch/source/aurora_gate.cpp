// SPDX-License-Identifier: GPL-3.0-or-later

#include <gxruntime/aurora_backend.h>
#include <gxruntime/platform.h>

#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace {
constexpr const char* kLogPath = "sdmc:/WindWakerRecomp-aurora-gate.log";

FILE* open_log() {
    FILE* log = std::fopen(kLogPath, "w");
    if (log == nullptr) {
        return nullptr;
    }
    setvbuf(log, nullptr, _IOLBF, 0);
    dup2(fileno(log), STDOUT_FILENO);
    dup2(fileno(log), STDERR_FILENO);
    return log;
}
} // namespace

int main(int argc, char** argv) {
    FILE* log = open_log();
    std::printf("Wind Waker Recomp - Aurora/Dawn/NVK gate\n");

    // NVK intentionally labels GM20B non-conformant. This is the same opt-in
    // used by the hardware-validated raw Vulkan gate.
    setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);

    const AuroraBackendConfig config = {
        .app_name = "WindWakerRecomp",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true,
        .allow_texture_dumps = false,
        .info_logging = true,
        .graphics_logging = false,
        .force_untextured = false,
    };
    if (!dol_aurora_initialize(argc, argv, &config)) {
        std::fprintf(stderr, "ERROR: dol_aurora_initialize failed\n");
        if (log != nullptr) {
            std::fclose(log);
        }
        return EXIT_FAILURE;
    }

    std::printf("Aurora initialized; presenting empty GX frames. Press + to exit.\n");
    PadState pad{};
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    unsigned long long frames = 0;
    while (!dol_platform_should_quit()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0) {
            break;
        }
        dol_platform_present();
        ++frames;
        if (frames == 1) {
            std::printf("first Aurora frame presented\n");
        }
    }

    std::printf("shutting down after %llu frames\n", frames);
    dol_aurora_shutdown();
    std::printf("Aurora shutdown complete\n");
    if (log != nullptr) {
        std::fclose(log);
    }
    return EXIT_SUCCESS;
}

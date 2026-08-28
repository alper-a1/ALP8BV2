#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <span>

// system installed libraries:
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <verilated.h>

// verilator generated headers
#include "Valp8b.h"
// required for cpu internals access
// provides the type definitions for all components
// unused __directly__, but requried
// without this clang complains when digging through cpu_top members.
#include "Valp8b__Syms.h" // IWYU pragma: keep
// ---

#include "alp8b_file.hpp"

constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 480;

// simstate for all the callbacks - connection to renderer and such
struct SimState {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    // current 512 byte program rom
    Alp8bFile prom;

    // timing, all integer. SDL_GetTicks() is ms since SDL_Init, so 0 is a valid start
    std::uint64_t last_frame_ms = 0; // previous frame's SDL_GetTicks()
    std::uint64_t tick_accum = 0;    // fractional ticks, 1000 units = 1 tick
    [[nodiscard]] std::uint16_t sim_clk_hz() const {
        return this->prom.clock_hz[0] | (this->prom.clock_hz[1] << 8); // little endian
    }

    // verilator access
    std::unique_ptr<VerilatedContext> contextp;
    std::unique_ptr<Valp8b> sim;

    // sim quick access functions
    [[nodiscard]] auto *dp() const { return sim->cpu_top->dp; }
    [[nodiscard]] auto ram() const {
        return std::span<const std::uint8_t>{sim->cpu_top->dp->u_ram->mem.data(), sim->cpu_top->dp->u_ram->mem.size()};
    }

    // run one low -> high clock tick with eval
    void sim_tick() const {
        this->sim->clk = 0;
        this->sim->eval();
        this->sim->clk = 1;
        this->sim->eval();
    }

    // run the sim forward by the wall-clock time elapsed since the last frame
    void sim_step(std::uint64_t now_ms) {
        // cap dt so losing window focus can't time-warp the machine
        std::uint64_t dt = now_ms - this->last_frame_ms;
        this->last_frame_ms = now_ms;
        dt = std::min<std::uint64_t>(dt, 100);

        this->tick_accum += static_cast<std::uint64_t>(this->sim_clk_hz()) * dt;
        const std::uint64_t ticks = this->tick_accum / 1000;
        this->tick_accum %= 1000;
        for (std::uint64_t i = 0; i < ticks; ++i) {
            this->sim_tick();
        }
    }

    // run the sim through a hardware reset cycle , refresh ram with fresh prog data
    void sim_reset() const {
        // direct copy the program data over into the RAM
        this->dp()->u_ram->mem = this->prom.program.data();

        // do the hardware reset cycle
        this->sim->rst_n = 0;
        sim_tick();
        sim_tick();
        sim_tick();

        this->sim->rst_n = 1;
        sim_tick();
    }

    ~SimState() {
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        if (sim) {
            // end the verialtor sim properly.
            sim->final();
        }

        // unique pointers die of natural causes
    }
};

std::expected<Alp8bFile, std::string> LoadProgram(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(std::format("file does not exist: {}", path.string()));
    }

    // size sanity check since anything that isnt the exact size is obv not the correct binary
    std::size_t size = std::filesystem::file_size(path);
    if (size != ALP8BF_TOTAL_BYTES) {
        return std::unexpected(std::format("wrong size: expected {} bytes, got {}", ALP8BF_TOTAL_BYTES, size));
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(std::format("could not open file: {}", path.string()));
    }

    Alp8bFile progrom;

    // read and overwrite everything in the progrom
    file.read(reinterpret_cast<char *>(&progrom), sizeof(progrom));

    if (!file) {
        return std::unexpected("read failed or file truncated");
    }

    // check the binary magic.
    if (progrom.magic != ALP8BF_MAGIC) {
        return std::unexpected(std::format("bad magic: expected {::#x}, got {::#x}", ALP8BF_MAGIC, progrom.magic));
    }

    // if magic is good we just assume the rest of the data is good, and return it
    // so it can be parsed and added to the sim in simstate
    return progrom;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (argc != 2) {
        SDL_Log("Usage: %s <program path>", argv[0]);
        return SDL_APP_FAILURE;
    }

    SDL_SetAppMetadata("ALP8BV2 Simulator", "1.0", "alp8bv2.sim");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto *ss = new SimState();
    *appstate = ss;

    if (!SDL_CreateWindowAndRenderer("ALP8BV2", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &ss->window,
                                     &ss->renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(ss->renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // set up the core sim
    ss->contextp = std::make_unique<VerilatedContext>();
    ss->sim = std::make_unique<Valp8b>(ss->contextp.get(), "ALP8BV2");

    auto result = LoadProgram(argv[1]);
    if (!result) {
        SDL_Log("Failed to load program: %s", result.error().c_str());
        return SDL_APP_FAILURE;
    }

    // direct copy since the file data is ensured to be trival copy!
    ss->prom = result.value();

    ss->sim_reset();

    return SDL_APP_CONTINUE;
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    auto &ss = *static_cast<SimState *>(appstate);

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
    auto *ss = static_cast<SimState *>(appstate);

    // step the iteration accurate number of sim steps
    const std::uint64_t now = SDL_GetTicks();
    ss->sim_step(now);

    const int charsize = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(ss->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE); /* black, full alpha */
    SDL_RenderClear(ss->renderer);                                   /* start with a blank canvas. */

    SDL_SetRenderDrawColor(ss->renderer, 255, 255, 255, SDL_ALPHA_OPAQUE); /* white, full alpha */
    SDL_RenderDebugText(ss->renderer, 272, 100, "Hello world!");
    SDL_RenderDebugText(ss->renderer, 224, 150, "This is some debug text.");

    SDL_SetRenderDrawColor(ss->renderer, 51, 102, 255, SDL_ALPHA_OPAQUE); /* light blue, full alpha */
    SDL_RenderDebugText(ss->renderer, 184, 200, "You can do it in different colors.");
    SDL_SetRenderDrawColor(ss->renderer, 255, 255, 255, SDL_ALPHA_OPAQUE); /* white, full alpha */

    SDL_SetRenderScale(ss->renderer, 4.0f, 4.0f);
    SDL_RenderDebugText(ss->renderer, 14, 65, "It can be scaled.");
    SDL_SetRenderScale(ss->renderer, 1.0f, 1.0f);
    SDL_RenderDebugText(ss->renderer, 64, 350, "This only does ASCII chars. So this laughing emoji won't draw: 🤣");

    SDL_RenderDebugTextFormat(ss->renderer, ((float)(WINDOW_WIDTH - (charsize * 46)) / 2), 400,
                              "(This program has been running for %" SDL_PRIu64 " seconds.)", SDL_GetTicks() / 1000);

    SDL_RenderPresent(ss->renderer); /* put it all on the screen! */

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    auto *ss = static_cast<SimState *>(appstate);

    delete ss;
    SDL_Quit();
}

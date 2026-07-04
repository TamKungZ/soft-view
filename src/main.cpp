// Soft View - lightweight native media viewer for Linux
// SDL2 (window/input, works on X11 and Wayland) + libmpv (decode/render) + Dear ImGui (overlay UI)

#include <SDL.h>
#include <SDL_opengl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>

// Custom SDL events used to wake the main loop from mpv's callback threads.
static Uint32 EVENT_MPV_WAKEUP = 0;
static Uint32 EVENT_MPV_RENDER = 0;

static void on_mpv_events(void *ctx) {
    SDL_Event event{};
    event.type = EVENT_MPV_WAKEUP;
    SDL_PushEvent(&event);
}

static void on_mpv_render_update(void *ctx) {
    SDL_Event event{};
    event.type = EVENT_MPV_RENDER;
    SDL_PushEvent(&event);
}

static void *get_proc_address(void *ctx, const char *name) {
    return SDL_GL_GetProcAddress(name);
}

struct PlayerState {
    bool paused = true;
    double duration = 0.0;
    double position = 0.0;
    double volume = 100.0;
    bool has_video = false;
    bool has_media = false;
    std::string filename;
};

static void handle_mpv_property(mpv_handle *mpv, mpv_event_property *prop, PlayerState &st) {
    if (!prop->data) return;
    std::string name = prop->name;
    if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
        st.paused = *(int *)prop->data;
    } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
        st.duration = *(double *)prop->data;
    } else if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
        st.position = *(double *)prop->data;
    } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
        st.volume = *(double *)prop->data;
    }
}

static std::string format_time(double seconds) {
    if (seconds < 0 || seconds != seconds) seconds = 0; // NaN guard
    int total = (int)seconds;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    char buf[32];
    if (h > 0) snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    else snprintf(buf, sizeof(buf), "%d:%02d", m, s);
    return buf;
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    EVENT_MPV_WAKEUP = SDL_RegisterEvents(1);
    EVENT_MPV_RENDER = SDL_RegisterEvents(1);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    SDL_Window *window = SDL_CreateWindow(
        "Soft View",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); // vsync

    // --- mpv setup ---
    mpv_handle *mpv = mpv_create();
    if (!mpv) { fprintf(stderr, "mpv_create failed\n"); return 1; }

    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "hwdec", "auto");
    mpv_set_option_string(mpv, "keep-open", "yes");

    if (mpv_initialize(mpv) < 0) {
        fprintf(stderr, "mpv_initialize failed\n");
        return 1;
    }

    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(mpv, on_mpv_events, nullptr);

    mpv_opengl_init_params gl_init_params{get_proc_address, nullptr};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context *mpv_gl = nullptr;
    if (mpv_render_context_create(&mpv_gl, mpv, params) < 0) {
        fprintf(stderr, "mpv_render_context_create failed\n");
        return 1;
    }
    mpv_render_context_set_update_callback(mpv_gl, on_mpv_render_update, nullptr);

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // no ui state file, keeps it stateless/instant
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    PlayerState state;
    if (argc > 1) {
        const char *cmd[] = {"loadfile", argv[1], nullptr};
        mpv_command_async(mpv, 0, cmd);
        state.filename = argv[1];
        state.has_media = true;
    }

    bool running = true;
    bool overlay_visible = true;
    bool fullscreen = false;
    auto last_activity = std::chrono::steady_clock::now();
    bool seeking = false;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);

            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) {
                overlay_visible = true;
                last_activity = std::chrono::steady_clock::now();
            }

            if (e.type == SDL_DROPFILE) {
                const char *cmd[] = {"loadfile", e.drop.file, nullptr};
                mpv_command_async(mpv, 0, cmd);
                state.filename = e.drop.file;
                state.has_media = true;
                SDL_free(e.drop.file);
            }

            if (e.type == SDL_KEYDOWN && !io.WantCaptureKeyboard) {
                switch (e.key.keysym.sym) {
                    case SDLK_SPACE: {
                        const char *cmd[] = {"cycle", "pause", nullptr};
                        mpv_command_async(mpv, 0, cmd);
                        break;
                    }
                    case SDLK_f: {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;
                    }
                    case SDLK_ESCAPE:
                        if (fullscreen) {
                            fullscreen = false;
                            SDL_SetWindowFullscreen(window, 0);
                        }
                        break;
                    case SDLK_LEFT: {
                        const char *cmd[] = {"seek", "-5", nullptr};
                        mpv_command_async(mpv, 0, cmd);
                        break;
                    }
                    case SDLK_RIGHT: {
                        const char *cmd[] = {"seek", "5", nullptr};
                        mpv_command_async(mpv, 0, cmd);
                        break;
                    }
                    default: break;
                }
            }

            if (e.type == EVENT_MPV_WAKEUP) {
                while (true) {
                    mpv_event *mp_event = mpv_wait_event(mpv, 0);
                    if (mp_event->event_id == MPV_EVENT_NONE) break;
                    if (mp_event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                        handle_mpv_property(mpv, (mpv_event_property *)mp_event->data, state);
                    } else if (mp_event->event_id == MPV_EVENT_VIDEO_RECONFIG) {
                        state.has_video = true;
                    } else if (mp_event->event_id == MPV_EVENT_SHUTDOWN) {
                        running = false;
                    }
                }
            }
        }

        // Auto-hide overlay after 2.5s of inactivity (only while playing).
        auto idle = std::chrono::duration<double>(std::chrono::steady_clock::now() - last_activity).count();
        if (!state.paused && idle > 2.5) overlay_visible = false;

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // Render video frame from mpv into the default framebuffer.
        mpv_opengl_fbo mpv_fbo{0, w, h, 0};
        int flip_y = 1;
        mpv_render_param render_params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_render(mpv_gl, render_params);

        // --- ImGui overlay ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (overlay_visible && state.has_media) {
            ImGui::SetNextWindowPos(ImVec2(0, (float)h - 56), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2((float)w, 56), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.55f));
            ImGui::Begin("##controls", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse);

            if (ImGui::Button(state.paused ? "Play" : "Pause")) {
                const char *cmd[] = {"cycle", "pause", nullptr};
                mpv_command_async(mpv, 0, cmd);
            }
            ImGui::SameLine();

            float pos = (float)state.position;
            float dur = state.duration > 0 ? (float)state.duration : 1.0f;
            ImGui::SetNextItemWidth(w - 260.0f);
            if (ImGui::SliderFloat("##seek", &pos, 0.0f, dur, "", ImGuiSliderFlags_NoInput)) {
                double target = pos;
                mpv_set_property_async(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE, &target);
            }
            ImGui::SameLine();
            ImGui::Text("%s / %s", format_time(state.position).c_str(), format_time(state.duration).c_str());

            ImGui::SameLine();
            float vol = (float)state.volume;
            ImGui::SetNextItemWidth(100);
            if (ImGui::SliderFloat("##vol", &vol, 0.0f, 130.0f, "Vol")) {
                double target = vol;
                mpv_set_property_async(mpv, 0, "volume", MPV_FORMAT_DOUBLE, &target);
            }

            ImGui::End();
            ImGui::PopStyleColor();
        }

        ImGui::Render();
        glViewport(0, 0, w, h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    mpv_render_context_free(mpv_gl);
    mpv_terminate_destroy(mpv);

    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

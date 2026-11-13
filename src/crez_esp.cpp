// crez_esp.cpp: ImGui ESP поверх eglSwapBuffers, Android arm64, inline hook
// НЕ ПРОВЕРЕНО НА УСТРОЙСТВЕ. Оффсеты ниже — стандартные UE4 значения-кандидаты,
// для этой сборки не подтверждены. Включи DEBUG и сверь значения до включения ESP.
#include <EGL/egl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include "inline_hook.h"

#define TAG "crez"
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// --- оффсеты (кандидаты, сверить через DEBUG) ---
#define OFF_GWORLD        0xF4EDC20   // кандидат из статического скана libUE4.so
#define OFF_PERSISTENT    0x30        // UWorld::PersistentLevel
#define OFF_LEVEL_ACTORS  0x98        // ULevel::Actors (TArray<AActor*>)
#define OFF_ROOTCOMP      0x188       // AActor::RootComponent
#define OFF_RELLOC        0x11C       // USceneComponent::RelativeLocation (FVector)
#define OFF_VIEWMATRIX    0x0         // FILL: матрица камеры

struct Cfg {
    bool esp = true, box = true, dot = true, debug = true;
    float maxDist = 400.f;
} cfg;

typedef EGLBoolean (*SwapFn)(EGLDisplay, EGLSurface);
static SwapFn oSwap = nullptr;
static bool inited = false;
static uintptr_t libBase = 0;
static int W = 0, H = 0;

static uintptr_t getLibBase(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, name)) { base = strtoull(line, nullptr, 16); break; }
    }
    fclose(f);
    return base;
}

// безопасное чтение: false на невалидном адресе, не падает
template <class T>
static bool rd(uintptr_t addr, T& out) {
    if (!addr) return false;
    struct iovec l { &out, sizeof(T) };
    struct iovec r { (void*)addr, sizeof(T) };
    return process_vm_readv(getpid(), &l, 1, &r, 1, 0) == (ssize_t)sizeof(T);
}

struct FVector { float x, y, z; };

// UE4 row-major 4x4: clip = M * (x,y,z,1); экран через перспективное деление
static bool w2s(const float m[16], const FVector& w, ImVec2& s) {
    float x = m[0]*w.x + m[4]*w.y + m[8]*w.z  + m[12];
    float y = m[1]*w.x + m[5]*w.y + m[9]*w.z  + m[13];
    float z = m[3]*w.x + m[7]*w.y + m[11]*w.z + m[15];
    if (z <= 0.001f) return false;
    s.x = W * 0.5f + (W * 0.5f) * (x / z);
    s.y = H * 0.5f - (H * 0.5f) * (y / z);
    return true;
}

static void drawESP() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    uintptr_t gworld = 0, level = 0, actorsData = 0;
    int32_t   actorsNum = 0;
    float     vm[16] = {0};

    if (!rd(libBase + OFF_GWORLD, gworld) || !gworld) return;
    if (!rd(gworld + OFF_PERSISTENT, level) || !level) return;
    if (!rd(level + OFF_LEVEL_ACTORS, actorsData) || !actorsData) return;
    if (!rd(level + OFF_LEVEL_ACTORS + 8, actorsNum)) return;   // Num в TArray
    if (actorsNum <= 0 || actorsNum > 8192) return;
    if (!rd(libBase + OFF_VIEWMATRIX, vm)) return;

    int drawn = 0;
    for (int32_t i = 0; i < actorsNum; i++) {
        uintptr_t actor = 0;
        if (!rd(actorsData + (uintptr_t)i * 8, actor) || !actor) continue;

        uintptr_t root = 0;
        if (!rd(actor + OFF_ROOTCOMP, root) || !root) continue;

        FVector pos{};
        if (!rd(root + OFF_RELLOC, pos)) continue;
        if (pos.x == 0.f && pos.y == 0.f && pos.z == 0.f) continue;

        ImVec2 s;
        if (!w2s(vm, pos, s)) continue;
        if (s.x < -50 || s.y < -50 || s.x > W + 50 || s.y > H + 50) continue;

        drawn++;
        if (cfg.dot)
            dl->AddCircleFilled(s, 3.f, IM_COL32(255, 60, 60, 255));
        if (cfg.box)
            dl->AddRect(ImVec2(s.x - 18, s.y - 40), ImVec2(s.x + 18, s.y),
                        IM_COL32(255, 255, 255, 220));
    }

    if (cfg.debug) {
        ImGui::Text("gworld %p", (void*)gworld);
        ImGui::Text("level  %p", (void*)level);
        ImGui::Text("actors %d (on screen %d)", actorsNum, drawn);
    }
}

static EGLBoolean hkSwap(EGLDisplay d, EGLSurface s) {
    if (!inited) {
        EGLint w = 0, h = 0;
        eglQuerySurface(d, s, EGL_WIDTH, &w);
        eglQuerySurface(d, s, EGL_HEIGHT, &h);
        W = w; H = h;
        libBase = getLibBase("libUE4.so");
        ImGui::CreateContext();
        ImGui::GetIO().DisplaySize = ImVec2((float)W, (float)H);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        LOG("init %dx%d base=%p", W, H, (void*)libBase);
        inited = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("crez", nullptr)) {
        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("ESP")) {
                ImGui::Checkbox("enable", &cfg.esp);
                ImGui::Checkbox("box", &cfg.box);
                ImGui::Checkbox("dot", &cfg.dot);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("DEBUG")) {
                ImGui::Checkbox("show debug", &cfg.debug);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (cfg.esp) drawESP();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return oSwap(d, s);
}

__attribute__((constructor))
static void init() {
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    void* sw = dlsym(egl, "eglSwapBuffers");
    inline_hook(sw, (void*)hkSwap, (void**)&oSwap);
    LOG("hooked eglSwapBuffers");
}

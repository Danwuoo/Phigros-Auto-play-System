#include <android/choreographer.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace {
constexpr int kTraceSlots = 64;
constexpr std::uint32_t kMask = 0xFFFFFF;

struct Trace {
    std::uint32_t sequence = 0;
    std::uint32_t meta = 0;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t checksum = 0;
};

struct Fixture {
    android_app* app = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int width = 0;
    int height = 0;
    std::int64_t first_frame_ns = 0;
    std::int64_t next_draw_ns = 0;
    std::int64_t interval_ns = 0;
    std::uint32_t rendered = 0;
    std::uint32_t sequence = 0;
    std::uint32_t overflow = 0;
    std::uint32_t down = 0, up = 0, move = 0, cancel = 0;
    std::int64_t next_property_ns = 0;
    bool capture_frozen = false;
    std::array<Trace, kTraceSlots> traces{};
    std::array<bool, 16> active{};
};

void rect(Fixture& fixture, int x, int y, int width, int height,
          float red, float green, float blue) {
    if (width <= 0 || height <= 0) return;
    glScissor(x, fixture.height - y - height, width, height);
    glClearColor(red, green, blue, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void cell(Fixture& fixture, int x, int y, std::uint32_t value, int width = 24, int height = 20) {
    rect(fixture, x, y, width, height,
         ((value >> 16) & 255) / 255.f, ((value >> 8) & 255) / 255.f,
         (value & 255) / 255.f);
}

#ifndef PAS_TOUCH_FIXTURE
void capture_scene(Fixture& fixture, std::int64_t frame_ns) {
    rect(fixture, 0, 0, fixture.width, fixture.height, 16/255.f, 16/255.f, 16/255.f);
    rect(fixture, 0, 0, 50, 50, 1, 0, 0);
    rect(fixture, fixture.width - 50, 0, 50, 50, 0, 1, 0);
    rect(fixture, 0, fixture.height - 50, 50, 50, 0, 0, 1);
    rect(fixture, fixture.width - 50, fixture.height - 50, 50, 50, 1, 1, 1);
    // Keep the legacy 24-bit counter chip geometry for A/B host comparison.
    for (int row = 0; row < 2; ++row) {
        const int y = 90 + row * 26;
        rect(fixture, 10, y, 12, 12, 1, 0, 0);
        rect(fixture, 200, y, 12, 12, 1, 0, 0);
        for (int bit = 0; bit < 12; ++bit) {
            const bool on = (fixture.rendered & (1u << (row * 12 + bit))) != 0;
            rect(fixture, 30 + bit * 14, y, 12, 12, on ? 1.f : 0.f,
                 on ? 1.f : 0.f, on ? 1.f : 0.f);
        }
    }
    const auto seconds = (frame_ns - fixture.first_frame_ns) / 1e9;
    const auto travel = std::max(1, fixture.width - 80);
    const int x = 40 + static_cast<int>(std::fmod(seconds * 420.0, travel));
    // Lossy-tolerant binary position truth, separate from runtime game logic.
    rect(fixture, 230, 55, 12, 12, 1, 0, 0);
    rect(fixture, 425, 55, 12, 12, 1, 0, 0);
    for (int bit = 0; bit < 11; ++bit) {
        const float on = (x & (1 << bit)) ? 1.f : 0.f;
        rect(fixture, 250 + bit * 14, 55, 12, 12, on, on, on);
    }
    rect(fixture, x, fixture.height / 2 - 25, 40, 50, 1, 1, 0);
    rect(fixture, 80, fixture.height / 2 + 50, fixture.width - 160, 1, 1, 1, 1);
    rect(fixture, 200, fixture.height - 140, 200, 20, 24/255.f, 24/255.f, 24/255.f);
    rect(fixture, 420, fixture.height - 140, 80, 20, 1, 0, 0);
    rect(fixture, 500, fixture.height - 140, 80, 20, 0, 0, 1);
    // Four identical identity cells permit cross-region consistency checks.
    for (int corner = 0; corner < 4; ++corner) {
        const int cx = corner % 2 ? fixture.width - 90 : 65;
        const int cy = corner / 2 ? fixture.height - 90 : 55;
        cell(fixture, cx, cy, fixture.rendered);
        cell(fixture, cx + 25, cy, (fixture.rendered ^ 0xA5C37E) & kMask);
    }
    fixture.rendered = (fixture.rendered + 1) & kMask;
}
#endif

#ifdef PAS_TOUCH_FIXTURE
void touch_scene(Fixture& fixture) {
    rect(fixture, 0, 0, fixture.width, fixture.height, 15/255.f, 18/255.f, 28/255.f);
    const auto active_count = static_cast<std::uint32_t>(std::count(fixture.active.begin(), fixture.active.end(), true));
    const std::array<std::uint32_t, 10> header = {
        0x504153, 2, fixture.sequence, fixture.overflow, active_count,
        fixture.down, fixture.up, fixture.move, fixture.cancel, fixture.rendered};
    for (std::size_t i = 0; i < header.size(); ++i)
        cell(fixture, 12 + static_cast<int>(i) * 30, 40, header[i] & kMask);
    for (int index = 0; index < kTraceSlots; ++index) {
        const auto& trace = fixture.traces[index];
        const int y = 140 + index * 8;
        if (y + 7 > fixture.height) break;
        const std::array<std::uint32_t, 5> values = {
            trace.sequence, trace.meta, trace.x, trace.y, trace.checksum};
        for (std::size_t field = 0; field < values.size(); ++field)
            cell(fixture, 20 + static_cast<int>(field) * 30, y, values[field] & kMask, 24, 6);
    }
    ++fixture.rendered;
}
#endif

bool initialize_display(Fixture& fixture) {
    fixture.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (fixture.display == EGL_NO_DISPLAY || !eglInitialize(fixture.display, nullptr, nullptr)) return false;
    const EGLint attributes[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(fixture.display, attributes, &config, 1, &count) || !count) return false;
    EGLint format = 0;
    eglGetConfigAttrib(fixture.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(fixture.app->window, 0, 0, format);
    fixture.surface = eglCreateWindowSurface(fixture.display, config, fixture.app->window, nullptr);
    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    fixture.context = eglCreateContext(fixture.display, config, EGL_NO_CONTEXT, context_attributes);
    if (fixture.surface == EGL_NO_SURFACE || fixture.context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(fixture.display, fixture.surface, fixture.surface, fixture.context)) return false;
    eglQuerySurface(fixture.display, fixture.surface, EGL_WIDTH, &fixture.width);
    eglQuerySurface(fixture.display, fixture.surface, EGL_HEIGHT, &fixture.height);
    fixture.next_draw_ns = 0;
    glViewport(0, 0, fixture.width, fixture.height);
    glEnable(GL_SCISSOR_TEST);
    return true;
}

void terminate_display(Fixture& fixture) {
    if (fixture.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(fixture.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (fixture.context != EGL_NO_CONTEXT) eglDestroyContext(fixture.display, fixture.context);
        if (fixture.surface != EGL_NO_SURFACE) eglDestroySurface(fixture.display, fixture.surface);
        eglTerminate(fixture.display);
    }
    fixture.display = EGL_NO_DISPLAY; fixture.surface = EGL_NO_SURFACE;
    fixture.context = EGL_NO_CONTEXT;
}

#ifdef PAS_TOUCH_FIXTURE
void append_trace(Fixture& fixture, int phase, int id, float x, float y) {
    ++fixture.sequence;
    if (fixture.sequence > kTraceSlots) ++fixture.overflow;
    auto& trace = fixture.traces[(fixture.sequence - 1) % kTraceSlots];
    trace.sequence = fixture.sequence & kMask;
    trace.meta = ((static_cast<std::uint32_t>(phase) & 255) << 16) |
                 (static_cast<std::uint32_t>(id) & 0xFFFF);
    trace.x = static_cast<std::uint32_t>(std::max(0.f, std::round(x))) & kMask;
    trace.y = static_cast<std::uint32_t>(std::max(0.f, std::round(y))) & kMask;
    trace.checksum = (trace.sequence ^ trace.meta ^ trace.x ^ trace.y ^ 0xA5C37E) & kMask;
}
#endif

int32_t input(android_app* app, AInputEvent* event) {
#ifdef PAS_TOUCH_FIXTURE
    auto& fixture = *static_cast<Fixture*>(app->userData);
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int action = AMotionEvent_getAction(event);
    const int phase = action & AMOTION_EVENT_ACTION_MASK;
    const int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    const int pointers = AMotionEvent_getPointerCount(event);
    if (phase == AMOTION_EVENT_ACTION_DOWN || phase == AMOTION_EVENT_ACTION_POINTER_DOWN ||
        phase == AMOTION_EVENT_ACTION_UP || phase == AMOTION_EVENT_ACTION_POINTER_UP) {
        if (index >= 0 && index < pointers) {
            const int id = AMotionEvent_getPointerId(event, index);
            const int kind = phase == AMOTION_EVENT_ACTION_DOWN || phase == AMOTION_EVENT_ACTION_POINTER_DOWN ? 1 : 3;
            append_trace(fixture, kind, id, AMotionEvent_getX(event, index), AMotionEvent_getY(event, index));
            if (id >= 0 && id < static_cast<int>(fixture.active.size())) fixture.active[id] = kind == 1;
            if (kind == 1) ++fixture.down; else ++fixture.up;
        }
    } else if (phase == AMOTION_EVENT_ACTION_MOVE) {
        for (int i = 0; i < pointers; ++i) {
            const int id = AMotionEvent_getPointerId(event, i);
            append_trace(fixture, 2, id, AMotionEvent_getX(event, i), AMotionEvent_getY(event, i));
            ++fixture.move;
        }
    } else if (phase == AMOTION_EVENT_ACTION_CANCEL) {
        for (int i = 0; i < pointers; ++i) {
            const int id = AMotionEvent_getPointerId(event, i);
            append_trace(fixture, 4, id, AMotionEvent_getX(event, i), AMotionEvent_getY(event, i));
        }
        fixture.active.fill(false);
        ++fixture.cancel;
    }
    if (fixture.display != EGL_NO_DISPLAY) { touch_scene(fixture); eglSwapBuffers(fixture.display, fixture.surface); }
    return 1;
#else
    (void)app; (void)event; return 0;
#endif
}

void command(android_app* app, int32_t action) {
    auto& fixture = *static_cast<Fixture*>(app->userData);
    if (action == APP_CMD_INIT_WINDOW && app->window) {
        if (!initialize_display(fixture)) __android_log_print(ANDROID_LOG_ERROR, "PASFixture", "EGL init failed");
#ifdef PAS_TOUCH_FIXTURE
        else { touch_scene(fixture); eglSwapBuffers(fixture.display, fixture.surface); }
#endif
    } else if (action == APP_CMD_TERM_WINDOW) terminate_display(fixture);
    else if (action == APP_CMD_PAUSE) {
#ifdef PAS_TOUCH_FIXTURE
        for (std::size_t id = 0; id < fixture.active.size(); ++id)
            if (fixture.active[id]) append_trace(fixture, 4, static_cast<int>(id), 0, 0);
        fixture.active.fill(false);
        ++fixture.cancel;
#endif
    }
}

void frame_callback(std::int64_t frame_ns, void* user) {
    auto& fixture = *static_cast<Fixture*>(user);
#ifdef PAS_TOUCH_FIXTURE
    (void)frame_ns;
#endif
    if (!fixture.app->destroyRequested) {
#ifndef PAS_TOUCH_FIXTURE
        if (frame_ns >= fixture.next_property_ns) {
            char frozen[PROP_VALUE_MAX]{};
            __system_property_get("debug.pas.fixture_freeze", frozen);
            fixture.capture_frozen = frozen[0] == '1';
            fixture.next_property_ns = frame_ns + 100'000'000;
        }
        if (fixture.display != EGL_NO_DISPLAY &&
            !fixture.capture_frozen &&
            (!fixture.interval_ns || !fixture.next_draw_ns || frame_ns >= fixture.next_draw_ns)) {
            if (!fixture.first_frame_ns) fixture.first_frame_ns = frame_ns;
            capture_scene(fixture, frame_ns);
            eglSwapBuffers(fixture.display, fixture.surface);
            if (fixture.interval_ns) {
                if (!fixture.next_draw_ns || frame_ns - fixture.next_draw_ns > 1'000'000'000LL)
                    fixture.next_draw_ns = frame_ns;
                do { fixture.next_draw_ns += fixture.interval_ns; }
                while (fixture.next_draw_ns <= frame_ns);
            }
        }
#endif
        AChoreographer_postFrameCallback64(AChoreographer_getInstance(), frame_callback, user);
    }
}
}

void android_main(android_app* app) {
    Fixture fixture;
    fixture.app = app;
    app->userData = &fixture;
    app->onAppCmd = command;
    app->onInputEvent = input;
    ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_FULLSCREEN, 0);
    char property[PROP_VALUE_MAX]{};
    const int size = __system_property_get("debug.pas.fixture_hz", property);
    const int requested = size ? std::atoi(property) : 0;
    const int target = requested == 40 || requested == 48 || requested == 57 ? requested : 0;
    fixture.interval_ns = target ? 1'000'000'000LL / target : 0;
    AChoreographer_postFrameCallback64(AChoreographer_getInstance(), frame_callback, &fixture);
    while (!app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 && source)
            source->process(app, source);
    }
    terminate_display(fixture);
}

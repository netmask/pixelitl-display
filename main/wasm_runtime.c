#include "wasm_runtime.h"
#include "wasm_host.h"
#include "framebuffer.h"
#include "board.h"
#include "wasm3.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "wasm_rt";

static face_t s_faces[MAX_FACES];
static int s_face_count = 0;

face_t *g_current_face = NULL;

esp_err_t wasm_init(void) {
    memset(s_faces, 0, sizeof(s_faces));
    return ESP_OK;
}

int wasm_load_face(const char *name, const uint8_t *wasm_data, uint32_t wasm_size) {
    if (s_face_count >= MAX_FACES) {
        ESP_LOGE(TAG, "Max faces reached");
        return -1;
    }

    face_t *f = &s_faces[s_face_count];
    memset(f, 0, sizeof(face_t));
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->budget_us = DEFAULT_BUDGET_US;
    f->state = FACE_STATE_IDLE;

    // Create wasm3 environment and runtime
    f->env = m3_NewEnvironment();
    if (!f->env) {
        ESP_LOGE(TAG, "m3_NewEnvironment failed");
        return -1;
    }

    f->runtime = m3_NewRuntime(f->env, WASM_STACK_SIZE, NULL);
    if (!f->runtime) {
        ESP_LOGE(TAG, "m3_NewRuntime failed");
        m3_FreeEnvironment(f->env);
        return -1;
    }

    // Parse WASM binary
    M3Result result = m3_ParseModule(f->env, &f->module, wasm_data, wasm_size);
    if (result) {
        ESP_LOGE(TAG, "ParseModule '%s': %s", name, result);
        m3_FreeRuntime(f->runtime);
        m3_FreeEnvironment(f->env);
        return -1;
    }

    // Load module into runtime
    result = m3_LoadModule(f->runtime, f->module);
    if (result) {
        ESP_LOGE(TAG, "LoadModule '%s': %s", name, result);
        m3_FreeModule(f->module);
        m3_FreeRuntime(f->runtime);
        m3_FreeEnvironment(f->env);
        return -1;
    }

    // Link host functions (must be after LoadModule)
    wasm_host_link_all(f->module);

    // Find required exports
    result = m3_FindFunction(&f->fn_init, f->runtime, "app_init");
    if (result) {
        ESP_LOGE(TAG, "'%s' missing app_init: %s", name, result);
        goto fail;
    }
    result = m3_FindFunction(&f->fn_update, f->runtime, "app_update");
    if (result) {
        ESP_LOGE(TAG, "'%s' missing app_update: %s", name, result);
        goto fail;
    }
    result = m3_FindFunction(&f->fn_draw, f->runtime, "app_draw");
    if (result) {
        ESP_LOGE(TAG, "'%s' missing app_draw: %s", name, result);
        goto fail;
    }

    // Optional exports (ok if not found)
    m3_FindFunction(&f->fn_on_gesture, f->runtime, "app_on_gesture");
    m3_FindFunction(&f->fn_destroy, f->runtime, "app_destroy");

    // Call app_init()
    g_current_face = f;
    f->draw_target = fb_get_back();
    result = m3_CallV(f->fn_init);
    g_current_face = NULL;
    if (result) {
        ESP_LOGW(TAG, "'%s' app_init error: %s", name, result);
    }

    f->state = FACE_STATE_RUNNING;
    int id = s_face_count++;
    ESP_LOGI(TAG, "Loaded '%s' (id=%d, %u bytes)", name, id, wasm_size);
    return id;

fail:
    m3_FreeRuntime(f->runtime);
    m3_FreeEnvironment(f->env);
    memset(f, 0, sizeof(face_t));
    return -1;
}

int64_t wasm_execute_frame(int face_id, uint32_t frame, uint32_t dt_ms) {
    if (face_id < 0 || face_id >= s_face_count) return -1;
    face_t *f = &s_faces[face_id];
    if (f->state == FACE_STATE_KILLED) return -1;

    g_current_face = f;
    f->draw_target = fb_get_back();
    f->over_budget = false;
    f->frame_start_us = esp_timer_get_time();

    // app_update(frame, dt)
    M3Result result = m3_CallV(f->fn_update, (int32_t)frame, (int32_t)dt_ms);
    if (result && result != wasm_host_budget_trap) {
        ESP_LOGW(TAG, "'%s' app_update: %s", f->name, result);
    }

    // app_draw()
    if (!f->over_budget) {
        result = m3_CallV(f->fn_draw);
        if (result && result != wasm_host_budget_trap) {
            ESP_LOGW(TAG, "'%s' app_draw: %s", f->name, result);
        }
    }

    int64_t elapsed = esp_timer_get_time() - f->frame_start_us;
    f->last_frame_us = elapsed;
    f->total_frames++;

    // Exponential moving average (alpha ~= 0.1)
    if (f->avg_frame_us == 0) {
        f->avg_frame_us = elapsed;
    } else {
        f->avg_frame_us = (f->avg_frame_us * 9 + elapsed) / 10;
    }

    // Budget enforcement
    if (elapsed > f->budget_us) {
        f->overbudget_streak++;
        f->skipped_frames++;

        switch (f->state) {
        case FACE_STATE_RUNNING:
            f->state = FACE_STATE_WARNED;
            ESP_LOGW(TAG, "'%s' over budget (%lld > %lld us)",
                     f->name, (long long)elapsed, (long long)f->budget_us);
            break;
        case FACE_STATE_WARNED:
            if (f->overbudget_streak >= 30) {
                f->state = FACE_STATE_THROTTLED;
                f->budget_us = f->budget_us * 3 / 4;
                if (f->budget_us < 20000) f->budget_us = 20000;
                ESP_LOGW(TAG, "'%s' throttled, budget=%lld",
                         f->name, (long long)f->budget_us);
            }
            break;
        case FACE_STATE_THROTTLED:
            if (f->overbudget_streak >= 300) {
                f->state = FACE_STATE_KILLED;
                ESP_LOGE(TAG, "'%s' KILLED (300x overbudget)", f->name);
            }
            break;
        default:
            break;
        }
    } else {
        if (f->overbudget_streak > 0) f->overbudget_streak--;
        if (f->overbudget_streak == 0 && f->state == FACE_STATE_WARNED) {
            f->state = FACE_STATE_RUNNING;
        }
        if (f->state == FACE_STATE_THROTTLED && f->overbudget_streak == 0) {
            f->budget_us += 100;
            if (f->budget_us >= DEFAULT_BUDGET_US) {
                f->budget_us = DEFAULT_BUDGET_US;
                f->state = FACE_STATE_RUNNING;
            }
        }
    }

    g_current_face = NULL;
    return elapsed;
}

face_t *wasm_get_face(int id) {
    return (id >= 0 && id < s_face_count) ? &s_faces[id] : NULL;
}

int wasm_face_count(void) {
    return s_face_count;
}

void wasm_unload_face(int id) {
    if (id < 0 || id >= s_face_count) return;
    face_t *f = &s_faces[id];
    if (f->fn_destroy) {
        g_current_face = f;
        m3_CallV(f->fn_destroy);
        g_current_face = NULL;
    }
    if (f->runtime) m3_FreeRuntime(f->runtime);
    if (f->env)     m3_FreeEnvironment(f->env);
    memset(f, 0, sizeof(face_t));
}

// WASM3 face runtime — lifecycle, budget enforcement, frame execution
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "wasm3.h"

#define MAX_FACES         8
#define WASM_STACK_SIZE   (16 * 1024)
#define DEFAULT_BUDGET_US 100000

typedef enum {
    FACE_STATE_IDLE,
    FACE_STATE_RUNNING,
    FACE_STATE_WARNED,
    FACE_STATE_THROTTLED,
    FACE_STATE_KILLED,
} face_state_t;

typedef struct {
    char name[32];

    IM3Environment env;
    IM3Runtime     runtime;
    IM3Module      module;
    IM3Function    fn_init;
    IM3Function    fn_update;
    IM3Function    fn_draw;
    IM3Function    fn_on_gesture;
    IM3Function    fn_destroy;

    face_state_t state;
    int64_t  budget_us;
    int64_t  frame_start_us;
    int64_t  last_frame_us;
    int64_t  avg_frame_us;
    int      overbudget_streak;
    bool     over_budget;

    uint32_t total_frames;
    uint32_t skipped_frames;

    uint16_t *draw_target;
} face_t;

// Global: currently executing face (set during wasm_execute_frame)
extern face_t *g_current_face;

esp_err_t wasm_init(void);
int       wasm_load_face(const char *name, const uint8_t *wasm_data, uint32_t wasm_size);
int64_t   wasm_execute_frame(int face_id, uint32_t frame, uint32_t dt_ms);
face_t   *wasm_get_face(int face_id);
int       wasm_face_count(void);
void      wasm_unload_face(int face_id);

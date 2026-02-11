#include "http_request.h"
#include "wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "http_req";

typedef struct {
    atomic_int state;           // http_state_t
    char       url[512];
    char       content_type[64];
    char      *post_body;       // heap-allocated for POST
    size_t     post_body_len;
    uint8_t   *response;        // PSRAM-allocated
    int        response_len;
    int        status_code;
} http_slot_t;

static http_slot_t s_slots[HTTP_MAX_SLOTS];
static TaskHandle_t s_http_task_handle;
static SemaphoreHandle_t s_work_sem;

// HTTP event handler — accumulates response into slot buffer
typedef struct {
    http_slot_t *slot;
    int          offset;
} http_event_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_event_ctx_t *ctx = (http_event_ctx_t *)evt->user_data;
    if (!ctx || !ctx->slot) return ESP_OK;

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int avail = HTTP_MAX_RESPONSE - ctx->offset;
        int to_copy = evt->data_len;
        if (to_copy > avail) to_copy = avail;
        if (to_copy > 0 && ctx->slot->response) {
            memcpy(ctx->slot->response + ctx->offset, evt->data, to_copy);
            ctx->offset += to_copy;
        }
    }
    return ESP_OK;
}

static void process_slot(int i) {
    http_slot_t *slot = &s_slots[i];

    int expected = HTTP_STATE_PENDING;
    if (!atomic_compare_exchange_strong(&slot->state, &expected, HTTP_STATE_IN_PROGRESS)) {
        return;
    }

    // Allocate response buffer in PSRAM
    slot->response = heap_caps_malloc(HTTP_MAX_RESPONSE, MALLOC_CAP_SPIRAM);
    if (!slot->response) {
        ESP_LOGE(TAG, "slot %d: PSRAM alloc failed", i);
        atomic_store(&slot->state, HTTP_STATE_ERROR);
        return;
    }
    slot->response_len = 0;
    slot->status_code = 0;

    http_event_ctx_t ctx = { .slot = slot, .offset = 0 };

    esp_http_client_config_t config = {
        .url = slot->url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "slot %d: client init failed", i);
        heap_caps_free(slot->response);
        slot->response = NULL;
        atomic_store(&slot->state, HTTP_STATE_ERROR);
        return;
    }

    // Set POST if we have a body
    if (slot->post_body && slot->post_body_len > 0) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", slot->content_type);
        esp_http_client_set_post_field(client, slot->post_body, slot->post_body_len);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        slot->status_code = esp_http_client_get_status_code(client);
        slot->response_len = ctx.offset;
        ESP_LOGI(TAG, "slot %d: %d, %d bytes", i, slot->status_code, slot->response_len);
        atomic_store(&slot->state, HTTP_STATE_DONE);
    } else {
        ESP_LOGE(TAG, "slot %d: request failed: %s", i, esp_err_to_name(err));
        heap_caps_free(slot->response);
        slot->response = NULL;
        atomic_store(&slot->state, HTTP_STATE_ERROR);
    }

    esp_http_client_cleanup(client);

    // Free POST body if allocated
    if (slot->post_body) {
        free(slot->post_body);
        slot->post_body = NULL;
        slot->post_body_len = 0;
    }
}

static void http_task(void *arg) {
    ESP_LOGI(TAG, "HTTP task on core %d", xPortGetCoreID());
    while (true) {
        // Wait for work notification
        xSemaphoreTake(s_work_sem, pdMS_TO_TICKS(100));

        for (int i = 0; i < HTTP_MAX_SLOTS; i++) {
            if (atomic_load(&s_slots[i].state) == HTTP_STATE_PENDING) {
                process_slot(i);
            }
        }
    }
}

esp_err_t http_request_init(void) {
    for (int i = 0; i < HTTP_MAX_SLOTS; i++) {
        atomic_store(&s_slots[i].state, HTTP_STATE_FREE);
        s_slots[i].response = NULL;
        s_slots[i].post_body = NULL;
    }

    s_work_sem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(http_task, "http", 8192, NULL, 4, &s_http_task_handle, 0);
    ESP_LOGI(TAG, "HTTP request pool initialized (%d slots)", HTTP_MAX_SLOTS);
    return ESP_OK;
}

static int find_free_slot(void) {
    for (int i = 0; i < HTTP_MAX_SLOTS; i++) {
        int expected = HTTP_STATE_FREE;
        if (atomic_compare_exchange_strong(&s_slots[i].state, &expected, HTTP_STATE_PENDING)) {
            return i;
        }
    }
    return -1;
}

int http_request_start_get(const char *url, size_t url_len) {
    if (!wifi_is_connected()) return -1;
    if (url_len >= sizeof(s_slots[0].url)) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    memcpy(s_slots[slot].url, url, url_len);
    s_slots[slot].url[url_len] = '\0';
    s_slots[slot].post_body = NULL;
    s_slots[slot].post_body_len = 0;

    xSemaphoreGive(s_work_sem);
    return slot;
}

int http_request_start_post(const char *url, size_t url_len,
                            const char *content_type, size_t ct_len,
                            const void *body, size_t body_len) {
    if (!wifi_is_connected()) return -1;
    if (url_len >= sizeof(s_slots[0].url)) return -1;
    if (ct_len >= sizeof(s_slots[0].content_type)) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    memcpy(s_slots[slot].url, url, url_len);
    s_slots[slot].url[url_len] = '\0';
    memcpy(s_slots[slot].content_type, content_type, ct_len);
    s_slots[slot].content_type[ct_len] = '\0';

    // Copy POST body to heap
    s_slots[slot].post_body = malloc(body_len);
    if (!s_slots[slot].post_body) {
        atomic_store(&s_slots[slot].state, HTTP_STATE_FREE);
        return -1;
    }
    memcpy(s_slots[slot].post_body, body, body_len);
    s_slots[slot].post_body_len = body_len;

    xSemaphoreGive(s_work_sem);
    return slot;
}

http_state_t http_request_status(int handle) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return HTTP_STATE_FREE;
    return (http_state_t)atomic_load(&s_slots[handle].state);
}

int http_request_response_code(int handle) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return -1;
    if (atomic_load(&s_slots[handle].state) != HTTP_STATE_DONE) return -1;
    return s_slots[handle].status_code;
}

int http_request_response_len(int handle) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return -1;
    if (atomic_load(&s_slots[handle].state) != HTTP_STATE_DONE) return -1;
    return s_slots[handle].response_len;
}

int http_request_read(int handle, void *dst, size_t max_len) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return -1;
    if (atomic_load(&s_slots[handle].state) != HTTP_STATE_DONE) return -1;
    if (!s_slots[handle].response) return -1;

    int to_copy = s_slots[handle].response_len;
    if ((size_t)to_copy > max_len) to_copy = (int)max_len;
    memcpy(dst, s_slots[handle].response, to_copy);
    return to_copy;
}

uint8_t *http_request_take_buffer(int handle, int *out_len) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return NULL;
    if (atomic_load(&s_slots[handle].state) != HTTP_STATE_DONE) return NULL;
    if (!s_slots[handle].response) return NULL;

    uint8_t *buf = s_slots[handle].response;
    if (out_len) *out_len = s_slots[handle].response_len;

    // Transfer ownership — slot no longer owns the buffer
    s_slots[handle].response = NULL;
    s_slots[handle].response_len = 0;
    return buf;
}

void http_request_close(int handle) {
    if (handle < 0 || handle >= HTTP_MAX_SLOTS) return;

    int state = atomic_load(&s_slots[handle].state);
    if (state == HTTP_STATE_FREE) return;
    // Don't close if in-progress — wait for completion
    if (state == HTTP_STATE_IN_PROGRESS) return;

    if (s_slots[handle].response) {
        heap_caps_free(s_slots[handle].response);
        s_slots[handle].response = NULL;
    }
    if (s_slots[handle].post_body) {
        free(s_slots[handle].post_body);
        s_slots[handle].post_body = NULL;
    }
    atomic_store(&s_slots[handle].state, HTTP_STATE_FREE);
}

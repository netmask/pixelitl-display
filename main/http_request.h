#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#define HTTP_MAX_SLOTS       4
#define HTTP_MAX_RESPONSE    (16 * 1024)
#define HTTP_TIMEOUT_MS      10000

typedef enum {
    HTTP_STATE_FREE        = 0,
    HTTP_STATE_PENDING     = 1,
    HTTP_STATE_IN_PROGRESS = 2,
    HTTP_STATE_DONE        = 3,
    HTTP_STATE_ERROR       = 4,
} http_state_t;

esp_err_t http_request_init(void);

// Returns handle (0..3) or -1 on failure
int http_request_start_get(const char *url, size_t url_len);
int http_request_start_post(const char *url, size_t url_len,
                            const char *content_type, size_t ct_len,
                            const void *body, size_t body_len);

// Query slot state
http_state_t http_request_status(int handle);
int          http_request_response_code(int handle);
int          http_request_response_len(int handle);

// Read response data (returns bytes copied or -1)
int  http_request_read(int handle, void *dst, size_t max_len);

// Transfer ownership of PSRAM response buffer out of slot (returns NULL on error)
uint8_t *http_request_take_buffer(int handle, int *out_len);

// Release slot back to FREE
void http_request_close(int handle);

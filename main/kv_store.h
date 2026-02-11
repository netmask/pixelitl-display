#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#define KV_MAX_KEY_LEN   15
#define KV_MAX_VALUE_LEN 2048

esp_err_t kv_store_init(void);

// All return 0 on success, -1 on error. get returns bytes read or -1.
int kv_store_set(const char *face_name, const void *key, size_t key_len,
                 const void *value, size_t value_len);
int kv_store_get(const char *face_name, const void *key, size_t key_len,
                 void *buf, size_t max_len);
int kv_store_del(const char *face_name, const void *key, size_t key_len);

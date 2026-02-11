#include "kv_store.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "kv_store";

esp_err_t kv_store_init(void) {
    ESP_LOGI(TAG, "KV store initialized");
    return ESP_OK;
}

// Build NVS namespace from face name (max 15 chars)
static void make_namespace(const char *face_name, char *ns, size_t ns_size) {
    size_t len = strlen(face_name);
    if (len >= ns_size) len = ns_size - 1;
    memcpy(ns, face_name, len);
    ns[len] = '\0';
}

// Build NVS key from raw bytes (max 15 chars, NVS key limit)
static bool make_key(const void *key, size_t key_len, char *out, size_t out_size) {
    if (key_len == 0 || key_len >= out_size) return false;
    memcpy(out, key, key_len);
    out[key_len] = '\0';
    return true;
}

int kv_store_set(const char *face_name, const void *key, size_t key_len,
                 const void *value, size_t value_len) {
    if (key_len > KV_MAX_KEY_LEN || value_len > KV_MAX_VALUE_LEN) return -1;

    char ns[16], k[16];
    make_namespace(face_name, ns, sizeof(ns));
    if (!make_key(key, key_len, k, sizeof(k))) return -1;

    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return -1;

    esp_err_t err = nvs_set_blob(h, k, value, value_len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set '%s':'%s' failed: %s", ns, k, esp_err_to_name(err));
        return -1;
    }
    return 0;
}

int kv_store_get(const char *face_name, const void *key, size_t key_len,
                 void *buf, size_t max_len) {
    if (key_len > KV_MAX_KEY_LEN) return -1;

    char ns[16], k[16];
    make_namespace(face_name, ns, sizeof(ns));
    if (!make_key(key, key_len, k, sizeof(k))) return -1;

    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return -1;

    size_t len = max_len;
    esp_err_t err = nvs_get_blob(h, k, buf, &len);
    nvs_close(h);

    if (err != ESP_OK) return -1;
    return (int)len;
}

int kv_store_del(const char *face_name, const void *key, size_t key_len) {
    if (key_len > KV_MAX_KEY_LEN) return -1;

    char ns[16], k[16];
    make_namespace(face_name, ns, sizeof(ns));
    if (!make_key(key, key_len, k, sizeof(k))) return -1;

    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return -1;

    esp_err_t err = nvs_erase_key(h, k);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK) return -1;
    return 0;
}

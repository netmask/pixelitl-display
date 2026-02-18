// Pixelitl Face SDK for AssemblyScript
// Compile: asc main.ts -o face.wasm --optimize --runtime stub --use abort=

// --- Gesture constants ---
export const GESTURE_NONE: i32 = 0;
export const GESTURE_TAP: i32 = 1;
export const GESTURE_SWIPE_LEFT: i32 = 2;
export const GESTURE_SWIPE_RIGHT: i32 = 3;
export const GESTURE_SWIPE_UP: i32 = 4;
export const GESTURE_SWIPE_DOWN: i32 = 5;
export const GESTURE_LONG_PRESS: i32 = 6;

// --- HTTP status ---
export const HTTP_FREE: i32 = 0;
export const HTTP_PENDING: i32 = 1;
export const HTTP_IN_PROGRESS: i32 = 2;
export const HTTP_DONE: i32 = 3;
export const HTTP_ERROR: i32 = 4;

// --- Face install status ---
export const INSTALL_IDLE: i32 = 0;
export const INSTALL_DOWNLOADING: i32 = 1;
export const INSTALL_LOADING: i32 = 2;
export const INSTALL_OK: i32 = 3;
export const INSTALL_ERROR: i32 = -1;

// --- Graphics ---
@external("env", "clear")
export declare function clear(color: u32): void;

@external("env", "set_pixel")
export declare function set_pixel(x: i32, y: i32, color: u32): void;

@external("env", "fill_rect")
export declare function fill_rect(x: i32, y: i32, w: i32, h: i32, color: u32): void;

@external("env", "draw_rect")
export declare function draw_rect(x: i32, y: i32, w: i32, h: i32, color: u32): void;

@external("env", "draw_line_h")
export declare function draw_line_h(x: i32, y: i32, len: i32, color: u32): void;

@external("env", "draw_line_v")
export declare function draw_line_v(x: i32, y: i32, len: i32, color: u32): void;

@external("env", "draw_circle")
export declare function draw_circle(cx: i32, cy: i32, r: i32, color: u32): void;

@external("env", "fill_circle")
export declare function fill_circle(cx: i32, cy: i32, r: i32, color: u32): void;

@external("env", "draw_text")
declare function _draw_text(x: i32, y: i32, ptr: usize, len: i32, color: u32): i32;

@external("env", "draw_text_big")
declare function _draw_text_big(x: i32, y: i32, ptr: usize, len: i32, color: u32): i32;

@external("env", "blit_framebuffer")
export declare function blit_framebuffer(ptr: usize): void;

@external("env", "blit_rect")
export declare function blit_rect(ptr: usize, dx: i32, dy: i32, w: i32, h: i32): void;

// --- Color ---
@external("env", "rgb")
export declare function rgb(r: i32, g: i32, b: i32): u32;

@external("env", "hsv")
export declare function hsv(h: f32, s: f32, v: f32): u32;

@external("env", "dim_color")
export declare function dim_color(color: u32, factor: f32): u32;

@external("env", "lerp_color")
export declare function lerp_color(c1: u32, c2: u32, t: f32): u32;

// --- Math ---
@external("env", "sinf")
export declare function sinf(x: f32): f32;

@external("env", "cosf")
export declare function cosf(x: f32): f32;

@external("env", "sqrtf")
export declare function sqrtf(x: f32): f32;

@external("env", "atan2f")
export declare function atan2f(y: f32, x: f32): f32;

@external("env", "fmodf")
export declare function fmodf(x: f32, y: f32): f32;

@external("env", "absf")
export declare function absf(x: f32): f32;

@external("env", "rand")
export declare function rand(): i32;

@external("env", "srand")
export declare function srand(seed: i32): void;

// --- System ---
@external("env", "set_resolution")
export declare function set_resolution(mode: i32): void;

@external("env", "millis")
export declare function millis(): i64;

@external("env", "width")
export declare function width(): i32;

@external("env", "height")
export declare function height(): i32;

@external("env", "get_frame")
export declare function get_frame(): i32;

@external("env", "log_print")
declare function _log_print(ptr: usize, len: i32): void;

// --- Touch ---
@external("env", "get_gesture")
export declare function get_gesture(): i32;

@external("env", "get_touch_x")
export declare function get_touch_x(): i32;

@external("env", "get_touch_y")
export declare function get_touch_y(): i32;

@external("env", "is_pressed")
export declare function is_pressed(): i32;

// --- Easing ---
@external("env", "ease_in_quad")
export declare function ease_in_quad(t: f32): f32;

@external("env", "ease_out_quad")
export declare function ease_out_quad(t: f32): f32;

@external("env", "ease_in_out_quad")
export declare function ease_in_out_quad(t: f32): f32;

@external("env", "ease_in_cubic")
export declare function ease_in_cubic(t: f32): f32;

@external("env", "ease_out_cubic")
export declare function ease_out_cubic(t: f32): f32;

@external("env", "ease_in_out_cubic")
export declare function ease_in_out_cubic(t: f32): f32;

@external("env", "ease_out_elastic")
export declare function ease_out_elastic(t: f32): f32;

@external("env", "ease_out_bounce")
export declare function ease_out_bounce(t: f32): f32;

// --- WiFi ---
@external("env", "wifi_status")
export declare function wifi_status(): i32;

// --- HTTP ---
@external("env", "http_get")
declare function _http_get(ptr: usize, len: i32): i32;

@external("env", "http_post")
declare function _http_post(urlPtr: usize, urlLen: i32, ctPtr: usize, ctLen: i32, bodyPtr: usize, bodyLen: i32): i32;

@external("env", "http_status")
export declare function http_status(handle: i32): i32;

@external("env", "http_response_code")
export declare function http_response_code(handle: i32): i32;

@external("env", "http_response_len")
export declare function http_response_len(handle: i32): i32;

@external("env", "http_read")
export declare function http_read(handle: i32, dst: usize, maxLen: i32): i32;

@external("env", "http_close")
export declare function http_close(handle: i32): void;

// --- KV Store ---
@external("env", "kv_set")
declare function _kv_set(keyPtr: usize, keyLen: i32, valPtr: usize, valLen: i32): i32;

@external("env", "kv_get")
declare function _kv_get(keyPtr: usize, keyLen: i32, dstPtr: usize, maxLen: i32): i32;

@external("env", "kv_del")
declare function _kv_del(keyPtr: usize, keyLen: i32): i32;

// --- Face Install ---
@external("env", "face_install")
declare function _face_install(urlPtr: usize, urlLen: i32, namePtr: usize, nameLen: i32): i32;

@external("env", "face_install_status")
export declare function face_install_status(): i32;

@external("env", "face_install_reset")
export declare function face_install_reset(): void;

@external("env", "face_count")
export declare function face_count(): i32;

// ---- String helpers ----
// AssemblyScript strings use a managed layout. These helpers encode to a
// temporary raw buffer so the host (which expects a plain pointer+length)
// gets valid UTF-8 bytes.

// Small scratch buffer for string encoding (256 bytes at a fixed address).
// We place it at the very start of the static data region.
const _STR_BUF_PTR: usize = 1024;
const _STR_BUF_LEN: i32 = 256;

function _encodeStr(s: string): i32 {
  const len = min(s.length, _STR_BUF_LEN) as i32;
  for (let i: i32 = 0; i < len; i++) {
    store<u8>(_STR_BUF_PTR + <usize>i, s.charCodeAt(i) as u8);
  }
  return len;
}

/** Draw a string at (x, y) with the 5x7 font. */
export function draw_str(x: i32, y: i32, s: string, color: u32): i32 {
  const len = _encodeStr(s);
  return _draw_text(x, y, _STR_BUF_PTR, len, color);
}

/** Draw a string at (x, y) with the 10x14 big font. */
export function draw_str_big(x: i32, y: i32, s: string, color: u32): i32 {
  const len = _encodeStr(s);
  return _draw_text_big(x, y, _STR_BUF_PTR, len, color);
}

/** Print a string to the host console. */
export function log(s: string): void {
  const len = _encodeStr(s);
  _log_print(_STR_BUF_PTR, len);
}

/** Start an HTTP GET request. Returns a handle or -1. */
export function http_get(url: string): i32 {
  const len = _encodeStr(url);
  return _http_get(_STR_BUF_PTR, len);
}

/** Set a key-value pair in the per-face KV store. */
export function kv_set(key: string, value: string): i32 {
  const klen = _encodeStr(key);
  // Copy key out of scratch buffer
  const keyBuf = _STR_BUF_PTR + 256;
  memory.copy(keyBuf, _STR_BUF_PTR, klen);
  const vlen = _encodeStr(value);
  return _kv_set(keyBuf, klen, _STR_BUF_PTR, vlen);
}

/** Get a value from the per-face KV store. Returns bytes read or -1. */
export function kv_get(key: string, dst: usize, maxLen: i32): i32 {
  const klen = _encodeStr(key);
  return _kv_get(_STR_BUF_PTR, klen, dst, maxLen);
}

/** Delete a key from the per-face KV store. */
export function kv_del(key: string): i32 {
  const len = _encodeStr(key);
  return _kv_del(_STR_BUF_PTR, len);
}

/** Start an HTTP POST request. Returns a handle or -1. */
export function http_post(url: string, contentType: string, body: string): i32 {
  // Encode URL
  const ulen = _encodeStr(url);
  const urlBuf: usize = _STR_BUF_PTR + 512;
  memory.copy(urlBuf, _STR_BUF_PTR, ulen);
  // Encode content type
  const ctlen = _encodeStr(contentType);
  const ctBuf: usize = _STR_BUF_PTR + 768;
  memory.copy(ctBuf, _STR_BUF_PTR, ctlen);
  // Encode body
  const blen = _encodeStr(body);
  return _http_post(urlBuf, ulen, ctBuf, ctlen, _STR_BUF_PTR, blen);
}

/** Install a face from a URL. Returns 0 on success, -1 on error. */
export function face_install(url: string, name: string): i32 {
  const ulen = _encodeStr(url);
  const urlBuf: usize = _STR_BUF_PTR + 512;
  memory.copy(urlBuf, _STR_BUF_PTR, ulen);
  const nlen = _encodeStr(name);
  return _face_install(urlBuf, ulen, _STR_BUF_PTR, nlen);
}

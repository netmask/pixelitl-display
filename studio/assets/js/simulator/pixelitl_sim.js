// Pixelitl Simulator Engine — extracted from pixelitl-simulator.html
// Encapsulated as a class for use in LiveView hooks

const VFB_STD_W = 80, VFB_STD_H = 48, STD_SCALE = 10;
const VFB_HD_W = 160, VFB_HD_H = 96, HD_SCALE = 5;
const LCD_W = 800, LCD_H = 480;

// 5x7 Font (column-major)
const FONT_5X7 = [
[0x00,0x00,0x00,0x00,0x00],[0x00,0x00,0x5F,0x00,0x00],[0x00,0x07,0x00,0x07,0x00],
[0x14,0x7F,0x14,0x7F,0x14],[0x24,0x2A,0x7F,0x2A,0x12],[0x23,0x13,0x08,0x64,0x62],
[0x36,0x49,0x55,0x22,0x50],[0x00,0x05,0x03,0x00,0x00],[0x00,0x1C,0x22,0x41,0x00],
[0x00,0x41,0x22,0x1C,0x00],[0x14,0x08,0x3E,0x08,0x14],[0x08,0x08,0x3E,0x08,0x08],
[0x00,0x50,0x30,0x00,0x00],[0x08,0x08,0x08,0x08,0x08],[0x00,0x60,0x60,0x00,0x00],
[0x20,0x10,0x08,0x04,0x02],[0x3E,0x51,0x49,0x45,0x3E],[0x00,0x42,0x7F,0x40,0x00],
[0x42,0x61,0x51,0x49,0x46],[0x21,0x41,0x45,0x4B,0x31],[0x18,0x14,0x12,0x7F,0x10],
[0x27,0x45,0x45,0x45,0x39],[0x3C,0x4A,0x49,0x49,0x30],[0x01,0x71,0x09,0x05,0x03],
[0x36,0x49,0x49,0x49,0x36],[0x06,0x49,0x49,0x29,0x1E],[0x00,0x36,0x36,0x00,0x00],
[0x00,0x56,0x36,0x00,0x00],[0x08,0x14,0x22,0x41,0x00],[0x14,0x14,0x14,0x14,0x14],
[0x00,0x41,0x22,0x14,0x08],[0x02,0x01,0x51,0x09,0x06],[0x32,0x49,0x79,0x41,0x3E],
[0x7E,0x11,0x11,0x11,0x7E],[0x7F,0x49,0x49,0x49,0x36],[0x3E,0x41,0x41,0x41,0x22],
[0x7F,0x41,0x41,0x22,0x1C],[0x7F,0x49,0x49,0x49,0x41],[0x7F,0x09,0x09,0x09,0x01],
[0x3E,0x41,0x49,0x49,0x7A],[0x7F,0x08,0x08,0x08,0x7F],[0x00,0x41,0x7F,0x41,0x00],
[0x20,0x40,0x41,0x3F,0x01],[0x7F,0x08,0x14,0x22,0x41],[0x7F,0x40,0x40,0x40,0x40],
[0x7F,0x02,0x0C,0x02,0x7F],[0x7F,0x04,0x08,0x10,0x7F],[0x3E,0x41,0x41,0x41,0x3E],
[0x7F,0x09,0x09,0x09,0x06],[0x3E,0x41,0x51,0x21,0x5E],[0x7F,0x09,0x19,0x29,0x46],
[0x46,0x49,0x49,0x49,0x31],[0x01,0x01,0x7F,0x01,0x01],[0x3F,0x40,0x40,0x40,0x3F],
[0x1F,0x20,0x40,0x20,0x1F],[0x3F,0x40,0x38,0x40,0x3F],[0x63,0x14,0x08,0x14,0x63],
[0x07,0x08,0x70,0x08,0x07],[0x61,0x51,0x49,0x45,0x43],[0x00,0x7F,0x41,0x41,0x00],
[0x02,0x04,0x08,0x10,0x20],[0x00,0x41,0x41,0x7F,0x00],[0x04,0x02,0x01,0x02,0x04],
[0x40,0x40,0x40,0x40,0x40],[0x00,0x01,0x02,0x04,0x00],[0x20,0x54,0x54,0x54,0x78],
[0x7F,0x48,0x44,0x44,0x38],[0x38,0x44,0x44,0x44,0x20],[0x38,0x44,0x44,0x48,0x7F],
[0x38,0x54,0x54,0x54,0x18],[0x08,0x7E,0x09,0x01,0x02],[0x0C,0x52,0x52,0x52,0x3E],
[0x7F,0x08,0x04,0x04,0x78],[0x00,0x44,0x7D,0x40,0x00],[0x20,0x40,0x44,0x3D,0x00],
[0x7F,0x10,0x28,0x44,0x00],[0x00,0x41,0x7F,0x40,0x00],[0x7C,0x04,0x18,0x04,0x78],
[0x7C,0x08,0x04,0x04,0x78],[0x38,0x44,0x44,0x44,0x38],[0x7C,0x14,0x14,0x14,0x08],
[0x08,0x14,0x14,0x18,0x7C],[0x7C,0x08,0x04,0x04,0x08],[0x48,0x54,0x54,0x54,0x20],
[0x04,0x3F,0x44,0x40,0x20],[0x3C,0x40,0x40,0x20,0x7C],[0x1C,0x20,0x40,0x20,0x1C],
[0x3C,0x40,0x30,0x40,0x3C],[0x44,0x28,0x10,0x28,0x44],[0x0C,0x50,0x50,0x50,0x3C],
[0x44,0x64,0x54,0x4C,0x44],[0x00,0x08,0x36,0x41,0x00],[0x00,0x00,0x7F,0x00,0x00],
[0x00,0x41,0x36,0x08,0x00],[0x10,0x08,0x08,0x10,0x08]
];

export class PixelitlSimulator {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.imageData = this.ctx.createImageData(LCD_W, LCD_H);
    this.pixels = new Uint32Array(this.imageData.data.buffer);
    this.vfb = new Uint16Array(VFB_HD_W * VFB_HD_H);

    this.curW = VFB_STD_W;
    this.curH = VFB_STD_H;
    this.curScale = STD_SCALE;

    this.faces = {};
    this.activeFace = null;
    this.frameCount = 0;
    this.glowMode = 'grid';
    this.running = true;

    // Gesture state
    this.gestureState = 0;
    this.touchX = -1;
    this.touchY = -1;
    this.isPressed = false;
    this.gestureStartX = 0;
    this.gestureStartY = 0;
    this.gestureStartTime = 0;

    // FPS tracking
    this.lastTime = 0;
    this.fpsFrames = 0;
    this.fpsLast = 0;
    this.statUpdate = 0;
    this.statDraw = 0;
    this.statRender = 0;
    this.currentFps = 0;

    // Console
    this.consoleLines = [];
    this.onConsole = null;
    this.onStats = null;
    this.onFps = null;

    // HTTP slots
    this.httpSlots = Array.from({length: 4}, () => ({
      state: 0, response: null, statusCode: 0, url: '', method: 'GET',
      contentType: '', body: null
    }));

    this._setupInput();
  }

  _setupInput() {
    const c = this.canvas;
    c.addEventListener('mousedown', e => {
      const r = c.getBoundingClientRect();
      this.touchX = Math.floor(((e.clientX - r.left) / r.width) * this.curW);
      this.touchY = Math.floor(((e.clientY - r.top) / r.height) * this.curH);
      this.isPressed = true;
      this.gestureStartX = e.clientX;
      this.gestureStartY = e.clientY;
      this.gestureStartTime = performance.now();
    });
    c.addEventListener('mousemove', e => {
      if (!this.isPressed) return;
      const r = c.getBoundingClientRect();
      this.touchX = Math.floor(((e.clientX - r.left) / r.width) * this.curW);
      this.touchY = Math.floor(((e.clientY - r.top) / r.height) * this.curH);
    });
    c.addEventListener('mouseup', e => {
      if (!this.isPressed) return;
      const dx = e.clientX - this.gestureStartX;
      const dy = e.clientY - this.gestureStartY;
      const dur = performance.now() - this.gestureStartTime;
      if (Math.abs(dx) > 30 || Math.abs(dy) > 30) {
        this.gestureState = Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 3 : 2) : (dy > 0 ? 5 : 4);
      } else if (dur < 300) this.gestureState = 1;
      else if (dur > 500) this.gestureState = 6;
      this.touchX = -1; this.touchY = -1; this.isPressed = false;
    });
    c.addEventListener('mouseleave', () => {
      if (this.isPressed) { this.touchX = -1; this.touchY = -1; this.isPressed = false; }
    });
  }

  _consumeGesture() { const g = this.gestureState; this.gestureState = 0; return g; }

  // Color conversion
  _rgb565to32(c) {
    const r = ((c >> 11) & 0x1F) * 255 / 31 | 0;
    const g = ((c >> 5) & 0x3F) * 255 / 63 | 0;
    const b = (c & 0x1F) * 255 / 31 | 0;
    return 0xFF000000 | (b << 16) | (g << 8) | r;
  }
  _toRGB565(r, g, b) { return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); }
  _hsvToRGB565(h, s, v) {
    let c = v * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = v - c;
    let r, g, b;
    if (h < 60)       { r=c; g=x; b=0; }
    else if (h < 120) { r=x; g=c; b=0; }
    else if (h < 180) { r=0; g=c; b=x; }
    else if (h < 240) { r=0; g=x; b=c; }
    else if (h < 300) { r=x; g=0; b=c; }
    else              { r=c; g=0; b=x; }
    return this._toRGB565((r+m)*255|0, (g+m)*255|0, (b+m)*255|0);
  }
  _dimRGB565(color, factor) {
    let r = ((color >> 11) & 0x1F) * factor | 0;
    let g = ((color >> 5) & 0x3F) * factor | 0;
    let b = (color & 0x1F) * factor | 0;
    return (r << 11) | (g << 5) | b;
  }
  _lerpRGB565(c1, c2, t) {
    let r1 = (c1>>11)&0x1F, r2 = (c2>>11)&0x1F;
    let g1 = (c1>>5)&0x3F, g2 = (c2>>5)&0x3F;
    let b1 = c1&0x1F, b2 = c2&0x1F;
    return ((r1 + (r2-r1)*t | 0) << 11) | ((g1 + (g2-g1)*t | 0) << 5) | (b1 + (b2-b1)*t | 0);
  }

  // Graphics ops on vfb
  _clear(color) {
    const c = color & 0xFFFF, count = this.curW * this.curH;
    for (let i = 0; i < count; i++) this.vfb[i] = c;
  }
  _setPixel(x, y, color) {
    if (x >= 0 && x < this.curW && y >= 0 && y < this.curH)
      this.vfb[y * this.curW + x] = color & 0xFFFF;
  }
  _fillRect(x, y, w, h, color) {
    const c = color & 0xFFFF;
    const x0 = Math.max(0, x), y0 = Math.max(0, y);
    const x1 = Math.min(this.curW, x + w), y1 = Math.min(this.curH, y + h);
    for (let ry = y0; ry < y1; ry++)
      for (let rx = x0; rx < x1; rx++)
        this.vfb[ry * this.curW + rx] = c;
  }
  _drawLineH(x, y, len, color) {
    if (y < 0 || y >= this.curH) return;
    const c = color & 0xFFFF;
    const x0 = Math.max(0, x), x1 = Math.min(this.curW, x + len);
    for (let rx = x0; rx < x1; rx++) this.vfb[y * this.curW + rx] = c;
  }
  _drawLineV(x, y, len, color) {
    if (x < 0 || x >= this.curW) return;
    const c = color & 0xFFFF;
    const y0 = Math.max(0, y), y1 = Math.min(this.curH, y + len);
    for (let ry = y0; ry < y1; ry++) this.vfb[ry * this.curW + x] = c;
  }
  _drawRect(x, y, w, h, color) {
    this._drawLineH(x, y, w, color);
    this._drawLineH(x, y+h-1, w, color);
    this._drawLineV(x, y, h, color);
    this._drawLineV(x+w-1, y, h, color);
  }
  _drawCircle(cx, cy, r, color) {
    let x = r, y = 0, d = 1 - r;
    while (x >= y) {
      this._setPixel(cx+x,cy+y,color); this._setPixel(cx-x,cy+y,color);
      this._setPixel(cx+x,cy-y,color); this._setPixel(cx-x,cy-y,color);
      this._setPixel(cx+y,cy+x,color); this._setPixel(cx-y,cy+x,color);
      this._setPixel(cx+y,cy-x,color); this._setPixel(cx-y,cy-x,color);
      y++;
      if (d <= 0) d += 2*y+1;
      else { x--; d += 2*(y-x)+1; }
    }
  }
  _fillCircle(cx, cy, r, color) {
    let x = r, y = 0, d = 1 - r;
    while (x >= y) {
      this._drawLineH(cx-x, cy+y, 2*x+1, color);
      this._drawLineH(cx-x, cy-y, 2*x+1, color);
      this._drawLineH(cx-y, cy+x, 2*y+1, color);
      this._drawLineH(cx-y, cy-x, 2*y+1, color);
      y++;
      if (d <= 0) d += 2*y+1;
      else { x--; d += 2*(y-x)+1; }
    }
  }
  _drawText(x, y, ptr, len, color, memory) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    let cx = x;
    for (let i = 0; i < len; i++) {
      const ch = bytes[i];
      if (ch >= 32 && ch <= 126) {
        const glyph = FONT_5X7[ch - 32];
        for (let col = 0; col < 5; col++)
          for (let row = 0; row < 7; row++)
            if (glyph[col] & (1 << row))
              this._setPixel(cx + col, y + row, color);
      }
      cx += 6;
    }
    return cx - x;
  }
  _drawTextBig(x, y, ptr, len, color, memory) {
    const bytes = new Uint8Array(memory.buffer, ptr, len);
    let cx = x;
    for (let i = 0; i < len; i++) {
      const ch = bytes[i];
      if (ch >= 32 && ch <= 126) {
        const glyph = FONT_5X7[ch - 32];
        for (let col = 0; col < 5; col++)
          for (let row = 0; row < 7; row++)
            if (glyph[col] & (1 << row)) {
              const px = cx + col*2, py = y + row*2;
              this._setPixel(px,py,color); this._setPixel(px+1,py,color);
              this._setPixel(px,py+1,color); this._setPixel(px+1,py+1,color);
            }
      }
      cx += 12;
    }
    return cx - x;
  }

  // HTTP
  _httpStartRequest(idx) {
    const slot = this.httpSlots[idx];
    slot.state = 2; // IN_PROGRESS
    const opts = { method: slot.method };
    if (slot.method === 'POST' && slot.body) {
      opts.headers = { 'Content-Type': slot.contentType };
      opts.body = slot.body;
    }
    fetch(slot.url, opts)
      .then(r => { slot.statusCode = r.status; return r.arrayBuffer(); })
      .then(buf => { slot.response = new Uint8Array(buf.slice(0, 16384)); slot.state = 3; })
      .catch(() => { slot.state = 4; });
  }
  _httpFindFreeSlot() {
    for (let i = 0; i < 4; i++) if (this.httpSlots[i].state === 0) return i;
    return -1;
  }

  // KV Store (localStorage based)
  _kvPrefix(name) { return `pxl_${name}_`; }
  _kvEncode(data) {
    let binary = '';
    for (let i = 0; i < data.length; i++) binary += String.fromCharCode(data[i]);
    return btoa(binary);
  }
  _kvDecode(str) {
    const binary = atob(str);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return bytes;
  }

  log(msg) {
    this.consoleLines.push(msg);
    if (this.consoleLines.length > 200) this.consoleLines.shift();
    if (this.onConsole) this.onConsole(msg);
  }

  async loadFaceFromUrl(name, url) {
    try {
      const resp = await fetch(url);
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      const bytes = new Uint8Array(await resp.arrayBuffer());
      await this.loadFace(name, bytes);
    } catch(e) {
      this.log(`Error fetching ${name}: ${e.message}`);
    }
  }

  async loadFace(name, wasmBytes) {
    const sim = this;
    const importedMemory = new WebAssembly.Memory({ initial: 2, maximum: 4 });
    let faceHD = false;

    // Mutable ref — updated after instantiation to instance.exports.memory
    // so host functions read from the WASM module's actual memory, not the
    // (potentially unused) imported one.
    const mem = { m: importedMemory };

    const faceCount = Object.keys(this.faces).length;

    const imports = {
      env: {
        memory: importedMemory,
        clear: c => sim._clear(c),
        set_pixel: (x,y,c) => sim._setPixel(x,y,c),
        fill_rect: (x,y,w,h,c) => sim._fillRect(x,y,w,h,c),
        draw_rect: (x,y,w,h,c) => sim._drawRect(x,y,w,h,c),
        draw_line_h: (x,y,l,c) => sim._drawLineH(x,y,l,c),
        draw_line_v: (x,y,l,c) => sim._drawLineV(x,y,l,c),
        draw_circle: (cx,cy,r,c) => sim._drawCircle(cx,cy,r,c),
        fill_circle: (cx,cy,r,c) => sim._fillCircle(cx,cy,r,c),
        draw_text: (x,y,p,l,c) => sim._drawText(x,y,p,l,c, mem.m),
        draw_text_big: (x,y,p,l,c) => sim._drawTextBig(x,y,p,l,c, mem.m),
        blit_framebuffer: (ptr) => {
          const count = sim.curW * sim.curH;
          const src = new Uint16Array(mem.m.buffer, ptr, count);
          sim.vfb.set(src);
        },
        blit_rect: (ptr, dx, dy, w, h) => {
          const src = new Uint16Array(mem.m.buffer, ptr, w * h);
          for (let row = 0; row < h; row++) {
            const sy = dy + row;
            if (sy < 0 || sy >= sim.curH) continue;
            for (let col = 0; col < w; col++) {
              const sx = dx + col;
              if (sx < 0 || sx >= sim.curW) continue;
              sim.vfb[sy * sim.curW + sx] = src[row * w + col];
            }
          }
        },
        rgb: (r,g,b) => sim._toRGB565(r,g,b),
        hsv: (h,s,v) => sim._hsvToRGB565(h,s,v),
        dim_color: (c,f) => sim._dimRGB565(c,f),
        lerp_color: (c1,c2,t) => sim._lerpRGB565(c1,c2,t),
        sinf: Math.sin, cosf: Math.cos, sqrtf: Math.sqrt,
        atan2f: Math.atan2,
        fmodf: (x,y) => ((x % y) + y) % y,
        absf: Math.abs,
        rand: () => (Math.random() * 32767) | 0,
        srand: () => {},
        millis: () => BigInt(Math.floor(performance.now())),
        width: () => sim.curW,
        height: () => sim.curH,
        set_resolution: (mode) => {
          faceHD = (mode === 1);
          if (faceHD) { sim.curW = VFB_HD_W; sim.curH = VFB_HD_H; sim.curScale = HD_SCALE; }
          else { sim.curW = VFB_STD_W; sim.curH = VFB_STD_H; sim.curScale = STD_SCALE; }
        },
        log_print: (ptr, len) => {
          const bytes = new Uint8Array(mem.m.buffer, ptr, len);
          sim.log(`[${name}] ${new TextDecoder().decode(bytes)}`);
        },
        get_frame: () => sim.frameCount,
        get_gesture: () => sim._consumeGesture(),
        get_touch_x: () => sim.touchX,
        get_touch_y: () => sim.touchY,
        is_pressed: () => sim.isPressed ? 1 : 0,
        // Easing
        ease_in_quad: t => t * t,
        ease_out_quad: t => 1 - (1 - t) * (1 - t),
        ease_in_out_quad: t => t < 0.5 ? 2 * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) * 0.5,
        ease_in_cubic: t => t * t * t,
        ease_out_cubic: t => { let u = 1 - t; return 1 - u * u * u; },
        ease_in_out_cubic: t => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) * 0.5,
        ease_out_elastic: t => {
          if (t <= 0) return 0; if (t >= 1) return 1;
          const p = 0.3;
          return Math.pow(2, -10 * t) * Math.sin((t - p / 4) * (2 * Math.PI) / p) + 1;
        },
        ease_out_bounce: t => {
          if (t < 1/2.75) return 7.5625*t*t;
          if (t < 2/2.75) { t -= 1.5/2.75; return 7.5625*t*t + 0.75; }
          if (t < 2.5/2.75) { t -= 2.25/2.75; return 7.5625*t*t + 0.9375; }
          t -= 2.625/2.75; return 7.5625*t*t + 0.984375;
        },
        wifi_status: () => 1,
        // HTTP
        http_get: (ptr, len) => {
          const url = new TextDecoder().decode(new Uint8Array(mem.m.buffer, ptr, len));
          const idx = sim._httpFindFreeSlot();
          if (idx < 0) return -1;
          sim.httpSlots[idx].url = url;
          sim.httpSlots[idx].method = 'GET';
          sim.httpSlots[idx].body = null;
          sim.httpSlots[idx].state = 1;
          sim._httpStartRequest(idx);
          return idx;
        },
        http_post: (urlPtr, urlLen, ctPtr, ctLen, bodyPtr, bodyLen) => {
          const url = new TextDecoder().decode(new Uint8Array(mem.m.buffer, urlPtr, urlLen));
          const ct = new TextDecoder().decode(new Uint8Array(mem.m.buffer, ctPtr, ctLen));
          const body = new Uint8Array(mem.m.buffer, bodyPtr, bodyLen).slice();
          const idx = sim._httpFindFreeSlot();
          if (idx < 0) return -1;
          Object.assign(sim.httpSlots[idx], {url, method: 'POST', contentType: ct, body, state: 1});
          sim._httpStartRequest(idx);
          return idx;
        },
        http_status: h => (h >= 0 && h < 4) ? sim.httpSlots[h].state : 0,
        http_response_code: h => (h >= 0 && h < 4 && sim.httpSlots[h].state === 3) ? sim.httpSlots[h].statusCode : -1,
        http_response_len: h => (h >= 0 && h < 4 && sim.httpSlots[h].state === 3 && sim.httpSlots[h].response) ? sim.httpSlots[h].response.length : -1,
        http_read: (h, dstPtr, maxLen) => {
          if (h < 0 || h >= 4 || sim.httpSlots[h].state !== 3 || !sim.httpSlots[h].response) return -1;
          const data = sim.httpSlots[h].response;
          const len = Math.min(data.length, maxLen);
          new Uint8Array(mem.m.buffer, dstPtr, len).set(data.subarray(0, len));
          return len;
        },
        http_close: h => {
          if (h >= 0 && h < 4) {
            sim.httpSlots[h].state = 0;
            sim.httpSlots[h].response = null;
            sim.httpSlots[h].body = null;
          }
        },
        // KV Store
        kv_set: (keyPtr, keyLen, valPtr, valLen) => {
          try {
            const key = new TextDecoder().decode(new Uint8Array(mem.m.buffer, keyPtr, keyLen));
            const val = new Uint8Array(mem.m.buffer, valPtr, valLen).slice();
            localStorage.setItem(sim._kvPrefix(name) + key, sim._kvEncode(val));
            return 0;
          } catch(e) { return -1; }
        },
        kv_get: (keyPtr, keyLen, dstPtr, maxLen) => {
          try {
            const key = new TextDecoder().decode(new Uint8Array(mem.m.buffer, keyPtr, keyLen));
            const stored = localStorage.getItem(sim._kvPrefix(name) + key);
            if (stored === null) return -1;
            const data = sim._kvDecode(stored);
            const len = Math.min(data.length, maxLen);
            new Uint8Array(mem.m.buffer, dstPtr, len).set(data.subarray(0, len));
            return len;
          } catch(e) { return -1; }
        },
        kv_del: (keyPtr, keyLen) => {
          try {
            const key = new TextDecoder().decode(new Uint8Array(mem.m.buffer, keyPtr, keyLen));
            localStorage.removeItem(sim._kvPrefix(name) + key);
            return 0;
          } catch(e) { return -1; }
        },
        // Face install stubs
        face_install: () => 0,
        face_install_status: () => 3,
        face_install_reset: () => {},
        face_count: () => faceCount,
      }
    };

    try {
      sim.curW = VFB_STD_W; sim.curH = VFB_STD_H; sim.curScale = STD_SCALE;
      faceHD = false;

      const { instance } = await WebAssembly.instantiate(wasmBytes, imports);

      // Use the instance's own memory if it exports one (modules compiled with
      // --initial-memory define their own memory instead of importing ours).
      const actualMemory = instance.exports.memory || importedMemory;
      mem.m = actualMemory;

      instance.exports.app_init();
      sim.faces[name] = {
        instance,
        update: instance.exports.app_update,
        draw: instance.exports.app_draw,
        memory: actualMemory,
        hd: faceHD,
      };
      if (!sim.activeFace) sim.activeFace = name;
      sim.log(`Loaded face: ${name}` + (faceHD ? ' (HD 160x96)' : ' (80x48)'));
      return true;
    } catch (e) {
      sim.log(`Error loading ${name}: ${e.message}`);
      return false;
    }
  }

  setActiveFace(name) {
    if (this.faces[name]) {
      this.activeFace = name;
    }
  }

  renderToCanvas() {
    const mode = this.glowMode;
    const p = this.pixels;
    const W = this.curW, H = this.curH, S = this.curScale;

    if (mode === 'flat') {
      for (let vy = 0; vy < H; vy++) {
        for (let vx = 0; vx < W; vx++) {
          const c32 = this._rgb565to32(this.vfb[vy * W + vx]);
          const bx = vx * S, by = vy * S;
          for (let dy = 0; dy < S; dy++) {
            const off = (by + dy) * LCD_W + bx;
            for (let dx = 0; dx < S; dx++) p[off + dx] = c32;
          }
        }
      }
    } else if (mode === 'grid') {
      for (let vy = 0; vy < H; vy++) {
        for (let vx = 0; vx < W; vx++) {
          const c = this.vfb[vy * W + vx];
          const c32 = this._rgb565to32(c);
          const r = ((c>>11)&0x1F)*255/31*0.3|0;
          const g = ((c>>5)&0x3F)*255/63*0.3|0;
          const b = (c&0x1F)*255/31*0.3|0;
          const dim32 = 0xFF000000|(b<<16)|(g<<8)|r;
          const bx = vx * S, by = vy * S;
          for (let dy = 0; dy < S; dy++) {
            const off = (by + dy) * LCD_W + bx;
            const isEdgeY = dy === S - 1;
            for (let dx = 0; dx < S; dx++) {
              p[off + dx] = (isEdgeY || dx === S - 1) ? dim32 : c32;
            }
          }
        }
      }
    } else if (mode === 'glow') {
      const halfS = S * 0.5;
      const invHalf = 1.0 / halfS;
      for (let vy = 0; vy < H; vy++) {
        for (let vx = 0; vx < W; vx++) {
          const c = this.vfb[vy * W + vx];
          const c32 = this._rgb565to32(c);
          const bx = vx * S, by = vy * S;
          if (c === 0) {
            for (let dy = 0; dy < S; dy++) {
              const off = (by + dy) * LCD_W + bx;
              for (let dx = 0; dx < S; dx++) p[off + dx] = c32;
            }
          } else {
            for (let dy = 0; dy < S; dy++) {
              for (let dx = 0; dx < S; dx++) {
                const distX = Math.abs(dx - (halfS - 0.5)) * invHalf;
                const distY = Math.abs(dy - (halfS - 0.5)) * invHalf;
                const dist = Math.sqrt(distX*distX + distY*distY);
                const dim = Math.max(0, 1 - dist * 0.4);
                const off = (by + dy) * LCD_W + bx + dx;
                const or_ = c32 & 0xFF;
                const og = (c32 >> 8) & 0xFF;
                const ob = (c32 >> 16) & 0xFF;
                p[off] = 0xFF000000 | ((Math.min(255, ob * dim | 0)) << 16) |
                         ((Math.min(255, og * dim | 0)) << 8) | (Math.min(255, or_ * dim | 0));
              }
            }
          }
        }
      }
    }
    this.ctx.putImageData(this.imageData, 0, 0);
  }

  start() {
    this.running = true;
    this._rafId = requestAnimationFrame(ts => this._loop(ts));
  }

  stop() {
    this.running = false;
    if (this._rafId) cancelAnimationFrame(this._rafId);
  }

  _loop(timestamp) {
    if (!this.running) return;
    const dt = timestamp - this.lastTime;
    this.lastTime = timestamp;

    if (this.activeFace && this.faces[this.activeFace]) {
      const face = this.faces[this.activeFace];
      if (face.hd) {
        this.curW = VFB_HD_W; this.curH = VFB_HD_H; this.curScale = HD_SCALE;
      } else {
        this.curW = VFB_STD_W; this.curH = VFB_STD_H; this.curScale = STD_SCALE;
      }

      const t0 = performance.now();
      face.update(this.frameCount, Math.floor(dt));
      const t1 = performance.now();
      face.draw();
      const t2 = performance.now();
      this.renderToCanvas();
      const t3 = performance.now();

      this.statUpdate = t1 - t0;
      this.statDraw = t2 - t1;
      this.statRender = t3 - t2;
      this.frameCount++;
    }

    this.fpsFrames++;
    if (timestamp - this.fpsLast >= 1000) {
      this.currentFps = this.fpsFrames;
      this.fpsFrames = 0;
      this.fpsLast = timestamp;
      if (this.onFps) this.onFps(this.currentFps);
      if (this.onStats) this.onStats(this._getStats());
    }

    this._rafId = requestAnimationFrame(ts => this._loop(ts));
  }

  _getStats() {
    const face = this.activeFace ? this.faces[this.activeFace] : null;
    const resLabel = face ? (face.hd ? '160x96 (HD)' : '80x48') : '-';
    return {
      frame: this.frameCount,
      resolution: resLabel,
      update: this.statUpdate.toFixed(2),
      draw: this.statDraw.toFixed(2),
      render: this.statRender.toFixed(2),
      total: (this.statUpdate + this.statDraw + this.statRender).toFixed(2),
      memory: face ? (face.memory.buffer.byteLength / 1024).toFixed(0) + 'KB' : '-',
    };
  }
}

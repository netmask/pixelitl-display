import { PixelitlSimulator } from "../simulator/pixelitl_sim.js";

const MiniSimulatorHook = {
  mounted() {
    const canvas = this.el;
    this.sim = new PixelitlSimulator(canvas);
    this.sim.glowMode = 'grid';

    // Wire up glow mode selector
    const glowSelect = document.getElementById('editor-glow-mode');
    if (glowSelect) {
      this.sim.glowMode = glowSelect.value;
      glowSelect.addEventListener('change', () => {
        this.sim.glowMode = glowSelect.value;
      });
    }

    // FPS display
    const fpsEl = document.getElementById('editor-fps');
    this.sim.onFps = (fps) => {
      if (fpsEl) fpsEl.textContent = fps + ' FPS';
    };

    // Console output
    const consoleEl = document.getElementById('editor-console');
    this.sim.onConsole = (msg) => {
      if (consoleEl) {
        consoleEl.textContent += msg + '\n';
        consoleEl.scrollTop = consoleEl.scrollHeight;
      }
    };

    // Stats display
    const statsEl = document.getElementById('editor-stats');
    this.sim.onStats = (stats) => {
      if (statsEl) {
        statsEl.innerHTML = `
          Frame: <span class="text-primary">${stats.frame}</span><br>
          Res: <span class="text-primary">${stats.resolution}</span><br>
          Update: <span class="text-primary">${stats.update}ms</span>
          Draw: <span class="text-primary">${stats.draw}ms</span>
          Render: <span class="text-primary">${stats.render}ms</span><br>
          Total: <span class="text-primary">${stats.total}ms</span>
          Mem: <span class="text-primary">${stats.memory}</span>
        `;
      }
    };

    this._loadFace();
    this.sim.start();

    // Listen for wasm_compiled events to hot-reload the face
    this.handleEvent("wasm_compiled", async ({ slug }) => {
      const name = slug;
      // Clear console on reload
      const consoleEl = document.getElementById('editor-console');
      if (consoleEl) consoleEl.textContent = '';

      // Remove old face and reload
      delete this.sim.faces[name];
      this.sim.activeFace = null;
      this.sim.frameCount = 0;
      const url = `/api/faces/${slug}.wasm?t=${Date.now()}`;
      await this.sim.loadFaceFromUrl(name, url);
      this.sim.setActiveFace(name);
    });
  },

  async _loadFace() {
    const slug = this.el.dataset.faceSlug;
    if (slug) {
      const url = `/api/faces/${slug}.wasm`;
      await this.sim.loadFaceFromUrl(slug, url);
      this.sim.setActiveFace(slug);
    }
  },

  destroyed() {
    if (this.sim) this.sim.stop();
  }
};

export default MiniSimulatorHook;

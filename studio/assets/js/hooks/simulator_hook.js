import { PixelitlSimulator } from "../simulator/pixelitl_sim.js";

const SimulatorHook = {
  mounted() {
    const canvas = this.el;
    this.sim = new PixelitlSimulator(canvas);

    // Glow mode selector
    const glowSelect = document.getElementById('glow-mode');
    if (glowSelect) {
      this.sim.glowMode = glowSelect.value;
      glowSelect.addEventListener('change', () => {
        this.sim.glowMode = glowSelect.value;
      });
    }

    // Console output
    const consoleEl = document.getElementById('console-output');
    this.sim.onConsole = (msg) => {
      if (consoleEl) {
        consoleEl.textContent += msg + '\n';
        consoleEl.scrollTop = consoleEl.scrollHeight;
      }
    };

    // FPS display
    const fpsEl = document.getElementById('fps-counter');
    this.sim.onFps = (fps) => {
      if (fpsEl) fpsEl.textContent = fps + ' FPS';
    };

    // Stats display
    const statsEl = document.getElementById('stats-content');
    this.sim.onStats = (stats) => {
      if (statsEl) {
        statsEl.innerHTML = `
          Frame: <span class="text-primary">${stats.frame}</span><br>
          Resolution: <span class="text-primary">${stats.resolution}</span><br>
          Update: <span class="text-primary">${stats.update}ms</span><br>
          Draw: <span class="text-primary">${stats.draw}ms</span><br>
          Render: <span class="text-primary">${stats.render}ms</span><br>
          Total: <span class="text-primary">${stats.total}ms</span><br>
          Memory: <span class="text-primary">${stats.memory}</span>
        `;
      }
    };

    // Drag & drop
    const dropOverlay = document.getElementById('drop-overlay');
    const wrap = document.getElementById('canvas-wrap');
    if (wrap && dropOverlay) {
      wrap.addEventListener('dragover', e => { e.preventDefault(); dropOverlay.style.display = 'flex'; });
      wrap.addEventListener('dragleave', e => {
        if (!wrap.contains(e.relatedTarget)) dropOverlay.style.display = 'none';
      });
      wrap.addEventListener('drop', async e => {
        e.preventDefault();
        dropOverlay.style.display = 'none';
        for (const file of e.dataTransfer.files) {
          if (file.name.endsWith('.wasm')) {
            const bytes = new Uint8Array(await file.arrayBuffer());
            await this.sim.loadFace(file.name.replace('.wasm', ''), bytes);
          }
        }
      });
    }

    // Load faces from data attribute
    this._loadFaces();

    this.sim.start();
  },

  async _loadFaces() {
    const facesData = JSON.parse(this.el.dataset.faces || '[]');
    const selectedId = this.el.dataset.selected;

    for (const face of facesData) {
      const url = `/api/faces/${face.slug}.wasm`;
      await this.sim.loadFaceFromUrl(face.name, url);
    }

    // Select the right face
    if (selectedId) {
      const selected = facesData.find(f => f.id === parseInt(selectedId));
      if (selected) this.sim.setActiveFace(selected.name);
    }
  },

  updated() {
    // When the selected face changes from LiveView
    const selectedId = this.el.dataset.selected;
    const facesData = JSON.parse(this.el.dataset.faces || '[]');
    if (selectedId) {
      const selected = facesData.find(f => f.id === parseInt(selectedId));
      if (selected && this.sim.faces[selected.name]) {
        this.sim.setActiveFace(selected.name);
      }
    }
  },

  destroyed() {
    if (this.sim) this.sim.stop();
  }
};

export default SimulatorHook;

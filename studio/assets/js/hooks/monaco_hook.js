let monacoLoaded = false;
let monacoLoadPromise = null;

function loadMonaco() {
  if (monacoLoaded) return Promise.resolve();
  if (monacoLoadPromise) return monacoLoadPromise;

  monacoLoadPromise = new Promise((resolve) => {
    const script = document.createElement('script');
    script.src = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js';
    script.onload = () => {
      window.require.config({
        paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }
      });
      window.require(['vs/editor/editor.main'], () => {
        monacoLoaded = true;
        resolve();
      });
    };
    document.head.appendChild(script);
  });
  return monacoLoadPromise;
}

const MonacoHook = {
  async mounted() {
    await loadMonaco();

    const language = this.el.dataset.language || 'c';
    const content = this.el.dataset.content || '';

    this.editor = monaco.editor.create(this.el, {
      value: content,
      language: language,
      theme: 'vs-dark',
      minimap: { enabled: false },
      fontSize: 13,
      fontFamily: "'SF Mono', 'Menlo', 'Consolas', monospace",
      lineNumbers: 'on',
      scrollBeyondLastLine: false,
      automaticLayout: true,
      tabSize: 4,
      wordWrap: 'off',
    });

    // Auto-save on change (debounced)
    this._saveTimer = null;
    this.editor.onDidChangeModelContent(() => {
      clearTimeout(this._saveTimer);
      this._saveTimer = setTimeout(() => {
        this.pushEvent("save_content", { content: this.editor.getValue() });
      }, 500);
    });

    // Ctrl+S to compile
    this.editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
      // Save immediately
      this.pushEvent("save_content", { content: this.editor.getValue() });
      // Then compile
      this.pushEvent("compile", {});
    });

    // Listen for compile errors to set markers
    this.handleEvent("compile_errors", ({ errors }) => {
      const model = this.editor.getModel();
      if (!model) return;
      const markers = errors.map(err => ({
        severity: err.severity === 'error'
          ? monaco.MarkerSeverity.Error
          : monaco.MarkerSeverity.Warning,
        startLineNumber: err.line,
        startColumn: err.column,
        endLineNumber: err.line,
        endColumn: err.column + 1,
        message: err.message,
      }));
      monaco.editor.setModelMarkers(model, 'compiler', markers);
    });

    // Listen for successful compile - clear markers
    this.handleEvent("wasm_compiled", () => {
      const model = this.editor.getModel();
      if (model) monaco.editor.setModelMarkers(model, 'compiler', []);
    });

    this._currentFileId = this.el.dataset.fileId;
  },

  updated() {
    if (!this.editor) return;

    const newFileId = this.el.dataset.fileId;
    if (newFileId !== this._currentFileId) {
      this._currentFileId = newFileId;
      const content = this.el.dataset.content || '';
      const language = this.el.dataset.language || 'c';

      this.editor.setValue(content);
      const model = this.editor.getModel();
      if (model) {
        monaco.editor.setModelLanguage(model, language);
        monaco.editor.setModelMarkers(model, 'compiler', []);
      }
    }
  },

  destroyed() {
    if (this.editor) this.editor.dispose();
    clearTimeout(this._saveTimer);
  }
};

export default MonacoHook;

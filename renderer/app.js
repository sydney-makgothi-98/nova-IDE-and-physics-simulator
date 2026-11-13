'use strict';
/* NOVA IDE – renderer. Vanilla JS + Monaco. */
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const IS_WIN = navigator.userAgent.includes('Windows');
const SEP = IS_WIN ? '\\' : '/';
const esc = (s) => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const key = (p) => IS_WIN ? p.replace(/\//g, '\\').toLowerCase() : p;
const base = (p) => p.split(/[\\/]/).filter(Boolean).pop() || p;
const dirname = (p) => p.replace(/[\\/][^\\/]*$/, '');
const join = (...a) => a.join(SEP).replace(/[\\/]+/g, SEP).replace(IS_WIN ? /^\\(?!\\)/ : /^$/, m => m);
const ext = (p) => (base(p).includes('.') ? base(p).split('.').pop().toLowerCase() : '');
const rel = (p) => S.root && key(p).startsWith(key(S.root)) ? p.slice(S.root.length + 1) : p;
function normPath(p) {
  const drive = /^[A-Za-z]:/.test(p) ? p.slice(0, 2) : '';
  const parts = p.slice(drive.length).split(/[\\/]/); const out = [];
  for (const x of parts) { if (x === '..') out.pop(); else if (x && x !== '.') out.push(x); }
  return drive + SEP + out.join(SEP);
}
const isAbs = (p) => /^([A-Za-z]:[\\/]|\/|\\\\)/.test(p);

const S = {
  root: null, tabs: [], active: null, settings: {}, tools: {}, expanded: new Set(), selected: null,
  config: { type: 'file' }, targets: [], cmakeDirty: true, problems: [], breaks: {},
  fileCache: null, termCwd: null, termHist: [], termHistI: 0, running: null, bottom: null, unbuf: null
};
const DEFAULTS = { theme: 'dark', fontSize: 14, fontFamily: 'JetBrains Mono, Cascadia Code, Consolas, monospace', ligatures: true, minimap: true, cppStd: 'c++17', cStd: 'c17', unbuffered: true, recent: [], autosave: true };

/* ---------- utilities ---------- */
function toast(msg, kind = '', ms = 3800) {
  const t = document.createElement('div'); t.className = 'toast ' + kind; t.innerHTML = msg; $('#toasts').appendChild(t);
  setTimeout(() => t.remove(), ms);
}
function showMenu(x, y, items) {
  const m = $('#menu'); m.innerHTML = '';
  for (const it of items) {
    if (it === '-') { m.appendChild(document.createElement('hr')); continue; }
    const d = document.createElement('div'); d.className = 'mi' + (it.danger ? ' danger' : '');
    d.innerHTML = `<span>${esc(it.label)}</span><span class="k">${it.key || ''}</span>`;
    d.onclick = () => { m.hidden = true; it.run(); }; m.appendChild(d);
  }
  m.hidden = false;
  const r = m.getBoundingClientRect();
  m.style.left = Math.min(x, innerWidth - r.width - 8) + 'px'; m.style.top = Math.min(y, innerHeight - r.height - 8) + 'px';
}
document.addEventListener('mousedown', (e) => { if (!e.target.closest('#menu')) $('#menu').hidden = true; });

function modal(html, { onMount, width } = {}) {
  const ov = $('#overlay'); ov.innerHTML = `<div class="modal" ${width ? `style="width:${width}"` : ''}>${html}</div>`; ov.hidden = false;
  ov.onmousedown = (e) => { if (e.target === ov) closeModal(); };
  onMount && onMount(ov.firstChild); return ov.firstChild;
}
function closeModal() { const ov = $('#overlay'); ov.hidden = true; ov.innerHTML = ''; ov.onkeydown = null; S.editor && S.editor.focus(); }
function promptModal(title, value = '', hint = '') {
  return new Promise(res => {
    const m = modal(`<h3>${esc(title)}</h3><div class="body"><div class="field"><input type="text" id="pmv" spellcheck="false" value="${esc(value)}" />${hint ? `<label>${esc(hint)}</label>` : ''}</div></div>
      <div class="foot"><button class="btn" id="pmc">Cancel</button><button class="btn primary" id="pmo">OK</button></div>`);
    const i = $('#pmv', m); i.focus(); i.select();
    const done = (v) => { closeModal(); res(v); };
    $('#pmc', m).onclick = () => done(null); $('#pmo', m).onclick = () => done(i.value.trim() || null);
    i.onkeydown = (e) => { if (e.key === 'Enter') done(i.value.trim() || null); if (e.key === 'Escape') done(null); };
  });
}
function choiceModal(title, msg, buttons) {
  return new Promise(res => {
    const m = modal(`<h3>${esc(title)}</h3><div class="body"><div>${esc(msg)}</div></div><div class="foot">${buttons.map((b, i) => `<button class="btn ${b.primary ? 'primary' : ''}" data-i="${i}">${esc(b.label)}</button>`).join('')}</div>`);
    $$('button', m).forEach(b => b.onclick = () => { closeModal(); res(buttons[b.dataset.i].value); });
    $('#overlay').onkeydown = (e) => { if (e.key === 'Escape') { closeModal(); res(null); } };
  });
}

/* ---------- file icons / languages ---------- */
const FI = {
  c: ['C', '#5b8def'], cpp: ['C+', '#3f7fe0'], cc: ['C+', '#3f7fe0'], cxx: ['C+', '#3f7fe0'], h: ['H', '#a46fe0'], hpp: ['H+', '#a46fe0'], hh: ['H+', '#a46fe0'], hxx: ['H+', '#a46fe0'],
  txt: ['T', '#7d828c'], md: ['M', '#5aa469'], json: ['{}', '#cfa03a'], cmake: ['CM', '#d4553c'], py: ['Py', '#3d7ab8'], js: ['JS', '#c9a227'], ts: ['TS', '#3178c6'],
  html: ['<>', '#e0723f'], css: ['#', '#6a5acd'], sh: ['sh', '#5aa469'], bat: ['bt', '#5aa469'], yml: ['Y', '#c0525b'], yaml: ['Y', '#c0525b'], exe: ['▶', '#7d828c'], o: ['o', '#7d828c']
};
function fileIcon(ent) {
  if (ent.dir) return `<span class="fi dir">${icon('folder')}</span>`;
  const e = base(ent.path || ent.name) === 'CMakeLists.txt' ? 'cmake' : ext(ent.name);
  const [t, c] = FI[e] || ['', '#7d828c'];
  return t ? `<span class="fi" style="background:${c}">${esc(t)}</span>` : `<span class="fi dir" style="color:#7d828c">${icon('file')}</span>`;
}
function langOf(p) {
  const n = base(p), e = ext(p);
  if (n === 'CMakeLists.txt' || e === 'cmake') return 'cmake';
  return ({ c: 'c', h: 'cpp', cpp: 'cpp', cc: 'cpp', cxx: 'cpp', hpp: 'cpp', hh: 'cpp', hxx: 'cpp', ino: 'cpp', json: 'json', md: 'markdown', py: 'python', js: 'javascript', ts: 'typescript', html: 'html', css: 'css', xml: 'xml', yml: 'yaml', yaml: 'yaml', sh: 'shell', bat: 'bat', ps1: 'powershell', txt: 'plaintext' })[e] || 'plaintext';
}
const LANG_LABEL = { c: 'C', cpp: 'C++', cmake: 'CMake', plaintext: 'Plain Text', json: 'JSON', markdown: 'Markdown', python: 'Python', javascript: 'JavaScript', typescript: 'TypeScript', html: 'HTML', css: 'CSS', xml: 'XML', yaml: 'YAML', shell: 'Shell', bat: 'Batch', powershell: 'PowerShell' };

/* ---------- boot ---------- */
async function boot() {
  hydrateIcons();
  S.settings = { ...DEFAULTS, ...(await nova.loadSettings()) };
  applyTheme();
  S.tools = await nova.detectTools();
  updateToolchain();
  const b = new URL('../node_modules/monaco-editor/min/', location.href).href;
  window.MonacoEnvironment = { getWorkerUrl: () => 'data:text/javascript;charset=utf-8,' + encodeURIComponent(`self.MonacoEnvironment={baseUrl:'${b}'};importScripts('${b}vs/base/worker/workerMain.js');`) };
  require.config({ paths: { vs: b + 'vs' } });
  require(['vs/editor/editor.main'], () => { initMonaco(); wireUI(); renderRecent(); renderWelcomeState(); });
}
function applyTheme() {
  document.documentElement.dataset.theme = S.settings.theme;
  if (window.monaco) { monaco.editor.setTheme(S.settings.theme === 'dark' ? 'nova-dark' : 'nova-light'); }
  if (typeof termApplyTheme === 'function') termApplyTheme();
}
function updateToolchain() {
  const t = S.tools; const v = (x) => (x && x.version.match(/(\d+\.\d+(\.\d+)?)\s*$/) || [])[1] || '';
  const parts = [];
  const cc = t['g++'] ? 'g++ ' + v(t['g++']) : t['clang++'] ? 'clang++ ' + v(t['clang++']) : null;
  parts.push(cc || 'no compiler'); if (t.cmake) parts.push('CMake ' + (t.cmake.version.match(/(\d+\.\d+\.\d+)/) || [])[1]);
  $('#stToolchain').textContent = parts.join(' · ');
  $('#stToolchain').title = Object.entries(t).map(([k, x]) => `${k}: ${x.path}`).join('\n');
}

/* ---------- Monaco ---------- */
function initMonaco() {
  const C_KW = 'auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while bool true false NULL'.split(' ');
  const CPP_KW = C_KW.concat('class namespace template typename using public private protected virtual override final constexpr noexcept nullptr new delete this try catch throw operator explicit friend mutable static_cast dynamic_cast reinterpret_cast const_cast decltype concept requires co_await co_return co_yield'.split(' '));
  monaco.languages.register({ id: 'cmake' });
  monaco.languages.setMonarchTokensProvider('cmake', {
    ignoreCase: true,
    keywords: ['cmake_minimum_required', 'project', 'add_executable', 'add_library', 'target_link_libraries', 'target_include_directories', 'target_compile_options', 'target_compile_definitions', 'set', 'if', 'else', 'elseif', 'endif', 'foreach', 'endforeach', 'function', 'endfunction', 'macro', 'endmacro', 'include', 'find_package', 'add_subdirectory', 'option', 'message', 'file', 'install', 'list', 'string', 'while', 'endwhile', 'return', 'include_directories', 'link_directories', 'add_definitions', 'enable_testing', 'add_test', 'add_custom_command', 'add_custom_target'],
    tokenizer: { root: [[/#.*$/, 'comment'], [/\$\{[^}]*\}/, 'variable'], [/"([^"\\]|\\.)*"/, 'string'], [/\b\d+(\.\d+)*\b/, 'number'], [/[A-Za-z_][\w]*(?=\s*\()/, { cases: { '@keywords': 'keyword', '@default': 'predefined' } }], [/[A-Z_]{3,}\b/, 'type']] }
  });
  monaco.languages.setLanguageConfiguration('cmake', { comments: { lineComment: '#' }, brackets: [['(', ')']], autoClosingPairs: [{ open: '(', close: ')' }, { open: '"', close: '"' }, { open: '{', close: '}' }] });

  const snip = (label, body, doc) => ({ label, insertText: body, insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet, kind: monaco.languages.CompletionItemKind.Snippet, documentation: doc, detail: 'snippet' });
  const SN_C = [
    snip('main', 'int main(int argc, char** argv) {\n\t$0\n\treturn 0;\n}', 'main function'),
    snip('for', 'for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t$0\n}', 'for loop'),
    snip('while', 'while (${1:cond}) {\n\t$0\n}'), snip('if', 'if (${1:cond}) {\n\t$0\n}'), snip('ifelse', 'if (${1:cond}) {\n\t$2\n} else {\n\t$0\n}'),
    snip('switch', 'switch (${1:x}) {\ncase ${2:0}:\n\t$0\n\tbreak;\ndefault:\n\tbreak;\n}'),
    snip('struct', 'typedef struct ${1:Name} {\n\t$0\n} ${1:Name};'), snip('printf', 'printf("${1:%d}\\n", ${2:x});'),
    snip('guard', '#ifndef ${1:HEADER_H}\n#define ${1:HEADER_H}\n\n$0\n\n#endif // ${1:HEADER_H}'), snip('inc', '#include <${1:stdio.h}>'), snip('incl', '#include "${1:file.h}"'),
    snip('malloc', '${1:int}* ${2:p} = (${1:int}*)malloc(${3:n} * sizeof(${1:int}));\nif (!${2:p}) { $0 }')
  ];
  const SN_CPP = SN_C.filter(s => !['struct', 'printf'].includes(s.label)).concat([
    snip('cout', 'std::cout << ${1:x} << std::endl;'), snip('cin', 'std::cin >> ${1:x};'), snip('class', 'class ${1:Name} {\npublic:\n\t${1:Name}();\n\t~${1:Name}();\n\nprivate:\n\t$0\n};'),
    snip('struct', 'struct ${1:Name} {\n\t$0\n};'), snip('forr', 'for (const auto& ${1:x} : ${2:container}) {\n\t$0\n}'), snip('vec', 'std::vector<${1:int}> ${2:v};'),
    snip('lambda', '[${1:&}](${2}) {\n\t$0\n}'), snip('uptr', 'auto ${1:p} = std::make_unique<${2:T}>(${3});'), snip('sptr', 'auto ${1:p} = std::make_shared<${2:T}>(${3});'),
    snip('ns', 'namespace ${1:name} {\n$0\n}'), snip('tmpl', 'template <typename ${1:T}>\n$0'), snip('map', 'std::unordered_map<${1:std::string}, ${2:int}> ${3:m};')
  ]);
  const HDR_C = 'stdio.h stdlib.h string.h math.h ctype.h time.h stdint.h stdbool.h stddef.h limits.h assert.h errno.h signal.h stdarg.h float.h locale.h setjmp.h'.split(' ');
  const HDR_CPP = 'iostream string vector map unordered_map set unordered_set list deque queue stack array algorithm numeric functional memory utility tuple optional variant any chrono thread mutex atomic future fstream sstream iomanip cmath cstdio cstdlib cstring cstdint cassert random regex filesystem span ranges bitset format'.split(' ');
  for (const lang of ['c', 'cpp']) {
    monaco.languages.registerCompletionItemProvider(lang, {
      triggerCharacters: ['<', '"', '/'],
      provideCompletionItems(model, pos) {
        const line = model.getLineContent(pos.lineNumber).slice(0, pos.column - 1);
        const w = model.getWordUntilPosition(pos);
        const range = { startLineNumber: pos.lineNumber, endLineNumber: pos.lineNumber, startColumn: w.startColumn, endColumn: w.endColumn };
        if (/#\s*include\s*[<"][^>"]*$/.test(line)) {
          return { suggestions: (lang === 'c' ? HDR_C : HDR_C.concat(HDR_CPP)).map(h => ({ label: h, kind: monaco.languages.CompletionItemKind.File, insertText: h, range })) };
        }
        const kws = (lang === 'c' ? C_KW : CPP_KW).map(k => ({ label: k, kind: monaco.languages.CompletionItemKind.Keyword, insertText: k, range }));
        return { suggestions: (lang === 'c' ? SN_C : SN_CPP).map(s => ({ ...s, range })).concat(kws) };
      }
    });
  }

  const common = { 'editor.background': '#1e1f22', 'editor.foreground': '#dfe1e5', 'editorLineNumber.foreground': '#4b5059', 'editorLineNumber.activeForeground': '#a1a3ab', 'editor.lineHighlightBackground': '#26282e', 'editor.selectionBackground': '#214283', 'editor.inactiveSelectionBackground': '#2e436e88', 'editorCursor.foreground': '#ced0d6', 'editorIndentGuide.background1': '#2e3033', 'editorIndentGuide.activeBackground1': '#4b5059', 'editorWidget.background': '#2b2d30', 'editorSuggestWidget.background': '#2b2d30', 'editorSuggestWidget.selectedBackground': '#2e436e', 'editorSuggestWidget.border': '#393b40', 'editorHoverWidget.background': '#2b2d30', 'editorHoverWidget.border': '#393b40', 'scrollbarSlider.background': '#86868644', 'minimap.background': '#1e1f22', 'editorGutter.background': '#1e1f22', 'editorBracketMatch.background': '#3b514d', 'editorBracketMatch.border': '#00000000', 'input.background': '#1e1f22', 'focusBorder': '#3574f0' };
  monaco.editor.defineTheme('nova-dark', {
    base: 'vs-dark', inherit: true, colors: common, rules: [
      { token: 'comment', foreground: '7a7e85', fontStyle: 'italic' }, { token: 'keyword', foreground: 'cf8e6d' }, { token: 'string', foreground: '6aab73' }, { token: 'string.escape', foreground: 'cf8e6d' },
      { token: 'number', foreground: '2aacb8' }, { token: 'number.float', foreground: '2aacb8' }, { token: 'number.hex', foreground: '2aacb8' }, { token: 'type', foreground: 'dfe1e5' }, { token: 'type.identifier', foreground: 'dfe1e5' },
      { token: 'metatag', foreground: 'b3ae60' }, { token: 'metatag.cpp', foreground: 'b3ae60' }, { token: 'predefined', foreground: '56a8f5' }, { token: 'variable', foreground: 'c77dbb' }, { token: 'delimiter', foreground: 'bcbec4' }, { token: 'identifier', foreground: 'bcbec4' }, { token: 'operator', foreground: 'bcbec4' }, { token: 'regexp', foreground: '42c3d4' }
    ]
  });
  monaco.editor.defineTheme('nova-light', {
    base: 'vs', inherit: true, colors: { 'editor.background': '#ffffff', 'editorLineNumber.foreground': '#b0b3ba', 'editor.lineHighlightBackground': '#f6f8fc', 'editor.selectionBackground': '#a6d2ff', 'editorGutter.background': '#ffffff', 'focusBorder': '#3574f0' }, rules: [
      { token: 'comment', foreground: '8c8c8c', fontStyle: 'italic' }, { token: 'keyword', foreground: '0033b3' }, { token: 'string', foreground: '067d17' }, { token: 'number', foreground: '1750eb' }, { token: 'metatag', foreground: '9e880d' }, { token: 'type', foreground: '000000' }, { token: 'predefined', foreground: '00627a' }
    ]
  });

  S.editor = monaco.editor.create($('#editor'), {
    model: null, theme: S.settings.theme === 'dark' ? 'nova-dark' : 'nova-light', automaticLayout: true, glyphMargin: true,
    fontSize: S.settings.fontSize, fontFamily: S.settings.fontFamily, fontLigatures: S.settings.ligatures, minimap: { enabled: S.settings.minimap, renderCharacters: false },
    smoothScrolling: true, cursorSmoothCaretAnimation: 'on', cursorBlinking: 'smooth', bracketPairColorization: { enabled: true }, guides: { bracketPairs: true },
    renderLineHighlight: 'all', padding: { top: 8 }, scrollBeyondLastLine: false, lineNumbersMinChars: 3, roundedSelection: true, mouseWheelZoom: true, fixedOverflowWidgets: true, tabSize: 4, insertSpaces: true, stickyScroll: { enabled: true }
  });
  S.bpDeco = S.editor.createDecorationsCollection();
  lspRegisterProviders(); dbgInit();
  S.editor.onDidChangeCursorPosition(e => { $('#stPos').textContent = `${e.position.lineNumber}:${e.position.column}`; });
  S.editor.onMouseDown(e => {
    if (e.target.type === monaco.editor.MouseTargetType.GUTTER_GLYPH_MARGIN && S.active) toggleBreak(S.active.path, e.target.position.lineNumber);
  });
}

/* ---------- settings ---------- */
function saveSettings() { nova.saveSettings(S.settings); }
function applyEditorSettings() {
  if (!S.editor) return;
  S.editor.updateOptions({ fontSize: S.settings.fontSize, fontFamily: S.settings.fontFamily, fontLigatures: S.settings.ligatures, minimap: { enabled: S.settings.minimap } });
  applyTheme();
}
function openSettings() {
  const st = S.settings;
  const m = modal(`<h3>Settings</h3><div class="body">
    <div class="field"><label>Theme</label><select id="sTheme"><option value="dark">NOVA Dark (Darcula-style)</option><option value="light">NOVA Light</option></select></div>
    <div class="field"><label>Editor font family</label><input type="text" id="sFont" value="${esc(st.fontFamily)}" /></div>
    <div class="field"><div class="row2"><div style="flex:1"><label>Font size</label><input type="number" id="sSize" min="9" max="32" value="${st.fontSize}" /></div>
      <div style="flex:1"><label>C++ standard</label><select id="sCpp">${['c++11', 'c++14', 'c++17', 'c++20', 'c++23'].map(x => `<option ${x === st.cppStd ? 'selected' : ''}>${x}</option>`).join('')}</select></div>
      <div style="flex:1"><label>C standard</label><select id="sC">${['c99', 'c11', 'c17', 'c23'].map(x => `<option ${x === st.cStd ? 'selected' : ''}>${x}</option>`).join('')}</select></div></div></div>
    <div class="field"><label><input type="checkbox" id="sLig" ${st.ligatures ? 'checked' : ''}/> Font ligatures</label>
      <label><input type="checkbox" id="sMini" ${st.minimap ? 'checked' : ''}/> Show minimap</label>
      <label><input type="checkbox" id="sAuto" ${st.autosave ? 'checked' : ''}/> Auto-save when the window loses focus / before build</label>
      <label><input type="checkbox" id="sDbgCon" ${st.debugExtConsole !== false ? 'checked' : ''}/> Debug: run the program in its own console window (needed for programs that read input)</label>
      <label><input type="checkbox" id="sUnbuf" ${st.unbuffered ? 'checked' : ''}/> Unbuffered console output (prompts from printf show immediately)</label></div>
    </div><div class="foot"><button class="btn primary" id="sOk">Done</button></div>`);
  $('#sTheme', m).value = st.theme;
  const upd = () => {
    Object.assign(S.settings, { theme: $('#sTheme', m).value, fontFamily: $('#sFont', m).value, fontSize: +$('#sSize', m).value || 14, cppStd: $('#sCpp', m).value, cStd: $('#sC', m).value, ligatures: $('#sLig', m).checked, minimap: $('#sMini', m).checked, autosave: $('#sAuto', m).checked, unbuffered: $('#sUnbuf', m).checked, debugExtConsole: $('#sDbgCon', m).checked });
    applyEditorSettings(); saveSettings();
  };
  $$('select,input', m).forEach(e => e.onchange = upd); $('#sOk', m).onclick = () => { upd(); closeModal(); };
}

/* ---------- project handling ---------- */
async function openProject(dir) {
  if (!dir) return;
  if (S.tabs.some(t => t.dirty)) await saveAll();
  await dbgStop();
  [...S.tabs].forEach(t => closeTab(t, true));
  S.root = dir; S.fileCache = null; S.problems = []; S.expanded = new Set([dir]); S.cmakeDirty = true; S.config = { type: 'file' };
  S.settings.recent = [dir, ...(S.settings.recent || []).filter(r => key(r) !== key(dir))].slice(0, 8); saveSettings();
  $('#projectName').textContent = base(dir);
  document.title = base(dir) + ' – NOVA IDE';
  await buildTree(); await scanTargets(); updateConfigLabel(); renderProblems(); renderWelcomeState(); renderRecent();
  detectBranch(); if (T.term) termRestart();
  lspBoot();
  const main = ['main.cpp', 'main.c', 'src/main.cpp', 'src/main.c'].map(f => join(dir, f));
  for (const f of main) if (await nova.exists(f)) { openFile(f); break; }
}
async function detectBranch() {
  $('#branchChip').hidden = true;
  if (!S.tools.git && !(await nova.exists(join(S.root, '.git')))) return;
  const r = await execCapture('git', ['rev-parse', '--abbrev-ref', 'HEAD'], S.root);
  if (r.code === 0 && r.out.trim()) { $('#branchName').textContent = r.out.trim(); $('#branchChip').hidden = false; }
}
function renderWelcomeState() { $('#welcome').style.display = S.tabs.length ? 'none' : 'grid'; $('#crumbs').style.display = S.tabs.length ? 'flex' : 'none'; }
function renderRecent() {
  const r = S.settings.recent || []; const el = $('#recentList');
  el.innerHTML = r.length ? '<h4>Recent projects</h4>' : '';
  r.forEach(p => { const d = document.createElement('div'); d.className = 'r'; d.innerHTML = `<span>${esc(base(p))}</span><span>${esc(p)}</span>`; d.onclick = () => openProject(p); el.appendChild(d); });
}
function projectMenu(e) {
  const r = e.currentTarget.getBoundingClientRect();
  const items = [{ label: 'New Project…', run: newProjectDialog }, { label: 'Open Folder…', key: 'Ctrl+Shift+O', run: pickFolder }];
  if ((S.settings.recent || []).length) { items.push('-'); S.settings.recent.forEach(p => items.push({ label: base(p) + '  —  ' + p, run: () => openProject(p) })); }
  showMenu(r.left, r.bottom + 4, items);
}
async function pickFolder() { const d = await nova.openFolder(); if (d) openProject(d); }

function newProjectDialog() {
  nova.paths().then(({ docs }) => {
    const m = modal(`<h3>New Project</h3><div class="body">
      <div class="field"><label>Name</label><input type="text" id="npName" value="untitled" spellcheck="false" /></div>
      <div class="field"><label>Location</label><div class="row2"><input type="text" id="npLoc" value="${esc(docs)}" spellcheck="false" /><button class="btn" id="npBrowse">Browse…</button></div></div>
      <div class="field"><div class="row2"><div style="flex:1"><label>Language</label><select id="npLang"><option value="cpp">C++ Executable</option><option value="c">C Executable</option></select></div>
        <div style="flex:1"><label>Standard</label><select id="npStd"></select></div></div></div></div>
      <div class="foot"><button class="btn" id="npCancel">Cancel</button><button class="btn primary" id="npCreate">Create</button></div>`);
    const fillStd = () => { $('#npStd', m).innerHTML = ($('#npLang', m).value === 'cpp' ? ['11', '14', '17', '20', '23'] : ['99', '11', '17', '23']).map(x => `<option ${x === (($('#npLang', m).value === 'cpp') ? '17' : '17') ? 'selected' : ''}>${x}</option>`).join(''); };
    fillStd(); $('#npLang', m).onchange = fillStd; $('#npName', m).select();
    $('#npBrowse', m).onclick = async () => { const d = await nova.openFolder(); if (d) $('#npLoc', m).value = d; };
    $('#npCancel', m).onclick = closeModal;
    $('#npCreate', m).onclick = async () => {
      const name = $('#npName', m).value.trim(), loc = $('#npLoc', m).value.trim(), lang = $('#npLang', m).value, std = $('#npStd', m).value;
      if (!/^[A-Za-z_][\w\-]*$/.test(name)) return toast('Use letters, digits, <b>_</b> or <b>-</b> for the project name.', 'error');
      const dir = join(loc, name);
      try { await nova.create(dir, true); } catch (e) { return toast('Folder already exists: ' + esc(dir), 'error'); }
      const isCpp = lang === 'cpp', mainName = isCpp ? 'main.cpp' : 'main.c';
      const cm = `cmake_minimum_required(VERSION 3.16)\nproject(${name} LANGUAGES ${isCpp ? 'CXX' : 'C'})\n\nset(CMAKE_${isCpp ? 'CXX' : 'C'}_STANDARD ${std})\nset(CMAKE_${isCpp ? 'CXX' : 'C'}_STANDARD_REQUIRED ON)\n\nadd_executable(${name} ${mainName})\n`;
      const src = isCpp ? `#include <iostream>\n\nint main() {\n    std::cout << "Hello, NOVA!" << std::endl;\n    return 0;\n}\n` : `#include <stdio.h>\n\nint main(void) {\n    printf("Hello, NOVA!\\n");\n    return 0;\n}\n`;
      for (const [f, t] of [['CMakeLists.txt', cm], [mainName, src], ['.gitignore', 'cmake-build-*/\n.nova/\n']]) { await nova.create(join(dir, f), false); await nova.write(join(dir, f), t); }
      closeModal(); await openProject(dir);
    };
  });
}

/* ---------- tree ---------- */
async function buildTree() {
  const t = $('#tree'); t.innerHTML = '';
  if (!S.root) { t.innerHTML = '<div class="muted" style="padding:14px">No project open.<br><br><button class="btn" id="tOpen">Open Folder</button></div>'; $('#tOpen').onclick = pickFolder; return; }
  t.appendChild(await nodeEl({ name: base(S.root), path: S.root, dir: true }, 0));
}
async function nodeEl(ent, depth) {
  const wrap = document.createElement('div'), row = document.createElement('div'), kids = document.createElement('div');
  row.className = 'node'; row.dataset.path = ent.path; row.style.paddingLeft = (depth * 14 + 2) + 'px';
  row.innerHTML = `<span class="tw">${ent.dir ? icon('right') : ''}</span>${fileIcon(ent)}<span class="nm">${esc(ent.name)}</span>`;
  if (S.selected && key(S.selected) === key(ent.path)) row.classList.add('sel');
  wrap.append(row, kids);
  let loaded = false;
  const setOpen = async (open) => {
    row.classList.toggle('open', open); kids.hidden = !open;
    if (open) S.expanded.add(ent.path); else S.expanded.delete(ent.path);
    if (open && !loaded) { loaded = true; try { for (const c of await nova.readDir(ent.path)) kids.appendChild(await nodeEl(c, depth + 1)); } catch { } }
  };
  kids.hidden = true;
  if (ent.dir && S.expanded.has(ent.path)) await setOpen(true);
  row.onclick = (e) => {
    $$('.node.sel').forEach(n => n.classList.remove('sel')); row.classList.add('sel'); S.selected = ent.path;
    if (ent.dir) setOpen(kids.hidden); else openFile(ent.path);
  };
  row.oncontextmenu = (e) => {
    e.preventDefault(); S.selected = ent.path;
    const parent = ent.dir ? ent.path : dirname(ent.path);
    showMenu(e.clientX, e.clientY, [
      { label: 'New File…', run: () => newEntry(parent, false) }, { label: 'New Folder…', run: () => newEntry(parent, true) }, '-',
      ...(ent.dir ? [] : [{ label: 'Open', run: () => openFile(ent.path) }]),
      { label: 'Rename…', key: 'F2', run: () => renameEntry(ent) },
      { label: 'Reveal in Explorer', run: () => nova.reveal(ent.path) },
      ...(key(ent.path) === key(S.root) ? [] : ['-', { label: 'Delete', danger: true, run: () => deleteEntry(ent) }])
    ]);
  };
  return wrap;
}
async function newEntry(parent, isDir) {
  const name = await promptModal(isDir ? 'New Folder' : 'New File', '', `in ${rel(parent) || base(parent)}`); if (!name) return;
  const p = join(parent, name);
  try { await nova.create(p, isDir); S.expanded.add(parent); S.fileCache = null; await buildTree(); if (!isDir) openFile(p); } catch (e) { toast('Could not create: ' + esc(e.message), 'error'); }
}
async function renameEntry(ent) {
  const name = await promptModal('Rename', base(ent.path)); if (!name || name === base(ent.path)) return;
  const np = join(dirname(ent.path), name);
  try {
    await nova.rename(ent.path, np); S.fileCache = null;
    const t = S.tabs.find(t => key(t.path) === key(ent.path)); if (t) { t.path = np; t.name = name; renderTabs(); }
    await buildTree();
  } catch (e) { toast('Rename failed: ' + esc(e.message), 'error'); }
}
async function deleteEntry(ent) {
  const ok = await choiceModal('Delete', `Delete "${base(ent.path)}"${ent.dir ? ' and everything inside it' : ''}? This cannot be undone.`, [{ label: 'Cancel', value: false }, { label: 'Delete', primary: true, value: true }]);
  if (!ok) return;
  S.tabs.filter(t => key(t.path).startsWith(key(ent.path))).forEach(t => closeTab(t, true));
  await nova.del(ent.path); S.fileCache = null; await buildTree();
}

/* ---------- tabs / editor ---------- */
async function openFile(path, pos) {
  if (!S.editor) return;
  let tab = S.tabs.find(t => key(t.path) === key(path));
  if (!tab) {
    let r; try { r = await nova.read(path); } catch (e) { return toast('Cannot open: ' + esc(e.message), 'error'); }
    if (r.binary) return toast('Binary file – not opened in the editor.');
    const model = monaco.editor.createModel(r.text, langOf(path), monaco.Uri.file(path));
    tab = { path, name: base(path), model, dirty: false, saved: model.getAlternativeVersionId(), view: null };
    model.onDidChangeContent(() => {
      const d = model.getAlternativeVersionId() !== tab.saved; if (d !== tab.dirty) { tab.dirty = d; renderTabs(); }
      if (S.active === tab) scheduleStructure();
      lspDidChange(tab);
    });
    S.tabs.push(tab); lspDidOpen(tab);
  }
  activate(tab);
  if (pos) { S.editor.revealLineInCenter(pos.line); S.editor.setPosition({ lineNumber: pos.line, column: pos.col || 1 }); S.editor.focus(); }
  else S.editor.focus();
}
function activate(tab) {
  if (S.active && S.active !== tab) S.active.view = S.editor.saveViewState();
  S.active = tab; S.editor.setModel(tab.model); if (tab.view) S.editor.restoreViewState(tab.view);
  S.selected = tab.path;
  renderTabs(); renderCrumbs(); renderWelcomeState(); applyMarkers(); applyBreaks(); scheduleStructure(true); dbgDecorate();
  $('#stLang').textContent = LANG_LABEL[tab.model.getLanguageId()] || tab.model.getLanguageId();
  const n = $(`.node[data-path="${CSS.escape(tab.path)}"]`); $$('.node.sel').forEach(x => x.classList.remove('sel')); if (n) { n.classList.add('sel'); n.scrollIntoView({ block: 'nearest' }); }
}
function renderTabs() {
  const el = $('#tabs'); el.innerHTML = '';
  for (const t of S.tabs) {
    const d = document.createElement('div'); d.className = 'tab' + (t === S.active ? ' active' : '') + (t.dirty ? ' dirty' : ''); d.title = t.path;
    d.innerHTML = `${fileIcon({ path: t.path, name: t.name })}<span>${esc(t.name)}</span><span class="dot"></span><span class="x">${icon('x')}</span>`;
    d.onmousedown = (e) => { if (e.button === 1) { e.preventDefault(); closeTab(t); } };
    d.onclick = (e) => { if (e.target.closest('.x')) closeTab(t); else activate(t); };
    el.appendChild(d); if (t === S.active) d.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  }
}
function renderCrumbs() {
  const t = S.active; if (!t) return;
  const parts = rel(t.path).split(/[\\/]/); $('#crumbs').innerHTML = (S.root && key(t.path).startsWith(key(S.root)) ? [`<span>${esc(base(S.root))}</span>`] : []).concat(parts.map((p, i) => i === parts.length - 1 ? `<b>${esc(p)}</b>` : `<span>${esc(p)}</span>`)).join(' <span>›</span> ');
}
async function closeTab(tab, force) {
  if (!force && tab.dirty) {
    const c = await choiceModal('Unsaved changes', `Save changes to ${tab.name}?`, [{ label: 'Cancel', value: null }, { label: "Don't Save", value: 'no' }, { label: 'Save', primary: true, value: 'yes' }]);
    if (!c) return; if (c === 'yes') await saveTab(tab);
  }
  const i = S.tabs.indexOf(tab); S.tabs.splice(i, 1);
  if (S.active === tab) { S.active = null; const n = S.tabs[Math.min(i, S.tabs.length - 1)]; if (n) activate(n); else { S.editor.setModel(null); renderTabs(); renderWelcomeState(); renderStructure([]); $('#stLang').textContent = ''; } }
  else renderTabs();
  lspDidClose(tab); setTimeout(() => tab.model.dispose(), 0);
}
async function saveTab(tab) {
  await nova.write(tab.path, tab.model.getValue()); tab.saved = tab.model.getAlternativeVersionId(); tab.dirty = false; renderTabs(); lspDidSave(tab);
  if (base(tab.path) === 'CMakeLists.txt') { S.cmakeDirty = true; scanTargets(); }
}
async function saveAll() { for (const t of S.tabs.filter(t => t.dirty)) await saveTab(t); }

/* breakpoints (gutter) */
function toggleBreak(path, line) {
  const k = key(path); const set = S.breaks[k] || (S.breaks[k] = new Set());
  const had = set.has(line); had ? set.delete(line) : set.add(line); applyBreaks(); dbgSyncBreak(path, line, !had);
}
function applyBreaks() {
  if (!S.active) return; const set = S.breaks[key(S.active.path)] || new Set();
  S.bpDeco.set([...set].map(l => ({ range: new monaco.Range(l, 1, l, 1), options: { glyphMarginClassName: 'bp-glyph', glyphMarginHoverMessage: { value: 'Breakpoint' } } })));
}

/* ---------- structure view ---------- */
let structTimer;
function scheduleStructure(now) { clearTimeout(structTimer); structTimer = setTimeout(computeStructure, now ? 0 : 400); }
const CTRL = new Set(['if', 'for', 'while', 'switch', 'return', 'else', 'catch', 'sizeof', 'do', 'new', 'delete', 'throw', 'defined', 'elif', 'case']);
async function computeStructure() {
  const t = S.active; if (!t || !['c', 'cpp'].includes(t.model.getLanguageId())) return renderStructure([]);
  const ls = await lspStructure(t); if (ls) return renderStructure(ls);
  const lines = t.model.getLinesContent(), syms = [];
  let inBlock = false;
  lines.forEach((ln, i) => {
    if (inBlock) { if (ln.includes('*/')) inBlock = false; return; }
    if (/^\s*\/\*/.test(ln) && !ln.includes('*/')) { inBlock = true; return; }
    if (/^\s*\/\//.test(ln)) return;
    let m;
    if ((m = ln.match(/^\s*(?:template\s*<[^>]*>\s*)?(class|struct|union|enum(?:\s+class)?|namespace)\s+(\w+)/)) && !/;\s*$/.test(ln)) return syms.push({ k: m[1][0] === 'c' ? 'c' : m[1][0] === 's' ? 's' : m[1][0] === 'e' ? 'e' : m[1][0] === 'n' ? 'n' : 'u', name: m[2], line: i + 1 });
    if ((m = ln.match(/^\s*#\s*define\s+(\w+)/))) return syms.push({ k: 'm', name: m[1], line: i + 1 });
    if ((m = ln.match(/^\s*(?:[\w:<>,\*&~\s]+?)\s+[\*&]*([A-Za-z_~][\w:~]*|operator\s*\S+)\s*\(([^;{}]*)\)?\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*(\{|$)/)) && !ln.trim().endsWith(';') && !CTRL.has(m[1]) && !/^\s*(return|else|using|typedef|delete|throw)\b/.test(ln) && /^\S|^\s{0,8}\S/.test(ln))
      syms.push({ k: m[1].includes('::') || /^\s{2,}/.test(ln) ? 'm' : 'f', name: m[1] + '(' + m[2].replace(/\s+/g, ' ').trim().slice(0, 40) + ')', line: i + 1 });
  });
  renderStructure(syms);
}
const SK = { v: ['v', '#c77dbb'], c: ['C', '#e0823f'], s: ['S', '#6aab73'], e: ['E', '#d2a038'], n: ['N', '#7d828c'], u: ['U', '#7d828c'], f: ['f', '#56a8f5'], m: ['m', '#b3ae60'] };
function renderStructure(syms) {
  S.syms = syms; const el = $('#structure'); el.innerHTML = syms.length ? '' : '<div class="muted" style="padding:14px">No symbols in this file.</div>';
  syms.forEach(s => { const d = document.createElement('div'); d.className = 'node'; d.style.paddingLeft = (8 + (s.depth || 0) * 14) + 'px'; d.innerHTML = `<span class="fi" style="background:${SK[s.k][1]}">${SK[s.k][0]}</span><span class="nm">${esc(s.name)}</span><span class="muted" style="margin-left:auto">${s.line}</span>`; d.onclick = () => { S.editor.revealLineInCenter(s.line); S.editor.setPosition({ lineNumber: s.line, column: 1 }); S.editor.focus(); }; el.appendChild(d); });
}

/* ---------- panels ---------- */
function showSide(name) {
  const ws = $('#workspace');
  if (S.side === name && !ws.classList.contains('nosidebar')) { ws.classList.add('nosidebar'); $$('#stripeLeft [data-panel]').forEach(b => b.classList.remove('active')); return; }
  S.side = name; ws.classList.remove('nosidebar');
  $('#tree').hidden = name !== 'project'; $('#structure').hidden = name !== 'structure'; $('#sidebarTitle').textContent = name === 'project' ? 'Project' : 'Structure';
  $$('#stripeLeft [data-panel]').forEach(b => b.classList.toggle('active', b.dataset.panel === name));
  if (name === 'structure') computeStructure();
}
function showBottom(name) {
  const same = S.bottom === name && !$('#bottom').hidden;
  if (same) return hideBottom();
  S.bottom = name; $('#bottom').hidden = false; $('#splitBottom').hidden = false;
  $$('.bpane').forEach(p => p.classList.toggle('active', p.id === 'pane-' + name));
  $$('#btabs button').forEach(b => b.classList.toggle('active', b.dataset.bottom === name));
  $$('#stripeLeft [data-bottom]').forEach(b => b.classList.toggle('active', b.dataset.bottom === name));
  setTimeout(() => { if (name === 'terminal') termFocus(); else ({ run: () => $('#runIn'), find: () => $('#findQ'), debug: () => $('#dbgIn') }[name] || (() => null))()?.focus(); }, 30);
}
function hideBottom() { $('#bottom').hidden = true; $('#splitBottom').hidden = true; $$('#stripeLeft [data-bottom]').forEach(b => b.classList.remove('active')); S.editor && S.editor.focus(); }
function forceBottom(name) { if ($('#bottom').hidden || S.bottom !== name) { const was = S.bottom; $('#bottom').hidden = true; showBottom(name); } }

function splitter(id, axis, apply) {
  const el = $(id);
  el.onmousedown = (e) => {
    e.preventDefault(); el.classList.add('drag');
    const mv = (ev) => apply(ev); const up = () => { el.classList.remove('drag'); removeEventListener('mousemove', mv); removeEventListener('mouseup', up); document.body.style.cursor = ''; };
    document.body.style.cursor = axis === 'x' ? 'col-resize' : 'row-resize'; addEventListener('mousemove', mv); addEventListener('mouseup', up);
  };
}

/* ---------- process execution ---------- */
const sinks = {}, early = {};
nova.onData(m => { const s = sinks[m.id]; if (s) s.data(m); else (early[m.id] = early[m.id] || []).push({ t: 'd', m }); });
nova.onExit(m => { const s = sinks[m.id]; if (s) s.exit(m.code); else (early[m.id] = early[m.id] || []).push({ t: 'x', m }); });
async function spawnProc(cmd, args, cwd, sink, opts) {
  const id = await nova.start(cmd, args, cwd, opts);
  const wrapped = { data: sink.data, exit: (code) => { delete sinks[id]; sink.exit(code); } };
  sinks[id] = wrapped;
  const pend = early[id] || []; delete early[id];
  for (const ev of pend) ev.t === 'd' ? wrapped.data(ev.m) : wrapped.exit(ev.m.code);
  return id;
}
function execCapture(cmd, args, cwd, opts) {
  return new Promise(async (res) => { let out = ''; await spawnProc(cmd, args, cwd, { data: m => out += m.data, exit: code => res({ code, out }) }, opts); });
}

const LINK_RE = /((?:[A-Za-z]:)?[^\s:()"'<>|]+?\.(?:c|cc|cpp|cxx|h|hh|hpp|hxx|txt|cmake)):(\d+)(?::(\d+))?/g;
const DIAG_RE = /^((?:[A-Za-z]:)?[^:\n]+?):(\d+):(?:(\d+):)? (fatal error|error|warning|note): (.*)$/;
function resolveFile(p, cwd) { const abs = isAbs(p) ? p : join(cwd || S.root || '', p); return normPath(abs); }
function out(el, text, cls = '', cwd) {
  const pane = $(el); const stick = pane.scrollTop + pane.clientHeight >= pane.scrollHeight - 30;
  const span = document.createElement('span'); if (cls) span.className = cls;
  text = text.replace(/\x1b\[[0-9;]*[A-Za-z]/g, '');
  let html = '', last = 0;
  for (const m of text.matchAll(LINK_RE)) {
    html += esc(text.slice(last, m.index)) + `<a data-f="${esc(resolveFile(m[1], cwd))}" data-l="${m[2]}" data-c="${m[3] || 1}">${esc(m[0])}</a>`; last = m.index + m[0].length;
  }
  span.innerHTML = html + esc(text.slice(last));
  pane.appendChild(span); if (stick) pane.scrollTop = pane.scrollHeight;
}
function linkClicks(el) { $(el).addEventListener('click', e => { const a = e.target.closest('a[data-f]'); if (a) openFile(a.dataset.f, { line: +a.dataset.l, col: +a.dataset.c }); }); }

function setBusy(on, label) {
  $('#stBuild').classList.toggle('busy', on); $('#stBuild').textContent = label;
  $('#btnStop').disabled = !on; $('#btnRun').classList.toggle('busy', on);
}
function parseDiag(line, cwd) {
  const m = line.match(DIAG_RE); if (!m) return;
  S.problems.push({ file: resolveFile(m[1], cwd), line: +m[2], col: +(m[3] || 1), sev: m[4] === 'note' ? 'note' : m[4] === 'warning' ? 'warning' : 'error', msg: m[5] });
}
/** Run one process, streaming to the Run pane. Resolves with the exit code. */
function execStep(step, { parseProblems = true, interactive = false } = {}) {
  return new Promise(async (res) => {
    out('#runOut', `$ ${[step.cmd, ...step.args].map(a => /\s/.test(a) ? `"${a}"` : a).join(' ')}\n`, 'cmd');
    let buf = '';
    const flush = (all) => { const ls = buf.split(/\r?\n/); buf = all ? '' : ls.pop(); if (all && ls[ls.length - 1] === '') ls.pop(); ls.forEach(l => parseProblems && parseDiag(l, step.cwd)); };
    let exited = false;
    const id = await spawnProc(step.cmd, step.args, step.cwd, {
      data: m => { out('#runOut', m.data, m.stream === 'err' ? 'err' : '', step.cwd); buf += m.data; flush(false); },
      exit: code => { exited = true; flush(true); S.running = null; res(code); }
    }, { showWindow: !!step.run });
    if (!exited) S.running = id;
  });
}
async function runPipeline(steps, label) {
  if (S.running) { await stopRun(); }
  $('#runOut').innerHTML = ''; S.problems = []; forceBottom('run'); setBusy(true, label);
  const t0 = performance.now(); let code = 0;
  for (const st of steps) {
    code = await execStep(st, { parseProblems: !st.run });
    if (code !== 0) break;
  }
  const secs = ((performance.now() - t0) / 1000).toFixed(1);
  const last = steps[steps.length - 1];
  out('#runOut', `\nProcess finished with exit code ${code}  (${secs}s)\n`, code === 0 ? 'ok' : 'err');
  setBusy(false, code === 0 ? 'Ready' : `Failed (exit ${code})`);
  renderProblems(); applyMarkers();
  if (S.problems.some(p => p.sev === 'error') && !last.run) showProblemsIfErrors();
  S.tabs.length && buildTree();
  return code;
}
function showProblemsIfErrors() { if (S.problems.length) toast(`${S.problems.filter(p => p.sev === 'error').length} error(s), ${S.problems.filter(p => p.sev === 'warning').length} warning(s) – see Problems`, 'error'); }
async function stopRun() { if (D.on) await dbgStop(); if (S.running) { await nova.kill(S.running); S.running = null; out('#runOut', '\nProcess terminated by user.\n', 'warn'); setBusy(false, 'Stopped'); } }

function compilerFor(path) {
  const isC = ext(path) === 'c';
  const t = S.tools; const cc = isC ? (t.gcc ? 'gcc' : t.clang ? 'clang' : null) : (t['g++'] ? 'g++' : t['clang++'] ? 'clang++' : null);
  return cc;
}
async function unbufFlag() { if (!S.settings.unbuffered) return []; if (S.unbuf === null || S.unbuf === undefined) S.unbuf = await nova.unbufHeader(); return S.unbuf ? ['-include', S.unbuf] : []; }

async function planFile() {
  const tab = S.active; if (!tab || !/^(c|cc|cpp|cxx)$/.test(ext(tab.path))) { toast('Open a .c / .cpp file to run it.'); return null; }
  const cc = compilerFor(tab.path); if (!cc) { toast('No C/C++ compiler found on PATH. Install MinGW-w64/MSYS2 or LLVM.', 'error'); return null; }
  const outDir = join(S.root || dirname(tab.path), '.nova', 'out');
  try { await nova.create(outDir, true); } catch { }
  const name = base(tab.path).replace(/\.[^.]+$/, ''), exe = join(outDir, name + (IS_WIN ? '.exe' : ''));
  const std = ext(tab.path) === 'c' ? '-std=' + S.settings.cStd : '-std=' + S.settings.cppStd;
  return { exe, build: { cmd: cc, args: ['-g', '-O0', std, '-Wall', '-Wextra', ...(await unbufFlag()), tab.path, '-o', exe], cwd: dirname(tab.path) }, run: { cmd: exe, args: [], cwd: dirname(tab.path), run: true }, tab };
}

function generator() {
  const t = S.tools; if (t.ninja) return 'Ninja'; if (t['mingw32-make'] && IS_WIN) return 'MinGW Makefiles'; if (t.make) return 'Unix Makefiles'; return null;
}
function cmakeSteps(target) {
  const dir = join(S.root, 'cmake-build-debug'), steps = [];
  if (S.cmakeDirty || !S.cmakeCfgOk) {
    const gen = generator(); const cflags = S.unbuffFlags ? S.unbuffFlags.join(' ') : '';
    const a = ['-S', S.root, '-B', dir, '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'];
    if (gen) a.push('-G', gen);
    if (S.tools.gcc) a.push('-DCMAKE_C_COMPILER=gcc'); if (S.tools['g++']) a.push('-DCMAKE_CXX_COMPILER=g++');
    if (cflags) a.push('-DCMAKE_C_FLAGS=' + cflags, '-DCMAKE_CXX_FLAGS=' + cflags);
    steps.push({ cmd: 'cmake', args: a, cwd: S.root, cfg: true });
  }
  const b = ['--build', dir]; if (target) b.push('--target', target); b.push('-j', String(navigator.hardwareConcurrency || 4));
  steps.push({ cmd: 'cmake', args: b, cwd: S.root });
  return { dir, steps };
}
async function scanTargets() {
  S.targets = []; S.isCmake = !!S.root && await nova.exists(join(S.root, 'CMakeLists.txt'));
  if (!S.isCmake) { updateConfigLabel(); return; }
  const files = (await nova.listAll(S.root)).filter(f => base(f) === 'CMakeLists.txt');
  for (const f of files) {
    try {
      const txt = (await nova.read(f)).text || ''; const proj = (txt.match(/project\s*\(\s*([\w\-.]+)/i) || [])[1];
      for (const m of txt.matchAll(/add_executable\s*\(\s*([^\s)]+)/gi)) { const n = m[1].replace('${PROJECT_NAME}', proj || ''); if (n && !n.includes('${') && !S.targets.includes(n)) S.targets.push(n); }
    } catch { }
  }
  if (S.targets.length && S.config.type === 'file' && !S.userPickedFile) S.config = { type: 'cmake', target: S.targets[0] };
  updateConfigLabel();
}
function updateConfigLabel() { $('#configName').textContent = S.config.type === 'cmake' ? S.config.target : S.config.type === 'all' ? 'All targets' : 'Current File'; }
window.addEventListener('focus', () => { if (S.root) { S.cmakeDirty = true; scanTargets(); } });   // pick up CMakeLists.txt edits made outside NOVA
async function configMenu(e) {
  const r = e.currentTarget.getBoundingClientRect();
  if (S.root) await scanTargets();   // always show the current list of targets
  const items = [{ label: 'Current File', run: () => { S.config = { type: 'file' }; S.userPickedFile = true; updateConfigLabel(); } }];
  if (S.targets.length) { items.push('-'); S.targets.forEach(t => items.push({ label: t, run: () => { S.config = { type: 'cmake', target: t }; updateConfigLabel(); } })); items.push({ label: 'Build all targets', run: () => { S.config = { type: 'all' }; updateConfigLabel(); } }); }
  showMenu(r.left, r.bottom + 4, items);
}
async function ensureUnbuf() { S.unbuffFlags = await unbufFlag(); }

async function doBuild() {
  if (S.settings.autosave) await saveAll();
  if (S.config.type === 'file') { const p = await planFile(); if (!p) return false; return (await runPipeline([p.build], 'Building ' + p.tab.name)) === 0; }
  if (!S.tools.cmake) { toast('CMake not found on PATH.', 'error'); return false; }
  await ensureUnbuf(); const { steps } = cmakeSteps(S.config.type === 'cmake' ? S.config.target : null);
  const ok = (await runPipeline(steps, 'Building ' + (S.config.target || 'all'))) === 0;
  if (ok) { S.cmakeDirty = false; S.cmakeCfgOk = true; } return ok;
}
async function doRun() {
  if (S.settings.autosave) await saveAll();
  if (S.config.type === 'file') { const p = await planFile(); if (!p) return; return runPipeline([p.build, p.run], 'Running ' + p.tab.name); }
  if (!S.tools.cmake) return toast('CMake not found on PATH.', 'error');
  if (S.config.type === 'all') return doBuild();
  await ensureUnbuf(); const { dir, steps } = cmakeSteps(S.config.target);
  const first = await runPipeline(steps, 'Building ' + S.config.target); if (first !== 0) return;
  S.cmakeDirty = false; S.cmakeCfgOk = true;
  const nm = S.config.target + (IS_WIN ? '.exe' : ''); const hit = (await nova.listAll(dir)).find(f => base(f).toLowerCase() === nm.toLowerCase() && !/CMakeFiles/.test(f));
  if (!hit) return toast('Built, but could not locate executable ' + esc(nm), 'error');
  setBusy(true, 'Running ' + S.config.target);
  const code = await execStep({ cmd: hit, args: [], cwd: dir, run: true }, { parseProblems: false });
  out('#runOut', `\nProcess finished with exit code ${code}\n`, code === 0 ? 'ok' : 'err'); setBusy(false, 'Ready');
}
const doDebug = () => startDebugSession();

/* ---------- problems / markers ---------- */
function renderProblems() {
  const list = $('#problemList'); list.innerHTML = '';
  const seen = new Set(), all = [];
  for (const p of S.problems.concat(lspProblems())) { const k = key(p.file) + ':' + p.line + ':' + p.msg; if (!seen.has(k)) { seen.add(k); all.push(p); } }
  const rank = (x) => (x.sev === 'error' ? 0 : x.sev === 'warning' ? 1 : 2);
  all.sort((a, b) => rank(a) - rank(b) || a.line - b.line);
  const errs = all.filter(p => p.sev === 'error').length, warns = all.filter(p => p.sev === 'warning').length;
  const badge = $('#problemBadge'); badge.hidden = !(errs + warns); badge.textContent = errs || warns; badge.classList.toggle('w', !errs);
  if (!all.length) { list.innerHTML = '<div class="muted" style="padding:14px">No problems detected.</div>'; return; }
  const by = {}; all.forEach(p => (by[key(p.file)] = by[key(p.file)] || []).push(p));
  for (const ps of Object.values(by)) {
    const f = ps[0].file;
    const h = document.createElement('div'); h.className = 'grp'; h.textContent = `${rel(f)}  (${ps.length})`; list.appendChild(h);
    ps.forEach(p => { const r = document.createElement('div'); r.className = 'row'; r.innerHTML = `<span class="sev ${p.sev}"></span><span class="txt">${esc(p.msg)}</span><span class="loc">${p.line}:${p.col}</span>`; r.onclick = () => openFile(p.file, { line: p.line, col: p.col }); list.appendChild(r); });
  }
}
function applyMarkers() {
  for (const t of S.tabs) {
    const ms = S.problems.filter(p => key(p.file) === key(t.path)).map(p => {
      const w = t.model.getWordAtPosition({ lineNumber: Math.min(p.line, t.model.getLineCount()), column: p.col });
      const ln = Math.min(p.line, t.model.getLineCount());
      return { startLineNumber: ln, startColumn: p.col, endLineNumber: ln, endColumn: w ? w.endColumn : p.col + 1, message: p.msg, severity: p.sev === 'error' ? monaco.MarkerSeverity.Error : p.sev === 'warning' ? monaco.MarkerSeverity.Warning : monaco.MarkerSeverity.Info, source: 'compiler' };
    });
    monaco.editor.setModelMarkers(t.model, 'compiler', ms);
  }
}

/* ---------- find in files ---------- */
async function doFind() {
  const q = $('#findQ').value; if (!q || !S.root) return;
  $('#findCount').textContent = 'Searching…';
  const res = await nova.grep(S.root, q, { case: $('#findCase').checked, word: $('#findWord').checked, regex: $('#findRe').checked });
  const list = $('#findList'); list.innerHTML = '';
  const by = {}; res.forEach(r => (by[r.file] = by[r.file] || []).push(r));
  $('#findCount').textContent = `${res.length} match${res.length === 1 ? '' : 'es'} in ${Object.keys(by).length} file(s)`;
  let re; try { re = new RegExp($('#findRe').checked ? q : q.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), $('#findCase').checked ? 'g' : 'gi'); } catch { }
  for (const [f, rs] of Object.entries(by)) {
    const h = document.createElement('div'); h.className = 'grp'; h.textContent = `${rel(f)}  (${rs.length})`; list.appendChild(h);
    rs.forEach(r => { const d = document.createElement('div'); d.className = 'row'; d.innerHTML = `<span class="txt">${re ? esc(r.text).replace(new RegExp(esc(re.source), re.flags), x => `<mark>${x}</mark>`) : esc(r.text)}</span><span class="loc">${r.line}</span>`; d.onclick = () => openFile(r.file, { line: r.line }); list.appendChild(d); });
  }
}

/* ---------- command palette / Search Everywhere ---------- */
const ACTIONS = () => [
  { name: 'Run', key: 'Shift+F10', run: doRun }, { name: 'Debug', key: 'Shift+F9', run: doDebug }, { name: 'Build', key: 'Ctrl+F9', run: doBuild }, { name: 'Stop', key: 'Ctrl+F2', run: stopRun },
  { name: 'Save All', key: 'Ctrl+Shift+S', run: saveAll }, { name: 'New Project…', run: newProjectDialog }, { name: 'Open Folder…', key: 'Ctrl+Shift+O', run: pickFolder },
  { name: 'Find in Files', key: 'Ctrl+Shift+F', run: () => forceBottom('find') }, { name: 'Toggle Terminal', key: 'Alt+F12', run: () => showBottom('terminal') }, { name: 'Toggle Problems', key: 'Alt+6', run: () => showBottom('problems') },
  { name: 'Toggle Project Panel', key: 'Alt+1', run: () => showSide('project') }, { name: 'Show Structure', key: 'Alt+7', run: () => showSide('structure') },
  { name: 'Reformat Code (clang-format)', key: 'Ctrl+Alt+L', run: reformat }, { name: 'Settings…', key: 'Ctrl+Alt+S', run: openSettings },
  { name: 'Toggle Dark/Light Theme', run: () => { S.settings.theme = S.settings.theme === 'dark' ? 'light' : 'dark'; applyTheme(); saveSettings(); } },
  { name: 'Close Active Tab', key: 'Ctrl+W', run: () => S.active && closeTab(S.active) }, { name: 'Reload CMake Project', run: () => { S.cmakeDirty = true; scanTargets(); toast('CMake project will be reconfigured on next build.'); } },
  { name: 'Restart Terminal', run: () => { showBottom_ensureTerm(); termRestart(); } }, { name: 'Show Debug Panel', key: 'Alt+5', run: () => forceBottom('debug') },
  { name: 'Add Watch…', run: () => dbgAddWatch() }, { name: 'Run to Cursor', key: 'Alt+F9', run: dbgRunToCursor },
  { name: 'Restart Language Server (clangd)', run: () => lspBoot() },
  { name: 'Toggle Developer Tools', key: 'F12', run: () => nova.devtools() }
];
const showBottom_ensureTerm = () => { if ($('#bottom').hidden || S.bottom !== 'terminal') forceBottom('terminal'); };
function fuzzy(q, s) {
  q = q.toLowerCase(); const t = s.toLowerCase(); let i = 0, score = 0, last = -2;
  for (let j = 0; j < t.length && i < q.length; j++) if (t[j] === q[i]) { score += (j === last + 1 ? 5 : 1) + (j === 0 || /[\/\\._\- ]/.test(t[j - 1]) ? 4 : 0); last = j; i++; }
  return i === q.length ? score - t.length * 0.01 : -1;
}
async function openPalette(mode = 'all') {
  if (!S.fileCache && S.root) S.fileCache = await nova.listAll(S.root);
  let sel = 0, items = [];
  const m = modal(`<div class="se-head"><div class="seg" id="seTabs">${['all:All', 'files:Files', 'symbols:Symbols', 'actions:Actions'].map(x => { const [k, l] = x.split(':'); return `<button data-m="${k}">${l}</button>`; }).join('')}</div>
    <input id="seQ" placeholder="Type to search files, symbols and actions…" spellcheck="false" /></div><div class="se-list" id="seList"></div>`, { width: '720px' });
  const q = $('#seQ', m), list = $('#seList', m);
  const compute = () => {
    const s = q.value.trim(); items = [];
    const add = (arr) => items.push(...arr);
    if (mode === 'all' || mode === 'files') add((S.fileCache || []).map(f => ({ t: 'file', label: base(f), sub: rel(dirname(f)), f, score: s ? Math.max(fuzzy(s, base(f)) * 2, fuzzy(s, rel(f))) : (S.tabs.some(t => key(t.path) === key(f)) ? 1 : 0) })));
    if (mode === 'all' || mode === 'symbols') add((S.syms || []).map(y => ({ t: 'sym', label: y.name, sub: ':' + y.line, y, score: s ? fuzzy(s, y.name) : 0.5 })));
    if (mode === 'all' || mode === 'actions') add(ACTIONS().map(a => ({ t: 'act', label: a.name, key: a.key, a, score: s ? fuzzy(s, a.name) : 0.1 })));
    items = items.filter(i => i.score >= (s ? 0 : 0)).filter(i => !s || i.score >= 0).sort((a, b) => b.score - a.score).slice(0, 60);
    if (!s && mode === 'all') items = items.filter(i => i.t !== 'file' || i.score > 0).concat(items.length < 8 ? [] : []);
    sel = 0; draw();
  };
  const draw = () => {
    list.innerHTML = items.length ? '' : '<div class="se-empty">Nothing found</div>';
    items.forEach((it, i) => {
      const d = document.createElement('div'); d.className = 'se-item' + (i === sel ? ' sel' : '');
      const ic = it.t === 'file' ? fileIcon({ path: it.f, name: it.label }) : it.t === 'sym' ? `<span class="fi" style="background:${SK[it.y.k][1]}">${SK[it.y.k][0]}</span>` : `<span class="fi dir">${icon('gear')}</span>`;
      d.innerHTML = `${ic}<span>${esc(it.label)}</span>${it.key ? `<span class="kb">${it.key}</span>` : `<span class="sub">${esc(it.sub || '')}</span>`}`;
      d.onclick = () => go(it); d.onmousemove = () => { if (sel !== i) { sel = i; $$('.se-item', list).forEach((e, j) => e.classList.toggle('sel', j === i)); } };
      list.appendChild(d);
    });
    $$('#seTabs button', m).forEach(b => b.classList.toggle('active', b.dataset.m === mode));
    const s = $('.se-item.sel', list); s && s.scrollIntoView({ block: 'nearest' });
  };
  const go = (it) => { closeModal(); if (it.t === 'file') openFile(it.f); else if (it.t === 'sym') { S.editor.revealLineInCenter(it.y.line); S.editor.setPosition({ lineNumber: it.y.line, column: 1 }); S.editor.focus(); } else setTimeout(() => it.a.run(), 0); };
  $$('#seTabs button', m).forEach(b => b.onclick = () => { mode = b.dataset.m; compute(); q.focus(); });
  q.oninput = compute;
  q.onkeydown = (e) => {
    if (e.key === 'ArrowDown') { sel = Math.min(sel + 1, items.length - 1); draw(); e.preventDefault(); }
    else if (e.key === 'ArrowUp') { sel = Math.max(sel - 1, 0); draw(); e.preventDefault(); }
    else if (e.key === 'Enter' && items[sel]) go(items[sel]);
    else if (e.key === 'Escape') closeModal();
    else if (e.key === 'Tab') { const o = ['all', 'files', 'symbols', 'actions']; mode = o[(o.indexOf(mode) + 1) % 4]; compute(); e.preventDefault(); }
  };
  q.focus(); compute();
}
async function reformat() {
  const t = S.active; if (!t) return;
  if (LSP.on && LSP.versions.has(t.path)) { const a = S.editor.getAction('editor.action.formatDocument'); if (a) return a.run(); }
  if (!S.tools['clang-format']) return toast('<b>clang-format</b> not found. Install LLVM to enable Reformat Code.', 'error');
  await saveTab(t); const r = await execCapture('clang-format', ['-i', '-style={BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 110}', t.path], dirname(t.path));
  if (r.code) return toast('clang-format failed:<br>' + esc(r.out), 'error');
  const txt = (await nova.read(t.path)).text; t.model.pushEditOperations([], [{ range: t.model.getFullModelRange(), text: txt }], () => null); await saveTab(t);
}

/* ---------- wiring ---------- */
function wireUI() {
  linkClicks('#runOut');
  $('#projectChip').onclick = projectMenu; $('#configChip').onclick = configMenu;
  $('#btnRun').onclick = doRun; $('#btnBuild').onclick = doBuild; $('#btnDebug').onclick = doDebug; $('#btnStop').onclick = stopRun;
  $('#btnSearch').onclick = () => openPalette('all'); $('#btnSettings').onclick = openSettings;
  $('#wNew').onclick = newProjectDialog; $('#wOpen').onclick = pickFolder;
  $('#btnNewFile').onclick = () => S.root && newEntry(S.root, false);
  $('#btnCollapse').onclick = () => { S.expanded = new Set([S.root]); buildTree(); };
  $('#btnRefresh').onclick = () => { S.fileCache = null; buildTree(); };
  $$('#stripeLeft [data-panel]').forEach(b => b.onclick = () => showSide(b.dataset.panel));
  $$('[data-bottom]').forEach(b => b.onclick = () => showBottom(b.dataset.bottom));
  $('#btnHide').onclick = hideBottom;
  $('#btnClear').onclick = () => { if (S.bottom === 'terminal') { T.term && T.term.clear(); return; } const p = { run: '#runOut', debug: '#dbgOut', find: '#findList', problems: '#problemList' }[S.bottom]; if (p) $(p).innerHTML = ''; if (S.bottom === 'problems') { S.problems = []; renderProblems(); applyMarkers(); } };
  $('#btnRerun').onclick = doRun;
  $('#runIn').onkeydown = (e) => { if (e.key === 'Enter' && S.running) { const v = e.target.value; nova.stdin(S.running, v + '\n'); out('#runOut', v + '\n', 'cmd'); e.target.value = ''; } };
  $('#findGo').onclick = doFind; $('#findQ').onkeydown = (e) => { if (e.key === 'Enter') doFind(); };
  splitter('#splitSide', 'x', (e) => { document.documentElement.style.setProperty('--side-w', Math.max(160, Math.min(600, e.clientX - 40)) + 'px'); });
  splitter('#splitBottom', 'y', (e) => { document.documentElement.style.setProperty('--bottom-h', Math.max(90, Math.min(innerHeight - 200, innerHeight - e.clientY - 24)) + 'px'); });
  showSide('project'); renderProblems();

  // Shortcuts
  let lastShift = 0, shiftClean = false;
  addEventListener('keydown', (e) => { if (e.key === 'Shift') { if (!e.repeat) shiftClean = true; } else shiftClean = false; }, true);
  addEventListener('keyup', (e) => { if (e.key === 'Shift' && shiftClean) { const n = Date.now(); if (n - lastShift < 380) { lastShift = 0; openPalette('all'); } else lastShift = n; } }, true);
  addEventListener('keydown', (e) => {
    const c = e.ctrlKey || e.metaKey, k = e.key.toLowerCase();
    const hit = (f) => { e.preventDefault(); e.stopPropagation(); f(); };
    if (e.key === 'Escape' && !$('#overlay').hidden) return;
    if (c && !e.shiftKey && !e.altKey && k === 's') hit(() => S.active && saveTab(S.active));
    else if (c && e.shiftKey && k === 's') hit(saveAll);
    else if (c && !e.shiftKey && k === 'w') hit(() => S.active && closeTab(S.active));
    else if (D.on && !e.ctrlKey && !e.altKey && e.key === 'F8') hit(e.shiftKey ? dbgOut : dbgOver);
    else if (D.on && !c && !e.altKey && !e.shiftKey && e.key === 'F7') hit(dbgInto);
    else if (D.on && !c && !e.altKey && !e.shiftKey && e.key === 'F9') hit(dbgResume);
    else if (e.altKey && e.key === 'F9') hit(dbgRunToCursor);
    else if (e.altKey && e.key === '5') hit(() => showBottom('debug'));
    else if (e.shiftKey && e.key === 'F10') hit(doRun);
    else if (e.shiftKey && e.key === 'F9') hit(doDebug);
    else if (c && e.key === 'F9') hit(doBuild);
    else if (c && e.key === 'F2') hit(stopRun);
    else if (c && e.shiftKey && k === 'f') hit(() => { forceBottom('find'); const s = S.editor.getModel() && S.editor.getModel().getValueInRange(S.editor.getSelection()); if (s && !s.includes('\n')) $('#findQ').value = s; $('#findQ').focus(); });
    else if (c && e.shiftKey && k === 'a') hit(() => openPalette('actions'));
    else if (c && ((e.shiftKey && k === 'n') || (!e.shiftKey && k === 'p'))) hit(() => openPalette('files'));
    else if (c && e.key === 'F12') hit(() => { if (!S.syms) computeStructure(); openPalette('symbols'); });
    else if (c && e.shiftKey && k === 'o') hit(pickFolder);
    else if (c && e.altKey && k === 's') hit(openSettings);
    else if (c && e.altKey && k === 'l') hit(reformat);
    else if (e.altKey && e.key === '1') hit(() => showSide('project'));
    else if (e.altKey && e.key === '7') hit(() => showSide('structure'));
    else if (e.altKey && e.key === '4') hit(() => showBottom('run'));
    else if (e.altKey && e.key === '6') hit(() => showBottom('problems'));
    else if (e.altKey && e.key === 'F12') hit(() => showBottom('terminal'));
    else if (e.key === 'F12') hit(() => nova.devtools());
    else if (c && e.key === 'Tab' && S.tabs.length > 1) hit(() => activate(S.tabs[(S.tabs.indexOf(S.active) + (e.shiftKey ? -1 : 1) + S.tabs.length) % S.tabs.length]));
    else if (e.key === 'F2' && S.selected && document.activeElement.tagName !== 'INPUT' && !S.editor.hasTextFocus()) hit(() => renameEntry({ path: S.selected }));
  }, true);
  addEventListener('blur', () => { if (S.settings.autosave) saveAll(); });
  addEventListener('keydown', (e) => { if (e.key === 'Escape' && !$('#overlay').hidden) closeModal(); });
}
boot();

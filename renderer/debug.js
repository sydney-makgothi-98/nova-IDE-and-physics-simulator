'use strict';
/* Interactive debugger on top of gdb's GDB/MI (mi3). Globals (S, key, base, ...) come from app.js. */
const D = { on: false, running: false, tok: 0, cbs: new Map(), frames: [], sel: 0, vars: [], prevVars: new Map(), bpNum: new Map(), exe: null, deco: null, watches: [], last: null };

/* ---------- MI parser ---------- */
function miParseLine(line) {
  const m = /^(\d*)([\^*=~@&])(.*)$/.exec(line);
  if (!m) return { t: 'raw', text: line };
  const tok = m[1] ? +m[1] : null, ch = m[2], rest = m[3];
  if (ch === '~' || ch === '@' || ch === '&') { const p = { s: rest, i: 0 }; return { t: ch, text: miCString(p) }; }
  const ci = rest.indexOf(','); const cls = ci < 0 ? rest : rest.slice(0, ci);
  const data = ci < 0 ? {} : miResults({ s: rest.slice(ci + 1), i: 0 });
  return { t: ch, tok, cls, data };
}
function miCString(p) {
  if (p.s[p.i] !== '"') return ''; p.i++; let out = '';
  while (p.i < p.s.length) {
    const c = p.s[p.i++];
    if (c === '"') break;
    if (c === '\\') {
      const n = p.s[p.i++];
      if (n === 'n') out += '\n'; else if (n === 't') out += '\t'; else if (n === 'r') out += '\r'; else if (n === 'e') out += '\x1b';
      else if (/[0-7]/.test(n)) { let o = n; while (o.length < 3 && /[0-7]/.test(p.s[p.i])) o += p.s[p.i++]; out += String.fromCharCode(parseInt(o, 8)); }
      else out += n;
    } else out += c;
  }
  return out;
}
function miValue(p) {
  const c = p.s[p.i];
  if (c === '"') return miCString(p);
  if (c === '{') { p.i++; const o = miResults(p, '}'); return o; }
  if (c === '[') {
    p.i++; const arr = [];
    while (p.i < p.s.length && p.s[p.i] !== ']') {
      if (/[A-Za-z_]/.test(p.s[p.i]) && /^[\w-]+=/.test(p.s.slice(p.i))) { const o = {}; const k = p.s.slice(p.i, p.s.indexOf('=', p.i)); p.i += k.length + 1; o[k] = miValue(p); arr.push(o); } else arr.push(miValue(p));
      if (p.s[p.i] === ',') p.i++;
    }
    p.i++; return arr;
  }
  return '';
}
function miResults(p, close) {
  const o = {};
  while (p.i < p.s.length && p.s[p.i] !== close) {
    const eq = p.s.indexOf('=', p.i); if (eq < 0) break;
    const k = p.s.slice(p.i, eq); p.i = eq + 1; const v = miValue(p);
    if (k in o) { o[k] = Array.isArray(o[k]) && o[k].__multi ? o[k].concat([v]) : Object.assign([o[k], v], { __multi: true }); } else o[k] = v;
    if (p.s[p.i] === ',') p.i++;
  }
  if (close && p.s[p.i] === close) p.i++;
  return o;
}
const miq = (s) => '"' + String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"';
const fwd = (p) => p.split('\\').join('/');

/* ---------- transport ---------- */
function dbgCmd(cmd) {
  return new Promise((res) => {
    if (!D.on) return res({ cls: 'error', data: { msg: 'not debugging' } });
    const tok = ++D.tok; D.cbs.set(tok, res); nova.gdbSend(tok + cmd);
  });
}
function dcon(text, cls = '') { out('#dbgOut', text, cls); }

nova.onGdbLines((lines) => { for (const l of lines) dbgHandle(miParseLine(l)); });
nova.onGdbExit(() => dbgCleanup());

function dbgHandle(r) {
  if (r.t === 'raw') { if (r.text && r.text !== '(gdb) ') dcon(r.text + '\n'); return; }
  if (r.t === '~') return dcon(r.text);
  if (r.t === '@') return dcon(r.text);
  if (r.t === '&') return; // gdb log stream (echoes commands)
  if (r.t === '^') { const cb = D.cbs.get(r.tok); if (cb) { D.cbs.delete(r.tok); cb(r); } else if (r.cls === 'error') dcon((r.data.msg || 'error') + '\n', 'err'); return; }
  if (r.t === '=') {
    if (r.cls === 'thread-group-exited' || r.cls === 'library-loaded') return;
    return;
  }
  if (r.t === '*') {
    if (r.cls === 'running') { D.running = true; dbgClearLine(); dbgUi(); }
    else if (r.cls === 'stopped') dbgStopped(r.data);
  }
}

/* ---------- session lifecycle ---------- */
async function dbgStart(exe, cwd) {
  if (D.on) await dbgStop();
  const r = await nova.gdbStart(exe, cwd, []); if (r.error) return toast('Could not start gdb: ' + esc(r.error), 'error');
  Object.assign(D, { on: true, running: false, frames: [], vars: [], sel: 0, bpNum: new Map(), exe, prevVars: new Map() });
  $('#dbgOut').innerHTML = ''; $('#dbgFrames').innerHTML = ''; $('#dbgVars').innerHTML = '';
  forceBottom('debug'); dbgUi(); setBusy(true, 'Debugging ' + base(exe));
  await dbgCmd('-gdb-set breakpoint pending on'); await dbgCmd('-gdb-set confirm off'); await dbgCmd('-gdb-set pagination off');
  await dbgCmd('-gdb-set print pretty off'); await dbgCmd('-gdb-set disassembly-flavor intel');
  await dbgCmd('-gdb-set new-console ' + (S.settings.debugExtConsole === false ? 'off' : 'on'));
  await dbgCmd('-environment-cd ' + miq(fwd(cwd)));
  for (const [k, set] of Object.entries(S.breaks)) {
    const tab = S.tabs.find(t => key(t.path) === k); const path = tab ? tab.path : k;
    for (const line of set) await dbgInsert(path, line);
  }
  dcon(`Debugging ${base(exe)}  —  ${D.bpNum.size} breakpoint(s)\n`, 'cmd');
  dbgCmd('-exec-run');
}
async function dbgInsert(path, line) {
  const r = await dbgCmd(`-break-insert -f --source ${miq(fwd(path))} --line ${line}`);
  if (r.cls === 'done' && r.data.bkpt) D.bpNum.set(key(path) + ':' + line, r.data.bkpt.number);
}
function dbgSyncBreak(path, line, added) {
  if (!D.on) return; const k = key(path) + ':' + line;
  if (added) dbgInsert(path, line); else if (D.bpNum.has(k)) { dbgCmd('-break-delete ' + D.bpNum.get(k)); D.bpNum.delete(k); }
}
async function dbgStop() { if (!D.on) return; await nova.gdbStop(); dbgCleanup(); }
function dbgCleanup() {
  if (!D.on && !D.running) { dbgClearLine(); return; }
  D.on = false; D.running = false; D.cbs.clear(); dbgClearLine(); dbgUi(); setBusy(false, 'Ready');
  dcon('\nDebug session ended.\n', 'cmd');
}

async function dbgStopped(d) {
  D.running = false;
  const reason = d.reason || '';
  if (reason.startsWith('exited')) {
    const code = d['exit-code'] !== undefined ? parseInt(d['exit-code'], 8) : 0;
    dcon(`\nProcess exited with code ${code}\n`, code ? 'err' : 'ok'); await nova.gdbSend('-gdb-exit'); setTimeout(() => D.on && nova.gdbStop(), 300); return;
  }
  if (reason === 'signal-received' || reason === 'exited-signalled') {
    const msg = `Program received signal ${d['signal-name'] || ''} (${d['signal-meaning'] || ''})`;
    dcon('\n' + msg + '\n', 'err'); toast(esc(msg), 'error', 6000);
    if (reason === 'exited-signalled') { setTimeout(() => D.on && nova.gdbStop(), 200); return; }
  }
  D.sel = 0; await dbgRefresh(true);
}
async function dbgRefresh(jump) {
  dbgUi();
  const st = await dbgCmd('-stack-list-frames'); const list = st.data.stack ? [].concat(st.data.stack) : [];
  D.frames = list.map(x => x.frame || x).map(f => ({ level: +f.level, func: f.func, file: f.fullname || f.file, line: +f.line, addr: f.addr }));
  renderFrames(); await dbgVars(); await dbgWatches();
  const f = D.frames[D.sel]; if (jump && f && f.file) await dbgShow(f); dbgUi();
}
async function dbgVars() {
  const r = await dbgCmd('-stack-list-variables --all-values'); const vs = r.data.variables ? [].concat(r.data.variables) : [];
  const prev = D.prevVars; D.prevVars = new Map(vs.map(v => [v.name, v.value]));
  D.vars = vs.map(v => ({ name: v.name, value: v.value, arg: v.arg === '1', changed: prev.has(v.name) && prev.get(v.name) !== v.value }));
  const el = $('#dbgVars'); el.innerHTML = D.vars.length ? '' : '<div class="muted dpad">No variables in this frame.</div>';
  D.vars.forEach(v => { const d = document.createElement('div'); d.className = 'row dv' + (v.changed ? ' chg' : ''); d.innerHTML = `<span class="vn">${esc(v.name)}</span><span class="eq">=</span><span class="vv">${esc(v.value ?? '{…}')}</span>`; el.appendChild(d); });
}
async function dbgEval(expr) { const r = await dbgCmd('-data-evaluate-expression ' + miq(expr)); return r.cls === 'done' ? { ok: true, v: r.data.value } : { ok: false, v: r.data.msg || 'error' }; }
async function dbgWatches() {
  const el = $('#dbgWatch'); el.innerHTML = D.watches.length ? '' : '<div class="muted dpad">No watches. Click + to add an expression.</div>';
  for (const [i, w] of D.watches.entries()) {
    const r = D.on && !D.running ? await dbgEval(w) : { ok: false, v: 'n/a' };
    const d = document.createElement('div'); d.className = 'row dv';
    d.innerHTML = `<span class="vn">${esc(w)}</span><span class="eq">=</span><span class="vv ${r.ok ? '' : 'err'}">${esc(r.v)}</span><span class="loc"><button class="ib sm" title="Remove">${icon('x')}</button></span>`;
    $('button', d).onclick = () => { D.watches.splice(i, 1); S.settings.watches = D.watches; saveSettings(); dbgWatches(); }; el.appendChild(d);
  }
}
function renderFrames() {
  const el = $('#dbgFrames'); el.innerHTML = '';
  D.frames.forEach((f, i) => {
    const d = document.createElement('div'); d.className = 'row' + (i === D.sel ? ' dsel' : '');
    d.innerHTML = `<span class="txt">${esc(f.func || '??')}</span><span class="loc">${f.file ? esc(base(f.file)) + ':' + f.line : '—'}</span>`;
    d.onclick = async () => { D.sel = i; await dbgCmd('-stack-select-frame ' + i); renderFrames(); await dbgVars(); await dbgWatches(); if (f.file) dbgShow(f); };
    el.appendChild(d);
  });
}
async function dbgShow(f) {
  const path = f.file.split('/').join(SEP); await openFile(path, { line: f.line });
  D.last = { path, line: f.line }; dbgDecorate();
}
function dbgDecorate() {
  if (!D.deco) D.deco = S.editor.createDecorationsCollection();
  if (D.on && !D.running && D.last && S.active && key(S.active.path) === key(D.last.path))
    D.deco.set([{ range: new monaco.Range(D.last.line, 1, D.last.line, 1), options: { isWholeLine: true, className: 'dbg-line', glyphMarginClassName: 'dbg-arrow' } }]);
  else D.deco.set([]);
}
function dbgClearLine() { D.last = null; if (D.deco) D.deco.set([]); }

/* ---------- controls ---------- */
const dbgReady = () => D.on && !D.running;
const dbgResume = () => dbgReady() && dbgCmd('-exec-continue');
const dbgOver = () => dbgReady() && dbgCmd('-exec-next');
const dbgInto = () => dbgReady() && dbgCmd('-exec-step');
const dbgOut = () => dbgReady() && dbgCmd('-exec-finish');
async function dbgRunToCursor() {
  if (!dbgReady() || !S.active) return; const line = S.editor.getPosition().lineNumber;
  await dbgCmd(`-break-insert -t -f --source ${miq(fwd(S.active.path))} --line ${line}`); dbgCmd('-exec-continue');
}
async function dbgAddWatch(expr) {
  if (!expr) expr = await promptModal('Add Watch', '', 'C/C++ expression, e.g. arr[i] or *ptr'); if (!expr) return;
  D.watches.push(expr); S.settings.watches = D.watches; saveSettings(); dbgWatches(); showBottom_ensure();
}
const showBottom_ensure = () => { if ($('#bottom').hidden || S.bottom !== 'debug') forceBottom('debug'); };
function dbgUi() {
  const on = D.on, stopped = on && !D.running;
  $$('#dbgBar [data-d]').forEach(b => { const k = b.dataset.d; b.disabled = k === 'stop' ? !on : !stopped; });
  $('#dbgState').textContent = !on ? 'Not debugging' : D.running ? 'Running…' : 'Paused';
  $('#dbgState').className = 'dstate ' + (!on ? '' : D.running ? 'run' : 'pause');
  $('#btnDebug').classList.toggle('on', on);
  $('#btnStop').disabled = !(on || S.running);
}

/* ---------- starting a session from the toolbar ---------- */
async function startDebugSession() {
  if (!S.tools.gdb) return toast('<b>gdb not found.</b> Install: <code>pacman -S mingw-w64-x86_64-gdb</code>', 'error', 8000);
  if (D.on) { if (!D.running) return dbgResume(); return; }
  if (S.settings.autosave) await saveAll();
  let exe, cwd;
  if (S.config.type === 'file') {
    const p = await planFile(); if (!p) return;
    if (await runPipeline([p.build], 'Building ' + p.tab.name) !== 0) return; exe = p.exe; cwd = dirname(p.tab.path);
  } else {
    if (!S.tools.cmake) return toast('CMake not found on PATH.', 'error');
    const tgt = S.config.type === 'cmake' ? S.config.target : S.targets[0]; if (!tgt) return toast('No executable target found.');
    await ensureUnbuf(); const { dir, steps } = cmakeSteps(tgt);
    if (await runPipeline(steps, 'Building ' + tgt) !== 0) return; S.cmakeDirty = false; S.cmakeCfgOk = true;
    const nm = tgt + (IS_WIN ? '.exe' : ''); exe = (await nova.listAll(dir)).find(f => base(f).toLowerCase() === nm.toLowerCase() && !/CMakeFiles/.test(f)); cwd = dir;
    if (!exe) return toast('Built, but could not locate ' + esc(nm), 'error');
  }
  dbgStart(exe, cwd);
}

/* ---------- editor hooks ---------- */
function dbgInit() {
  D.watches = S.settings.watches || [];
  for (const lang of ['c', 'cpp']) monaco.languages.registerHoverProvider(lang, {
    async provideHover(model, pos) {
      if (!dbgReady()) return null;
      const sel = S.editor.getSelection(); const w = model.getWordAtPosition(pos); if (!w) return null;
      const expr = !sel.isEmpty() && sel.containsPosition(pos) ? model.getValueInRange(sel) : w.word;
      const r = await dbgEval(expr); if (!r.ok) return null;
      return { range: new monaco.Range(pos.lineNumber, w.startColumn, pos.lineNumber, w.endColumn), contents: [{ value: '**' + expr + '** = `' + r.v.replace(/`/g, "'") + '`' }] };
    }
  });
  S.editor.addAction({ id: 'nova.runToCursor', label: 'Run to Cursor', keybindings: [monaco.KeyMod.Alt | monaco.KeyCode.F9], contextMenuGroupId: 'navigation', contextMenuOrder: 9, run: dbgRunToCursor });
  S.editor.addAction({ id: 'nova.addWatch', label: 'Add to Watches', contextMenuGroupId: 'navigation', contextMenuOrder: 9.1, run: (ed) => { const s = ed.getModel().getValueInRange(ed.getSelection()); dbgAddWatch(s || undefined); } });
  S.editor.addAction({ id: 'nova.toggleBreakpoint', label: 'Toggle Breakpoint', keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.F8], contextMenuGroupId: 'navigation', contextMenuOrder: 9.2, run: (ed) => S.active && toggleBreak(S.active.path, ed.getPosition().lineNumber) });
  $$('#dbgBar [data-d]').forEach(b => b.onclick = () => ({ resume: dbgResume, over: dbgOver, into: dbgInto, out: dbgOut, cursor: dbgRunToCursor, stop: dbgStop })[b.dataset.d]());
  $('#dbgAddWatch').onclick = () => dbgAddWatch();
  $('#dbgIn').onkeydown = (e) => {
    if (e.key !== 'Enter') return; const v = e.target.value.trim(); e.target.value = ''; if (!v || !D.on) return;
    dcon('(gdb) ' + v + '\n', 'cmd'); dbgCmd('-interpreter-exec console ' + miq(v)).then(r => { if (r.cls === 'error') dcon((r.data.msg || '') + '\n', 'err'); else dbgRefresh(false); });
  };
  dbgUi(); dbgWatches();
}

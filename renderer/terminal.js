'use strict';
/* Integrated terminal: xterm.js front-end over a node-pty (ConPTY) shell. */
const T = { term: null, fit: null, id: null, ro: null, starting: false };

function termTheme() {
  const dark = S.settings.theme === 'dark';
  return dark
    ? { background: '#1e1f22', foreground: '#dfe1e5', cursor: '#ced0d6', selectionBackground: '#214283', black: '#1e1f22', red: '#f75464', green: '#5fb865', yellow: '#f2c55c', blue: '#548af7', magenta: '#c77dbb', cyan: '#42c3d4', white: '#dfe1e5', brightBlack: '#6f737a', brightRed: '#ff7a86', brightGreen: '#7fd28a', brightYellow: '#ffd97a', brightBlue: '#7aa7ff', brightMagenta: '#e0a0d4', brightCyan: '#6fdcea', brightWhite: '#ffffff' }
    : { background: '#ffffff', foreground: '#1e1f22', cursor: '#1e1f22', selectionBackground: '#a6d2ff', black: '#000000', red: '#c42b3b', green: '#208a3c', yellow: '#9a7400', blue: '#2352c8', magenta: '#a02da0', cyan: '#007a8a', white: '#bfbfbf' };
}

async function termEnsure() {
  if (T.term || T.starting) { termFit(); return; }
  T.starting = true;
  const host = $('#xterm');
  T.term = new Terminal({ fontFamily: S.settings.fontFamily, fontSize: Math.max(11, S.settings.fontSize - 1), cursorBlink: true, theme: termTheme(), allowProposedApi: true, scrollback: 5000, convertEol: false });
  T.fit = new FitAddon.FitAddon(); T.term.loadAddon(T.fit); T.term.open(host); T.fit.fit();
  T.term.onData(d => T.id && nova.ptyWrite(T.id, d));
  T.term.onResize(({ cols, rows }) => T.id && nova.ptyResize(T.id, cols, rows));
  T.term.attachCustomKeyEventHandler((e) => {
    if (e.type === 'keydown' && e.ctrlKey && e.shiftKey && e.code === 'KeyC') { navigator.clipboard.writeText(T.term.getSelection()); return false; }
    if (e.type === 'keydown' && e.ctrlKey && e.shiftKey && e.code === 'KeyV') { navigator.clipboard.readText().then(t => T.id && nova.ptyWrite(T.id, t)); return false; }
    if (e.type === 'keydown' && e.ctrlKey && e.code === 'KeyC' && T.term.hasSelection()) { navigator.clipboard.writeText(T.term.getSelection()); T.term.clearSelection(); return false; }
    return true;
  });
  T.ro = new ResizeObserver(() => termFit()); T.ro.observe(host);
  await termSpawn(); T.starting = false;
}
async function termSpawn() {
  const r = await nova.ptySpawn(T.term.cols, T.term.rows, S.root || undefined);
  if (r.error) { T.term.write('\x1b[31mTerminal unavailable: ' + r.error + '\x1b[0m\r\n'); return; }
  T.id = r.id;
}
function termFit() { try { if (T.fit && $('#pane-terminal').classList.contains('active')) T.fit.fit(); } catch { } }
function termFocus() { termEnsure().then(() => { termFit(); T.term && T.term.focus(); }); }
function termRestart() { if (T.id) nova.ptyKill(T.id); T.id = null; if (T.term) { T.term.reset(); termSpawn(); } }
/** Type a command into the shell (used by "open terminal here" style actions). */
function termSend(cmd) { termEnsure().then(() => T.id && nova.ptyWrite(T.id, cmd + '\r')); }

nova.onPtyData(({ id, data }) => { if (id === T.id && T.term) T.term.write(data); });
nova.onPtyExit(({ id }) => { if (id === T.id) { T.id = null; if (T.term) T.term.write('\r\n\x1b[90m[process exited — press any key to restart]\x1b[0m'); const d = T.term.onKey(() => { d.dispose(); termRestart(); }); } });
function termApplyTheme() { if (T.term) { T.term.options.theme = termTheme(); T.term.options.fontFamily = S.settings.fontFamily; T.term.options.fontSize = Math.max(11, S.settings.fontSize - 1); termFit(); } }

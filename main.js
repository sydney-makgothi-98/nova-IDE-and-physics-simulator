const { app, BrowserWindow, ipcMain, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawn, spawnSync } = require('child_process');

let win;
const procs = new Map(); // id -> child process
let procSeq = 0;

function createWindow() {
  win = new BrowserWindow({
    width: 1500, height: 920, minWidth: 900, minHeight: 600,
    backgroundColor: '#1e1f22',
    frame: false,
    titleBarStyle: 'hidden',
    titleBarOverlay: { color: '#2b2d30', symbolColor: '#dfe1e5', height: 40 },
    title: 'NOVA IDE',
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false }
  });
  win.setMenuBarVisibility(false);
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'));
}

app.whenReady().then(() => {
  createWindow();
  if (process.env.NOVA_SHOTDIR) win.setSize(1000, 680);
  if (process.env.NOVA_SELFTEST) {
    // Dev aid: run a script inside the renderer, dump console + screenshot, then quit.
    const log = [];
    win.webContents.on('console-message', (_e, level, msg, line) => log.push(`[${level}] ${msg} (${line})`));
    win.webContents.on('did-finish-load', async () => {
      await new Promise(r => setTimeout(r, 2500));
      try {
        const script = fs.readFileSync(process.env.NOVA_SELFTEST, 'utf8');
        const r = await win.webContents.executeJavaScript(script);
        log.push('RESULT: ' + JSON.stringify(r));
      } catch (e) { log.push('SCRIPT ERROR: ' + e.message); }
      const img = await win.webContents.capturePage();
      fs.writeFileSync(process.env.NOVA_SHOT, img.toPNG());
      fs.writeFileSync(process.env.NOVA_SHOT + '.log', log.join('\n'));
      app.quit();
    });
  }
});
app.on('window-all-closed', () => { for (const p of procs.values()) { try { p.kill(); } catch {} } for (const t of ptys.values()) { try { t.kill(); } catch {} } try { lsp && lsp.kill(); gdb && gdb.kill(); } catch {} app.quit(); });

// ---------- file system ----------
const IGNORE = new Set(['.git', 'node_modules', '.idea', '.vs', '.nova']);
ipcMain.handle('fs:readDir', async (_e, dir) => {
  const ents = await fs.promises.readdir(dir, { withFileTypes: true });
  return ents
    .filter(d => !IGNORE.has(d.name))
    .map(d => ({ name: d.name, path: path.join(dir, d.name), dir: d.isDirectory() }))
    .sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name, undefined, { sensitivity: 'base' }));
});
ipcMain.handle('fs:read', async (_e, f) => {
  const buf = await fs.promises.readFile(f);
  if (buf.includes(0)) return { binary: true };
  return { text: buf.toString('utf8') };
});
ipcMain.handle('fs:write', async (_e, f, text) => { await fs.promises.writeFile(f, text, 'utf8'); return true; });
ipcMain.handle('fs:create', async (_e, p, isDir) => {
  if (fs.existsSync(p)) throw new Error('Already exists');
  if (isDir) await fs.promises.mkdir(p, { recursive: true });
  else { await fs.promises.mkdir(path.dirname(p), { recursive: true }); await fs.promises.writeFile(p, ''); }
  return true;
});
ipcMain.handle('fs:rename', async (_e, a, b) => { await fs.promises.rename(a, b); return true; });
ipcMain.handle('fs:delete', async (_e, p) => { await fs.promises.rm(p, { recursive: true, force: true }); return true; });
ipcMain.handle('fs:reveal', (_e, p) => { shell.showItemInFolder(p); });

ipcMain.handle('dialog:openFolder', async () => {
  const r = await dialog.showOpenDialog(win, { properties: ['openDirectory', 'createDirectory'] });
  return r.canceled ? null : r.filePaths[0];
});
ipcMain.handle('dialog:openFile', async () => {
  const r = await dialog.showOpenDialog(win, { properties: ['openFile'] });
  return r.canceled ? null : r.filePaths[0];
});

// recursive file list (for Search Everywhere)
function walk(dir, out, limit = 20000) {
  let ents; try { ents = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
  for (const d of ents) {
    if (out.length >= limit) return;
    if (IGNORE.has(d.name) || d.name === 'build' || d.name === 'cmake-build-debug') continue;
    const p = path.join(dir, d.name);
    if (d.isDirectory()) walk(p, out, limit); else out.push(p);
  }
}
ipcMain.handle('fs:listAll', (_e, root) => { const out = []; walk(root, out); return out; });

// find in files
ipcMain.handle('fs:grep', (_e, root, query, opts = {}) => {
  const files = []; walk(root, files);
  const results = [];
  let re;
  try {
    const src = opts.regex ? query : query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    re = new RegExp(opts.word ? `\\b${src}\\b` : src, opts.case ? '' : 'i');
  } catch { return []; }
  for (const f of files) {
    try {
      const st = fs.statSync(f); if (st.size > 2e6) continue;
      const buf = fs.readFileSync(f); if (buf.includes(0)) continue;
      const lines = buf.toString('utf8').split(/\r?\n/);
      lines.forEach((ln, i) => { if (re.test(ln) && results.length < 2000) results.push({ file: f, line: i + 1, text: ln.trim().slice(0, 300) }); });
    } catch {}
  }
  return results;
});

// ---------- toolchain detection ----------
function which(cmd) {
  const r = spawnSync(process.platform === 'win32' ? 'where' : 'which', [cmd], { encoding: 'utf8' });
  return r.status === 0 ? r.stdout.split(/\r?\n/)[0].trim() : null;
}
function ver(cmd, arg = '--version') {
  try { const r = spawnSync(cmd, [arg], { encoding: 'utf8' }); return (r.stdout || r.stderr || '').split(/\r?\n/)[0]; } catch { return ''; }
}
ipcMain.handle('tools:detect', () => {
  const tools = {};
  for (const t of ['g++', 'gcc', 'clang++', 'clang', 'cmake', 'ninja', 'make', 'mingw32-make', 'gdb', 'lldb', 'clang-format', 'clangd', 'git']) {
    const p = which(t); if (p) tools[t] = { path: p, version: ver(t) };
  }
  return tools;
});

// ---------- process runner (build / run / terminal) ----------
// opts.showWindow: programs you Run may open their own window (GUI apps); builds stay hidden
function startProc(cmd, args, cwd, opts = {}) {
  const id = ++procSeq;
  const child = spawn(cmd, args, { cwd, shell: !!opts.shell, windowsHide: !opts.showWindow, env: { ...process.env, ...(opts.env || {}) } });
  procs.set(id, child);
  const send = (stream, data) => win && !win.isDestroyed() && win.webContents.send('proc:data', { id, stream, data: data.toString() });
  child.stdout.on('data', d => send('out', d));
  child.stderr.on('data', d => send('err', d));
  child.on('error', e => { send('err', `Failed to start: ${e.message}\n`); win.webContents.send('proc:exit', { id, code: -1 }); procs.delete(id); });
  child.on('close', code => { win && !win.isDestroyed() && win.webContents.send('proc:exit', { id, code }); procs.delete(id); });
  return { id, child };
}
ipcMain.handle('proc:start', (_e, cmd, args, cwd, opts) => startProc(cmd, args, cwd, opts).id);
ipcMain.handle('proc:stdin', (_e, id, text) => { const p = procs.get(id); if (p && p.stdin.writable) p.stdin.write(text); });
ipcMain.handle('proc:kill', (_e, id) => {
  const p = procs.get(id); if (!p) return;
  if (process.platform === 'win32') spawnSync('taskkill', ['/pid', String(p.pid), '/T', '/F']); else p.kill('SIGKILL');
});

ipcMain.handle('app:unbuf', () => {
  // Header force-included into user code so printf prompts appear immediately when stdout is a pipe.
  const f = path.join(app.getPath('userData'), 'nova_unbuf.h');
  if (/\s/.test(f)) return null;
  fs.writeFileSync(f, [
    '#ifndef NOVA_UNBUF_H', '#define NOVA_UNBUF_H', '#include <stdio.h>',
    '__attribute__((constructor)) static void nova_unbuf_init(void){ setvbuf(stdout,NULL,_IONBF,0); setvbuf(stderr,NULL,_IONBF,0); }',
    '#endif', ''
  ].join('\n'));
  return f.replace(/\\/g, '/');
});

// ---------- PTY terminal (node-pty / ConPTY) ----------
let nodePty = null; try { nodePty = require('node-pty'); } catch (e) { console.error('node-pty unavailable:', e.message); }
const ptys = new Map(); let ptySeq = 0;
ipcMain.handle('pty:spawn', (_e, cols, rows, cwd) => {
  if (!nodePty) return { error: 'node-pty is not available' };
  const shell = process.platform === 'win32' ? 'powershell.exe' : (process.env.SHELL || 'bash');
  const args = process.platform === 'win32' ? ['-NoLogo'] : [];
  const env = { ...process.env }; delete env.ELECTRON_RUN_AS_NODE;
  try {
    const id = ++ptySeq;
    const t = nodePty.spawn(shell, args, { name: 'xterm-256color', cols: cols || 80, rows: rows || 24, cwd: cwd || os.homedir(), env });
    ptys.set(id, t);
    t.onData(d => win && !win.isDestroyed() && win.webContents.send('pty:data', { id, data: d }));
    t.onExit(() => { ptys.delete(id); win && !win.isDestroyed() && win.webContents.send('pty:exit', { id }); });
    return { id };
  } catch (e) { return { error: e.message }; }
});
ipcMain.handle('pty:write', (_e, id, d) => { const t = ptys.get(id); if (t) t.write(d); });
ipcMain.handle('pty:resize', (_e, id, c, r) => { const t = ptys.get(id); try { if (t && c > 0 && r > 0) t.resize(c, r); } catch {} });
ipcMain.handle('pty:kill', (_e, id) => { const t = ptys.get(id); if (t) { try { t.kill(); } catch {} ptys.delete(id); } });

// ---------- clangd (LSP over stdio) ----------
let lsp = null;
ipcMain.handle('lsp:start', (_e, root, compileDir) => {
  if (lsp) { try { lsp.kill(); } catch {} lsp = null; }
  const exe = which('clangd'); if (!exe) return { error: 'clangd not found' };
  const args = ['--background-index', '--clang-tidy=false', '--header-insertion=never', '--completion-style=detailed', '--query-driver=**/g++*,**/gcc*,**/clang*,**/c++*,**/cc*', '--log=error'];
  if (compileDir) args.push('--compile-commands-dir=' + compileDir);
  const child = spawn(exe, args, { cwd: root, windowsHide: true });
  lsp = child; let buf = Buffer.alloc(0);
  child.stdout.on('data', (d) => {
    buf = Buffer.concat([buf, d]);
    for (;;) {
      const he = buf.indexOf('\r\n\r\n'); if (he < 0) break;
      const m = /Content-Length:\s*(\d+)/i.exec(buf.slice(0, he).toString('ascii')); if (!m) { buf = buf.slice(he + 4); continue; }
      const len = +m[1]; if (buf.length < he + 4 + len) break;
      const body = buf.slice(he + 4, he + 4 + len).toString('utf8'); buf = buf.slice(he + 4 + len);
      if (child === lsp && win && !win.isDestroyed()) win.webContents.send('lsp:msg', body);
    }
  });
  child.stderr.on('data', () => {});
  child.on('exit', (code) => { if (child === lsp) { lsp = null; win && !win.isDestroyed() && win.webContents.send('lsp:exit', code); } });
  child.on('error', () => {});
  return { ok: true };
});
ipcMain.handle('lsp:send', (_e, json) => {
  if (!lsp || !lsp.stdin.writable) return false;
  lsp.stdin.write(`Content-Length: ${Buffer.byteLength(json, 'utf8')}\r\n\r\n${json}`); return true;
});
ipcMain.handle('lsp:stop', () => { if (lsp) { try { lsp.kill(); } catch {} lsp = null; } });

// ---------- gdb (GDB/MI) ----------
let gdb = null;
ipcMain.handle('gdb:start', (_e, exe, cwd, args = []) => {
  if (gdb) { try { gdb.kill(); } catch {} gdb = null; }
  const g = which('gdb'); if (!g) return { error: 'gdb not found' };
  const child = spawn(g, ['--interpreter=mi3', '-q', '-nx', ...args, exe], { cwd, windowsHide: true });
  gdb = child; let buf = '';
  child.stdout.on('data', (d) => {
    buf += d.toString('utf8'); const lines = buf.split(/\r?\n/); buf = lines.pop();
    if (lines.length && win && !win.isDestroyed()) win.webContents.send('gdb:lines', lines);
  });
  child.stderr.on('data', (d) => win && !win.isDestroyed() && win.webContents.send('gdb:lines', ['@' + JSON.stringify(d.toString())]));
  child.on('exit', (code) => { if (child === gdb) { gdb = null; win && !win.isDestroyed() && win.webContents.send('gdb:exit', code); } });
  return { ok: true };
});
ipcMain.handle('gdb:send', (_e, line) => { if (gdb && gdb.stdin.writable) { gdb.stdin.write(line + '\n'); return true; } return false; });
ipcMain.handle('gdb:stop', () => {
  if (!gdb) return; const p = gdb; gdb = null;
  if (process.platform === 'win32') spawnSync('taskkill', ['/pid', String(p.pid), '/T', '/F']); else p.kill('SIGKILL');
  win && !win.isDestroyed() && win.webContents.send('gdb:exit', 0);
});
ipcMain.handle('gdb:interrupt', () => {
  // Windows has no SIGINT for child processes; break into the inferior via gdb's own console control.
  if (gdb && process.platform !== 'win32') gdb.kill('SIGINT');
});

// Dev aid: only active when NOVA_SHOTDIR is set (used to capture documentation screenshots).
ipcMain.handle('dev:shot', async (_e, name) => {
  if (!process.env.NOVA_SHOTDIR) return false;
  const img = await win.webContents.capturePage();
  fs.writeFileSync(path.join(process.env.NOVA_SHOTDIR, name + '.png'), img.toPNG()); return true;
});
ipcMain.handle('app:homedir', () => os.homedir());
ipcMain.handle('app:path', () => ({ cwd: process.cwd(), docs: app.getPath('documents') }));
ipcMain.handle('path:exists', (_e, p) => fs.existsSync(p));
ipcMain.handle('settings:load', () => {
  try { return JSON.parse(fs.readFileSync(path.join(app.getPath('userData'), 'settings.json'), 'utf8')); } catch { return {}; }
});
ipcMain.handle('settings:save', (_e, s) => { fs.writeFileSync(path.join(app.getPath('userData'), 'settings.json'), JSON.stringify(s, null, 2)); return true; });
ipcMain.handle('win:maximizeToggle', () => { win.isMaximized() ? win.unmaximize() : win.maximize(); });
ipcMain.handle('win:devtools', () => win.webContents.toggleDevTools());

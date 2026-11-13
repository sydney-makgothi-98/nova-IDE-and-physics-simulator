const { contextBridge, ipcRenderer } = require('electron');
const inv = (ch) => (...a) => ipcRenderer.invoke(ch, ...a);
contextBridge.exposeInMainWorld('nova', {
  readDir: inv('fs:readDir'), read: inv('fs:read'), write: inv('fs:write'), create: inv('fs:create'),
  rename: inv('fs:rename'), del: inv('fs:delete'), reveal: inv('fs:reveal'), listAll: inv('fs:listAll'), grep: inv('fs:grep'),
  openFolder: inv('dialog:openFolder'), openFile: inv('dialog:openFile'),
  detectTools: inv('tools:detect'),
  start: inv('proc:start'), stdin: inv('proc:stdin'), kill: inv('proc:kill'),
  onData: (cb) => ipcRenderer.on('proc:data', (_e, m) => cb(m)),
  onExit: (cb) => ipcRenderer.on('proc:exit', (_e, m) => cb(m)),
  exists: inv('path:exists'), loadSettings: inv('settings:load'), saveSettings: inv('settings:save'),
  ptySpawn: inv('pty:spawn'), ptyWrite: inv('pty:write'), ptyResize: inv('pty:resize'), ptyKill: inv('pty:kill'),
  onPtyData: (cb) => ipcRenderer.on('pty:data', (_e, m) => cb(m)), onPtyExit: (cb) => ipcRenderer.on('pty:exit', (_e, m) => cb(m)),
  lspStart: inv('lsp:start'), lspSend: inv('lsp:send'), lspStop: inv('lsp:stop'),
  onLspMsg: (cb) => ipcRenderer.on('lsp:msg', (_e, m) => cb(m)), onLspExit: (cb) => ipcRenderer.on('lsp:exit', (_e, c) => cb(c)),
  gdbStart: inv('gdb:start'), gdbSend: inv('gdb:send'), gdbStop: inv('gdb:stop'), gdbInterrupt: inv('gdb:interrupt'),
  onGdbLines: (cb) => ipcRenderer.on('gdb:lines', (_e, m) => cb(m)), onGdbExit: (cb) => ipcRenderer.on('gdb:exit', (_e, c) => cb(c)),
  devShot: inv('dev:shot'),
  paths: inv('app:path'), unbufHeader: inv('app:unbuf'), maximizeToggle: inv('win:maximizeToggle'), devtools: inv('win:devtools')
});

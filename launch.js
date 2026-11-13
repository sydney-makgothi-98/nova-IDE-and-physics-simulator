// Starts NOVA IDE. Clears ELECTRON_RUN_AS_NODE (set by some host tools), which would make Electron behave like plain Node.
const { spawn } = require('child_process');
const electron = require('electron');
const env = { ...process.env }; delete env.ELECTRON_RUN_AS_NODE;
spawn(electron, [__dirname, ...process.argv.slice(2)], { env, stdio: 'inherit', detached: true }).unref();

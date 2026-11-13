'use strict';
/* clangd language-server client + Monaco providers. Globals (S, key, base, ...) come from app.js. */
const LSP = { on: false, id: 0, pending: new Map(), versions: new Map(), timers: new Map(), diags: new Map(), starting: false, initialized: false, capsSent: false };

const lspUri = (p) => monaco.Uri.file(p).toString();
const lspPath = (uri) => monaco.Uri.parse(uri).fsPath;
const toPos = (p) => ({ line: p.lineNumber - 1, character: p.column - 1 });
const fromRange = (r) => new monaco.Range(r.start.line + 1, r.start.character + 1, r.end.line + 1, r.end.character + 1);
const lspLang = (tab) => (tab.model.getLanguageId() === 'c' ? 'c' : 'cpp');
const isCFamily = (tab) => ['c', 'cpp'].includes(tab.model.getLanguageId());

function lspSend(obj) { nova.lspSend(JSON.stringify({ jsonrpc: '2.0', ...obj })); }
function lspRequest(method, params, timeout = 8000) {
  if (!LSP.on) return Promise.resolve(null);
  return new Promise((res) => {
    const id = ++LSP.id; const t = setTimeout(() => { LSP.pending.delete(id); res(null); }, timeout);
    LSP.pending.set(id, (r) => { clearTimeout(t); res(r); }); lspSend({ id, method, params });
  });
}
const lspNotify = (method, params) => { if (LSP.on) lspSend({ method, params }); };

function setLspStatus(text) { const el = $('#stLsp'); if (el) { el.textContent = text; el.hidden = !text; } }

async function lspBoot() {
  LSP.on = false; LSP.initialized = false; LSP.versions.clear(); LSP.diags.clear(); LSP.pending.clear(); setLspStatus('');
  if (!S.root || !S.tools.clangd) return;
  const compileDir = await lspPrepareCompileDb();
  const r = await nova.lspStart(S.root, compileDir); if (r.error) return;
  LSP.on = true;
  const init = await lspRequest('initialize', {
    processId: null, rootUri: lspUri(S.root), workspaceFolders: [{ uri: lspUri(S.root), name: base(S.root) }],
    capabilities: {
      textDocument: {
        synchronization: { didSave: true }, publishDiagnostics: { relatedInformation: true },
        completion: { completionItem: { snippetSupport: true, documentationFormat: ['markdown', 'plaintext'], commitCharactersSupport: false }, contextSupport: true },
        hover: { contentFormat: ['markdown', 'plaintext'] }, signatureHelp: { signatureInformation: { documentationFormat: ['markdown', 'plaintext'], parameterInformation: { labelOffsetSupport: true } } },
        definition: {}, references: {}, documentSymbol: { hierarchicalDocumentSymbolSupport: true }, rename: { prepareSupport: false },
        formatting: {}, rangeFormatting: {}, codeAction: { codeActionLiteralSupport: { codeActionKind: { valueSet: ['quickfix', 'refactor', 'source'] } } }
      },
      workspace: { workspaceFolders: true, configuration: false, applyEdit: true }, window: { workDoneProgress: true }
    }
  }, 30000);
  if (!init) { LSP.on = false; return; }
  lspNotify('initialized', {}); LSP.initialized = true; setLspStatus('clangd');
  for (const t of S.tabs) if (isCFamily(t)) lspDidOpen(t);
}

/** Make sure clangd can find compile flags: CMake's compile_commands.json, or one we generate. */
async function lspPrepareCompileDb() {
  if (S.isCmake) {
    const dir = join(S.root, 'cmake-build-debug');
    if (await nova.exists(join(dir, 'compile_commands.json'))) return dir;
    if (S.tools.cmake) { // silent configure so clangd gets real flags
      setLspStatus('configuring CMake…');
      await ensureUnbuf(); const cfg = cmakeSteps(null).steps.find(s => s.cfg);
      if (cfg) { const r = await execCapture(cfg.cmd, cfg.args, cfg.cwd); if (r.code === 0) { S.cmakeDirty = false; S.cmakeCfgOk = true; } }
      if (await nova.exists(join(dir, 'compile_commands.json'))) return dir;
    }
  }
  // Fallback: synthesise a compilation database for loose files.
  const files = (await nova.listAll(S.root)).filter(f => /\.(c|cc|cpp|cxx)$/i.test(f) && !/cmake-build|[\\/]\.nova[\\/]/.test(f));
  const cxx = S.tools['g++'] ? 'g++' : 'clang++', cc = S.tools.gcc ? 'gcc' : 'clang';
  const db = files.map(f => { const c = ext(f) === 'c'; return { directory: dirname(f).split(SEP).join('/'), file: f.split(SEP).join('/'), arguments: [c ? cc : cxx, '-std=' + (c ? S.settings.cStd : S.settings.cppStd), '-I' + dirname(f).split(SEP).join('/'), '-I' + S.root.split(SEP).join('/'), '-c', f.split(SEP).join('/')] }; });
  const dbDir = join(S.root, '.nova');
  try { await nova.create(dbDir, true); } catch { }
  try { await nova.create(join(dbDir, 'compile_commands.json'), false); } catch { }
  await nova.write(join(dbDir, 'compile_commands.json'), JSON.stringify(db, null, 1));
  return dbDir;
}

nova.onLspMsg((raw) => {
  let m; try { m = JSON.parse(raw); } catch { return; }
  if (m.id !== undefined && (m.result !== undefined || m.error) && !m.method) { const cb = LSP.pending.get(m.id); if (cb) { LSP.pending.delete(m.id); cb(m.error ? null : m.result); } return; }
  if (m.method && m.id !== undefined) { // server -> client request
    if (m.method === 'workspace/applyEdit') { applyWorkspaceEdit(m.params.edit); lspSend({ id: m.id, result: { applied: true } }); }
    else lspSend({ id: m.id, result: null });
    return;
  }
  if (m.method === 'textDocument/publishDiagnostics') lspDiagnostics(m.params);
  else if (m.method === '$/progress') {
    const v = m.params.value || {};
    if (v.kind === 'begin') setLspStatus('clangd: ' + (v.title || 'working') + '…'); else if (v.kind === 'end') setLspStatus('clangd');
    else if (v.kind === 'report' && v.message) setLspStatus('clangd: ' + v.message);
  }
});
nova.onLspExit(() => { LSP.on = false; setLspStatus(''); });

/* --- document sync --- */
function lspDidOpen(tab) {
  if (!LSP.initialized || !isCFamily(tab)) return;
  LSP.versions.set(tab.path, 1);
  lspNotify('textDocument/didOpen', { textDocument: { uri: lspUri(tab.path), languageId: lspLang(tab), version: 1, text: tab.model.getValue() } });
}
function lspDidChange(tab) {
  if (!LSP.initialized || !isCFamily(tab) || !LSP.versions.has(tab.path)) return;
  clearTimeout(LSP.timers.get(tab.path));
  LSP.timers.set(tab.path, setTimeout(() => lspFlush(tab), 120));
}
function lspFlush(tab) {
  if (!LSP.versions.has(tab.path)) return;
  clearTimeout(LSP.timers.get(tab.path));
  const v = LSP.versions.get(tab.path) + 1; LSP.versions.set(tab.path, v);
  lspNotify('textDocument/didChange', { textDocument: { uri: lspUri(tab.path), version: v }, contentChanges: [{ text: tab.model.getValue() }] });
}
function lspDidSave(tab) { if (LSP.versions.has(tab.path)) { lspFlush(tab); lspNotify('textDocument/didSave', { textDocument: { uri: lspUri(tab.path) } }); } }
function lspDidClose(tab) {
  if (!LSP.versions.has(tab.path)) return; LSP.versions.delete(tab.path); LSP.diags.delete(key(tab.path));
  lspNotify('textDocument/didClose', { textDocument: { uri: lspUri(tab.path) } }); monaco.editor.setModelMarkers(tab.model, 'clangd', []); renderProblems();
}

/* --- diagnostics --- */
function lspDiagnostics(p) {
  const path = lspPath(p.uri), k = key(path);
  const list = p.diagnostics.map(d => ({ file: path, line: d.range.start.line + 1, col: d.range.start.character + 1, sev: d.severity === 1 ? 'error' : d.severity === 2 ? 'warning' : 'note', msg: d.message, range: d.range, raw: d, src: 'clangd' }));
  LSP.diags.set(k, list);
  const tab = S.tabs.find(t => key(t.path) === k);
  if (tab) monaco.editor.setModelMarkers(tab.model, 'clangd', list.map(d => ({
    startLineNumber: d.range.start.line + 1, startColumn: d.range.start.character + 1, endLineNumber: d.range.end.line + 1, endColumn: Math.max(d.range.end.character + 1, d.range.start.character + 2),
    message: d.msg, severity: d.sev === 'error' ? monaco.MarkerSeverity.Error : d.sev === 'warning' ? monaco.MarkerSeverity.Warning : monaco.MarkerSeverity.Info, source: 'clangd', code: d.raw.code ? String(d.raw.code) : undefined
  })));
  renderProblems();
}
const lspProblems = () => [...LSP.diags.values()].flat();

/* --- Monaco providers --- */
const CK = () => monaco.languages.CompletionItemKind;
const KIND_MAP = () => ({ 1: CK().Text, 2: CK().Method, 3: CK().Function, 4: CK().Constructor, 5: CK().Field, 6: CK().Variable, 7: CK().Class, 8: CK().Interface, 9: CK().Module, 10: CK().Property, 11: CK().Unit, 12: CK().Value, 13: CK().Enum, 14: CK().Keyword, 15: CK().Snippet, 16: CK().Color, 17: CK().File, 18: CK().Reference, 19: CK().Folder, 20: CK().EnumMember, 21: CK().Constant, 22: CK().Struct, 23: CK().Event, 24: CK().Operator, 25: CK().TypeParameter });
const md = (v) => !v ? undefined : typeof v === 'string' ? { value: v } : { value: v.value };

function lspRegisterProviders() {
  const langs = ['c', 'cpp'], km = KIND_MAP();
  const live = (model) => { const t = S.tabs.find(t => t.model === model); return t && LSP.initialized && LSP.versions.has(t.path) ? t : null; };
  const sync = (t) => lspFlush(t);
  for (const lang of langs) {
    monaco.languages.registerCompletionItemProvider(lang, {
      triggerCharacters: ['.', '>', ':', '<', '"', '/', '(', ','],
      async provideCompletionItems(model, pos, ctx) {
        const t = live(model); if (!t) return { suggestions: [] };
        sync(t);
        const r = await lspRequest('textDocument/completion', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos), context: { triggerKind: ctx.triggerKind === 1 ? 2 : 1, triggerCharacter: ctx.triggerCharacter } });
        if (!r) return { suggestions: [] };
        const items = Array.isArray(r) ? r : r.items; const w = model.getWordUntilPosition(pos);
        const def = new monaco.Range(pos.lineNumber, w.startColumn, pos.lineNumber, w.endColumn);
        return {
          incomplete: !!r.isIncomplete, suggestions: items.map(it => {
            const te = it.textEdit; const range = te ? fromRange(te.range || te.replace) : def;
            return { label: { label: it.label.trim(), detail: it.detail ? ' ' + it.detail : '', description: it.labelDetails && it.labelDetails.description }, kind: km[it.kind] || CK().Text, detail: it.detail, documentation: md(it.documentation), sortText: it.sortText, filterText: it.filterText || it.label,
              insertText: te ? te.newText : (it.insertText || it.label), range, insertTextRules: it.insertTextFormat === 2 ? monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet : undefined, preselect: it.preselect,
              additionalTextEdits: (it.additionalTextEdits || []).map(e => ({ range: fromRange(e.range), text: e.newText })) };
          })
        };
      }
    });
    monaco.languages.registerHoverProvider(lang, {
      async provideHover(model, pos) {
        const t = live(model); if (!t) return null; sync(t);
        const r = await lspRequest('textDocument/hover', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos) }); if (!r || !r.contents) return null;
        const c = r.contents; const arr = Array.isArray(c) ? c : [c];
        return { range: r.range ? fromRange(r.range) : undefined, contents: arr.map(x => typeof x === 'string' ? { value: x } : x.language ? { value: '```' + x.language + '\n' + x.value + '\n```' } : { value: x.value }) };
      }
    });
    monaco.languages.registerSignatureHelpProvider(lang, {
      signatureHelpTriggerCharacters: ['(', ','], signatureHelpRetriggerCharacters: [')'],
      async provideSignatureHelp(model, pos) {
        const t = live(model); if (!t) return null; sync(t);
        const r = await lspRequest('textDocument/signatureHelp', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos) }); if (!r || !r.signatures || !r.signatures.length) return null;
        return { value: { activeSignature: r.activeSignature || 0, activeParameter: r.activeParameter || 0, signatures: r.signatures.map(s => ({ label: s.label, documentation: md(s.documentation), parameters: (s.parameters || []).map(p => ({ label: p.label, documentation: md(p.documentation) })) })) }, dispose() { } };
      }
    });
    const locs = (r) => (!r ? [] : (Array.isArray(r) ? r : [r])).map(l => ({ uri: monaco.Uri.parse(l.uri || l.targetUri), range: fromRange(l.range || l.targetSelectionRange) }));
    monaco.languages.registerDefinitionProvider(lang, { async provideDefinition(model, pos) { const t = live(model); if (!t) return null; sync(t); return locs(await lspRequest('textDocument/definition', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos) })); } });
    monaco.languages.registerReferenceProvider(lang, { async provideReferences(model, pos) { const t = live(model); if (!t) return null; sync(t); return locs(await lspRequest('textDocument/references', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos), context: { includeDeclaration: true } })); } });
    monaco.languages.registerDocumentSymbolProvider(lang, {
      async provideDocumentSymbols(model) {
        const t = live(model); if (!t) return []; const r = await lspRequest('textDocument/documentSymbol', { textDocument: { uri: lspUri(t.path) } }); if (!r) return [];
        const conv = (s) => ({ name: s.name, detail: s.detail || '', kind: s.kind - 1, tags: [], range: fromRange(s.range || s.location.range), selectionRange: fromRange(s.selectionRange || s.range || s.location.range), children: (s.children || []).map(conv) });
        return r.map(conv);
      }
    });
    const fmt = async (model, range) => {
      const t = live(model); if (!t) return []; sync(t);
      const opts = { tabSize: 4, insertSpaces: true };
      const r = await lspRequest(range ? 'textDocument/rangeFormatting' : 'textDocument/formatting', { textDocument: { uri: lspUri(t.path) }, options: opts, ...(range ? { range: { start: toPos({ lineNumber: range.startLineNumber, column: range.startColumn }), end: toPos({ lineNumber: range.endLineNumber, column: range.endColumn }) } } : {}) });
      return (r || []).map(e => ({ range: fromRange(e.range), text: e.newText }));
    };
    monaco.languages.registerDocumentFormattingEditProvider(lang, { provideDocumentFormattingEdits: (m) => fmt(m) });
    monaco.languages.registerDocumentRangeFormattingEditProvider(lang, { provideDocumentRangeFormattingEdits: (m, r) => fmt(m, r) });
    monaco.languages.registerRenameProvider(lang, {
      async provideRenameEdits(model, pos, newName) {
        const t = live(model); if (!t) return { edits: [] }; sync(t);
        const r = await lspRequest('textDocument/rename', { textDocument: { uri: lspUri(t.path) }, position: toPos(pos), newName });
        if (!r) return { edits: [], rejectReason: 'Cannot rename this symbol' };
        return { edits: wsEditToMonaco(r) };
      },
      async resolveRenameLocation(model, pos) { const w = model.getWordAtPosition(pos); if (!w) return { range: new monaco.Range(1, 1, 1, 1), text: '', rejectReason: 'No symbol here' }; return { range: new monaco.Range(pos.lineNumber, w.startColumn, pos.lineNumber, w.endColumn), text: w.word }; }
    });
    monaco.languages.registerCodeActionProvider(lang, {
      async provideCodeActions(model, range, ctx) {
        const t = live(model); if (!t) return { actions: [], dispose() { } };
        const diags = (LSP.diags.get(key(t.path)) || []).filter(d => d.line >= range.startLineNumber && d.line <= range.endLineNumber).map(d => d.raw);
        const r = await lspRequest('textDocument/codeAction', { textDocument: { uri: lspUri(t.path) }, range: { start: toPos({ lineNumber: range.startLineNumber, column: range.startColumn }), end: toPos({ lineNumber: range.endLineNumber, column: range.endColumn }) }, context: { diagnostics: diags } });
        const actions = (r || []).filter(a => a.edit).map(a => ({ title: a.title, kind: a.kind || 'quickfix', isPreferred: a.isPreferred, edit: { edits: wsEditToMonaco(a.edit) } }));
        return { actions, dispose() { } };
      }
    });
  }
}
function wsEditToMonaco(we) {
  const out = []; const add = (uri, edits) => edits.forEach(e => out.push({ resource: monaco.Uri.parse(uri), textEdit: { range: fromRange(e.range), text: e.newText }, versionId: undefined }));
  if (we.changes) for (const [u, es] of Object.entries(we.changes)) add(u, es);
  if (we.documentChanges) for (const dc of we.documentChanges) if (dc.textDocument) add(dc.textDocument.uri, dc.edits);
  return out;
}
async function applyWorkspaceEdit(we) {
  const byUri = {}; (we.changes ? Object.entries(we.changes) : (we.documentChanges || []).filter(d => d.textDocument).map(d => [d.textDocument.uri, d.edits])).forEach(([u, e]) => (byUri[u] = (byUri[u] || []).concat(e)));
  for (const [u, edits] of Object.entries(byUri)) { await openFile(lspPath(u)); const tab = S.tabs.find(t => key(t.path) === key(lspPath(u))); if (tab) tab.model.pushEditOperations([], edits.map(e => ({ range: fromRange(e.range), text: e.newText })), () => null); }
}

/** Flat symbol list for the Structure panel, from clangd (falls back to regex view if unavailable). */
async function lspStructure(tab) {
  if (!LSP.initialized || !LSP.versions.has(tab.path)) return null;
  const r = await lspRequest('textDocument/documentSymbol', { textDocument: { uri: lspUri(tab.path) } }, 5000); if (!r) return null;
  const kindOf = (k) => ({ 5: 'c', 23: 's', 10: 'e', 3: 'n', 2: 'n', 12: 'f', 6: 'm', 9: 'm', 8: 'm', 13: 'v', 14: 'v', 7: 'v', 22: 'v' }[k] || 'f');
  const out = []; const walk = (list, depth) => list.forEach(s => { const rg = s.selectionRange || s.range || s.location.range; out.push({ k: kindOf(s.kind), name: s.name + (s.detail && [6, 9, 12].includes(s.kind) && s.detail.includes('(') ? s.detail.slice(s.detail.indexOf('(')) : ''), line: rg.start.line + 1, depth }); if (s.children) walk(s.children, depth + 1); });
  walk(r, 0); return out;
}

// Minimal stroke icon set (16x16 viewBox), original artwork.
window.ICONS = {
  chev: 'M4 6l4 4 4-4',
  right: 'M6 3l5 5-5 5',
  branch: 'M5 3v7M5 13a1.5 1.5 0 100-3 1.5 1.5 0 000 3zM5 5a1.5 1.5 0 100-3 1.5 1.5 0 000 3zM11 5a1.5 1.5 0 100-3 1.5 1.5 0 000 3zM11 5c0 4-6 2-6 5',
  cfg: 'M2.5 4h11M2.5 8h11M2.5 12h11M6 2.5v3M10 6.5v3M5 10.5v3',
  play: 'M4.5 3l8 5-8 5z',
  stop: 'M4 4h8v8H4z',
  bug: 'M8 5a3 3 0 013 3v2a3 3 0 01-6 0V8a3 3 0 013-3zM6 3l1 1.5M10 3L9 4.5M2.5 8H5M11 8h2.5M3 12l2-1.2M13 12l-2-1.2M3 4.5l2 1.5M13 4.5l-2 1.5',
  hammer: 'M9.5 2.5l4 4-2 2-1.2-1.2-5.6 5.6a1.2 1.2 0 01-1.7-1.7l5.6-5.6L7.5 4.4z',
  search: 'M7 12a5 5 0 100-10 5 5 0 000 10zM14 14l-3.4-3.4',
  gear: 'M8 10.2a2.2 2.2 0 100-4.4 2.2 2.2 0 000 4.4zM8 1.8l1 1.6 1.8-.4.6 1.8 1.8.6-.4 1.8 1.6 1-1.6 1 .4 1.8-1.8.6-.6 1.8-1.8-.4-1 1.6-1-1.6-1.8.4-.6-1.8-1.8-.6.4-1.8L1.2 8l1.6-1-.4-1.8 1.8-.6.6-1.8 1.8.4z',
  folder: 'M2 4.5A1.5 1.5 0 013.5 3h2.8l1.4 1.5h4.8A1.5 1.5 0 0114 6v5.5a1.5 1.5 0 01-1.5 1.5h-9A1.5 1.5 0 012 11.5z',
  struct: 'M3 3h4v3H3zM9 7h4v3H9zM9 11h4v3H9zM5 6v6.5h4M5 8.5h4',
  warn: 'M8 2.5l6 10.5H2zM8 7v3M8 11.3v.1',
  term: 'M2.5 3h11a1 1 0 011 1v8a1 1 0 01-1 1h-11a1 1 0 01-1-1V4a1 1 0 011-1zM4.5 6.5l2 1.5-2 1.5M8 10h3',
  newfile: 'M4 2h5l3 3v9H4zM9 2v3h3M8 7.5v4M6 9.5h4',
  collapse: 'M3 6l5-3 5 3M3 10l5 3 5-3',
  refresh: 'M13 8a5 5 0 11-1.6-3.7M13 2.5v3h-3',
  trash: 'M3 4.5h10M6.5 4.5V3h3v1.5M4.5 4.5l.6 8.5h5.8l.6-8.5M7 7v4M9 7v4',
  minus: 'M3.5 8h9',
  plus: 'M8 3v10M3 8h10',
  x: 'M4 4l8 8M12 4l-8 8',
  over: 'M2.5 9.5a5.5 5.5 0 0111 0M11 7l2.7 2.6L11 12.2M8 13.2v.1',
  into: 'M8 2.5v7M5 7l3 3 3-3M4 13h8',
  outof: 'M8 10.5v-7M5 6l3-3 3 3M4 13h8',
  cursor: 'M3 2.5l9 4.5-3.7 1.2L7 12z',
  file: 'M4 2h5l3 3v9H4z'
};
window.icon = (n) => `<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35" stroke-linecap="round" stroke-linejoin="round"><path d="${ICONS[n] || ''}"/></svg>`;
window.hydrateIcons = (root = document) => root.querySelectorAll('i[data-i]').forEach(el => { if (!el.firstChild) el.innerHTML = icon(el.dataset.i); });

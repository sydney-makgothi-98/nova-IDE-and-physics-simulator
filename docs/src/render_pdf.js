// Usage: electron render_pdf.js <input.html> <output.pdf> <withFooter: 1|0>
const { app, BrowserWindow } = require('electron');
const fs = require('fs');
const [html, pdf, withFooter, footText] = process.argv.slice(2);
app.disableHardwareAcceleration();
app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 900, height: 1200, webPreferences: { offscreen: false } });
  await win.loadFile(html);
  await new Promise(r => setTimeout(r, 800));
  const footer = `<div style="width:100%;font-family:'Times New Roman',serif;font-size:10px;color:#000;display:flex;justify-content:space-between;padding:0 25mm;">
      <span style="font-style:italic">${footText || 'How to Build the NOVA C-IDE — Team NOVA'}</span><span><span class="pageNumber"></span></span></div>`;
  const data = await win.webContents.printToPDF({
    pageSize: 'A4', printBackground: true, preferCSSPageSize: false,
    displayHeaderFooter: withFooter === '1', headerTemplate: '<span></span>', footerTemplate: footer,
    margins: { top: 0.95, bottom: 0.95, left: 0.95, right: 0.95 }
  });
  fs.writeFileSync(pdf, data);
  app.quit();
});

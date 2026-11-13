#include "xlsx.hpp"
#include <zlib.h>
#include <cstdio>
#include <cstring>

namespace vlab {

// ====================================================================== ZIP
static void put16(std::string& s, unsigned v) { s += (char)(v & 0xFF); s += (char)((v >> 8) & 0xFF); }
static void put32(std::string& s, unsigned v) { put16(s, v & 0xFFFF); put16(s, (v >> 16) & 0xFFFF); }

bool ZipWriter::open(const std::string& path) {
    f_ = std::fopen(path.c_str(), "wb");
    entries_.clear();
    offset_ = 0;
    return f_ != nullptr;
}

bool ZipWriter::add(const std::string& name, const std::string& data) {
    if (!f_) return false;
    std::string comp;
    {   // raw deflate stream (no zlib header), as required inside a ZIP file
        z_stream zs;
        std::memset(&zs, 0, sizeof zs);
        if (deflateInit2(&zs, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
        comp.resize(deflateBound(&zs, (uLong)data.size()) + 16);
        zs.next_in = (Bytef*)data.data();
        zs.avail_in = (uInt)data.size();
        zs.next_out = (Bytef*)&comp[0];
        zs.avail_out = (uInt)comp.size();
        int rc = deflate(&zs, Z_FINISH);
        comp.resize(zs.total_out);
        deflateEnd(&zs);
        if (rc != Z_STREAM_END) return false;
    }
    Entry e;
    e.name = name;
    e.crc = (unsigned)crc32(0L, (const Bytef*)data.data(), (uInt)data.size());
    e.csize = (unsigned)comp.size();
    e.usize = (unsigned)data.size();
    e.offset = offset_;
    std::string h;
    put32(h, 0x04034b50);
    put16(h, 20);            // version needed
    put16(h, 0x0800);        // flags: UTF-8 names
    put16(h, 8);             // method: deflate
    put16(h, 0);             // time
    put16(h, 0x21);          // date (1980-01-01)
    put32(h, e.crc);
    put32(h, e.csize);
    put32(h, e.usize);
    put16(h, (unsigned)name.size());
    put16(h, 0);
    h += name;
    if (std::fwrite(h.data(), 1, h.size(), (FILE*)f_) != h.size()) return false;
    if (!comp.empty() && std::fwrite(comp.data(), 1, comp.size(), (FILE*)f_) != comp.size()) return false;
    offset_ += (unsigned)(h.size() + comp.size());
    entries_.push_back(e);
    return true;
}

bool ZipWriter::close() {
    if (!f_) return false;
    std::string cd;
    for (const Entry& e : entries_) {
        put32(cd, 0x02014b50);
        put16(cd, 20);
        put16(cd, 20);
        put16(cd, 0x0800);
        put16(cd, 8);
        put16(cd, 0);
        put16(cd, 0x21);
        put32(cd, e.crc);
        put32(cd, e.csize);
        put32(cd, e.usize);
        put16(cd, (unsigned)e.name.size());
        put16(cd, 0);
        put16(cd, 0);
        put16(cd, 0);
        put16(cd, 0);
        put32(cd, 0);
        put32(cd, e.offset);
        cd += e.name;
    }
    std::string end;
    put32(end, 0x06054b50);
    put16(end, 0);
    put16(end, 0);
    put16(end, (unsigned)entries_.size());
    put16(end, (unsigned)entries_.size());
    put32(end, (unsigned)cd.size());
    put32(end, offset_);
    put16(end, 0);
    bool ok = std::fwrite(cd.data(), 1, cd.size(), (FILE*)f_) == cd.size() && std::fwrite(end.data(), 1, end.size(), (FILE*)f_) == end.size();
    ok = (std::fclose((FILE*)f_) == 0) && ok;
    f_ = nullptr;
    return ok;
}

// ====================================================================== XML helpers
static std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default:
                if (c < 0x20 && c != '\t' && c != '\n') break;          // control characters are illegal in XML
                o += (char)c;
        }
    }
    return o;
}

static std::string numText(double v) {
    char b[40];
    if (v != v || v > 1e300 || v < -1e300) return "0";
    std::snprintf(b, sizeof b, "%.12g", v);
    return b;
}

std::string XlsxWorkbook::columnLetters(int c) {
    std::string s;
    for (c += 1; c > 0; c = (c - 1) / 26) s.insert(s.begin(), (char)('A' + (c - 1) % 26));
    return s;
}

// ====================================================================== sheets
void XlsxSheet::addRow(const std::vector<XCell>& cells) {
    ++row_;
    if (cells.empty()) return;
    body_ += "<row r=\"" + std::to_string(row_) + "\">";
    for (size_t c = 0; c < cells.size(); ++c) {
        const XCell& x = cells[c];
        if (x.kind == XCell::Empty) continue;
        std::string ref = XlsxWorkbook::columnLetters((int)c) + std::to_string(row_);
        std::string st = x.style ? " s=\"" + std::to_string(x.style) + "\"" : "";
        if (x.kind == XCell::Num) body_ += "<c r=\"" + ref + "\"" + st + "><v>" + numText(x.num) + "</v></c>";
        else body_ += "<c r=\"" + ref + "\"" + st + " t=\"inlineStr\"><is><t xml:space=\"preserve\">" + esc(x.str) + "</t></is></c>";
    }
    body_ += "</row>";
    if ((int)cells.size() > maxCol_) maxCol_ = (int)cells.size();
}

std::string XlsxSheet::xml(bool selected, bool hasDrawing) const {
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                    "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
    int rows = row_ > 0 ? row_ : 1, cols = maxCol_ > 0 ? maxCol_ : 1;
    s += "<dimension ref=\"A1:" + XlsxWorkbook::columnLetters(cols - 1) + std::to_string(rows) + "\"/>";
    s += "<sheetViews><sheetView workbookViewId=\"0\"";
    if (selected) s += " tabSelected=\"1\"";
    s += ">";
    if (freeze_) s += "<pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" state=\"frozen\"/><selection pane=\"bottomLeft\"/>";
    s += "</sheetView></sheetViews><sheetFormatPr defaultRowHeight=\"15\"/>";
    bool anyWidth = false;
    for (double w : widths_) anyWidth = anyWidth || w > 0;
    if (anyWidth) {
        s += "<cols>";
        for (size_t i = 0; i < widths_.size(); ++i)
            if (widths_[i] > 0)
                s += "<col min=\"" + std::to_string(i + 1) + "\" max=\"" + std::to_string(i + 1) + "\" width=\"" + numText(widths_[i]) + "\" customWidth=\"1\"/>";
        s += "</cols>";
    }
    s += "<sheetData>" + body_ + "</sheetData>";
    s += "<pageMargins left=\"0.7\" right=\"0.7\" top=\"0.75\" bottom=\"0.75\" header=\"0.3\" footer=\"0.3\"/>";
    if (hasDrawing) s += "<drawing r:id=\"rId1\"/>";
    s += "</worksheet>";
    return s;
}

// ====================================================================== charts
static std::string rich(const std::string& text, bool vertical) {
    return std::string("<c:tx><c:rich><a:bodyPr") + (vertical ? " rot=\"-5400000\" vert=\"horz\"" : "") +
           "/><a:lstStyle/><a:p><a:pPr><a:defRPr/></a:pPr><a:r><a:rPr lang=\"en-US\"/><a:t>" + esc(text) + "</a:t></a:r></a:p></c:rich></c:tx><c:layout/><c:overlay val=\"0\"/>";
}

static std::string cellRef(const std::string& sheet, int col, int row) {          // Sheet!$B$1 (row is 0-based)
    return "'" + sheet + "'!$" + XlsxWorkbook::columnLetters(col) + "$" + std::to_string(row + 1);
}
static std::string rangeRef(const std::string& sheet, int col, int r0, int r1) {
    std::string L = XlsxWorkbook::columnLetters(col);
    return "'" + sheet + "'!$" + L + "$" + std::to_string(r0 + 1) + ":$" + L + "$" + std::to_string(r1 + 1);
}

static std::string chartXml(const XChart& ch) {
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                    "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
                    "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><c:lang val=\"en-US\"/><c:chart>";
    s += "<c:title>" + rich(ch.title, false) + "</c:title><c:autoTitleDeleted val=\"0\"/><c:plotArea><c:layout/><c:scatterChart><c:scatterStyle val=\"lineMarker\"/><c:varyColors val=\"0\"/>";
    int idx = 0;
    for (const XChartSeries& se : ch.series) {
        s += "<c:ser><c:idx val=\"" + std::to_string(idx) + "\"/><c:order val=\"" + std::to_string(idx) + "\"/>";
        s += "<c:tx><c:strRef><c:f>" + esc(cellRef(se.nameSheet, se.nameCol, se.nameRow)) + "</c:f><c:strCache><c:ptCount val=\"1\"/><c:pt idx=\"0\"><c:v>" +
             esc(se.cachedName) + "</c:v></c:pt></c:strCache></c:strRef></c:tx>";
        s += "<c:spPr><a:ln w=\"19050\" cap=\"rnd\"><a:round/></a:ln></c:spPr><c:marker><c:symbol val=\"none\"/></c:marker>";
        s += "<c:xVal><c:numRef><c:f>" + esc(rangeRef(se.sheet, se.xCol, se.rowFirst, se.rowLast)) + "</c:f></c:numRef></c:xVal>";
        s += "<c:yVal><c:numRef><c:f>" + esc(rangeRef(se.sheet, se.yCol, se.rowFirst, se.rowLast)) + "</c:f></c:numRef></c:yVal><c:smooth val=\"0\"/></c:ser>";
        ++idx;
    }
    s += "<c:axId val=\"50010001\"/><c:axId val=\"50010002\"/></c:scatterChart>";
    s += "<c:valAx><c:axId val=\"50010001\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling><c:delete val=\"0\"/><c:axPos val=\"b\"/><c:title>" + rich(ch.xTitle, false) +
         "</c:title><c:numFmt formatCode=\"General\" sourceLinked=\"1\"/><c:majorTickMark val=\"out\"/><c:minorTickMark val=\"none\"/><c:tickLblPos val=\"nextTo\"/>"
         "<c:crossAx val=\"50010002\"/><c:crosses val=\"autoZero\"/><c:crossBetween val=\"midCat\"/></c:valAx>";
    s += "<c:valAx><c:axId val=\"50010002\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling><c:delete val=\"0\"/><c:axPos val=\"l\"/><c:majorGridlines/><c:title>" + rich(ch.yTitle, true) +
         "</c:title><c:numFmt formatCode=\"General\" sourceLinked=\"1\"/><c:majorTickMark val=\"out\"/><c:minorTickMark val=\"none\"/><c:tickLblPos val=\"nextTo\"/>"
         "<c:crossAx val=\"50010001\"/><c:crosses val=\"autoZero\"/><c:crossBetween val=\"midCat\"/></c:valAx>";
    s += "</c:plotArea><c:legend><c:legendPos val=\"b\"/><c:layout/><c:overlay val=\"0\"/></c:legend><c:plotVisOnly val=\"1\"/><c:dispBlanksAs val=\"gap\"/></c:chart>";
    s += "<c:printSettings><c:headerFooter/><c:pageMargins b=\"0.75\" l=\"0.7\" r=\"0.7\" t=\"0.75\" header=\"0.3\" footer=\"0.3\"/><c:pageSetup/></c:printSettings></c:chartSpace>";
    return s;
}

static std::string drawingXml(const std::vector<XChart>& charts) {
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                    "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
                    "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">";
    for (size_t i = 0; i < charts.size(); ++i) {
        const XChart& c = charts[i];
        s += "<xdr:twoCellAnchor><xdr:from><xdr:col>" + std::to_string(c.anchorCol) + "</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>" + std::to_string(c.anchorRow) +
             "</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from><xdr:to><xdr:col>" + std::to_string(c.anchorCol + c.widthCols) + "</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>" +
             std::to_string(c.anchorRow + c.heightRows) + "</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>";
        s += "<xdr:graphicFrame macro=\"\"><xdr:nvGraphicFramePr><xdr:cNvPr id=\"" + std::to_string(i + 2) + "\" name=\"Chart " + std::to_string(i + 1) +
             "\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr><xdr:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/></xdr:xfrm><a:graphic>"
             "<a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\"><c:chart xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
             "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" r:id=\"rId" + std::to_string(i + 1) +
             "\"/></a:graphicData></a:graphic></xdr:graphicFrame><xdr:clientData/></xdr:twoCellAnchor>";
    }
    s += "</xdr:wsDr>";
    return s;
}

// ====================================================================== workbook
XlsxSheet& XlsxWorkbook::addSheet(const std::string& name) {
    std::string n;
    for (char c : name) if (std::string("[]:*?/\\").find(c) == std::string::npos) n += c;
    if (n.size() > 31) n.resize(31);
    if (n.empty()) n = "Sheet";
    sheets_.emplace_back(n);
    return sheets_.back();
}

bool XlsxWorkbook::save(const std::string& path, std::string* error) const {
    ZipWriter z;
    if (!z.open(path)) {
        if (error) *error = "Could not create the file (is it open in Excel?)";
        return false;
    }
    const std::string hdr = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    int chartCount = 0, drawingCount = 0;
    std::string types = hdr + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                              "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                              "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                              "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
                              "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>";
    std::string wbSheets, wbRels = hdr + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    bool ok = true;
    for (size_t i = 0; i < sheets_.size(); ++i) {
        const XlsxSheet& sh = sheets_[i];
        std::string n = std::to_string(i + 1);
        types += "<Override PartName=\"/xl/worksheets/sheet" + n + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
        wbSheets += "<sheet name=\"" + esc(sh.name_) + "\" sheetId=\"" + n + "\" r:id=\"rId" + n + "\"/>";
        wbRels += "<Relationship Id=\"rId" + n + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" + n + ".xml\"/>";
        bool hasDrawing = !sh.charts.empty();
        ok = ok && z.add("xl/worksheets/sheet" + n + ".xml", sh.xml(i == 0, hasDrawing));
        if (hasDrawing) {
            ++drawingCount;
            std::string d = std::to_string(drawingCount);
            ok = ok && z.add("xl/worksheets/_rels/sheet" + n + ".xml.rels",
                             hdr + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" "
                                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing" + d + ".xml\"/></Relationships>");
            ok = ok && z.add("xl/drawings/drawing" + d + ".xml", drawingXml(sh.charts));
            types += "<Override PartName=\"/xl/drawings/drawing" + d + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>";
            std::string drels = hdr + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
            for (size_t k = 0; k < sh.charts.size(); ++k) {
                ++chartCount;
                std::string c = std::to_string(chartCount);
                drels += "<Relationship Id=\"rId" + std::to_string(k + 1) + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart" + c + ".xml\"/>";
                ok = ok && z.add("xl/charts/chart" + c + ".xml", chartXml(sh.charts[k]));
                types += "<Override PartName=\"/xl/charts/chart" + c + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/>";
            }
            drels += "</Relationships>";
            ok = ok && z.add("xl/drawings/_rels/drawing" + d + ".xml.rels", drels);
        }
    }
    wbRels += "<Relationship Id=\"rId" + std::to_string(sheets_.size() + 1) + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/></Relationships>";
    types += "</Types>";
    // styles: 0 normal, 1 header (bold, grey fill, bottom border), 2 title (bold 16), 3 bold, 4 note (italic grey)
    std::string styles = hdr +
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"5\">"
        "<font><sz val=\"11\"/><color rgb=\"FF000000\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "<font><b/><sz val=\"11\"/><color rgb=\"FF000000\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "<font><b/><sz val=\"16\"/><color rgb=\"FF1F3864\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "<font><b/><sz val=\"11\"/><color rgb=\"FF000000\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "<font><i/><sz val=\"10\"/><color rgb=\"FF666666\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "</fonts>"
        "<fills count=\"3\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFD9E2F3\"/><bgColor indexed=\"64\"/></patternFill></fill></fills>"
        "<borders count=\"2\"><border><left/><right/><top/><bottom/><diagonal/></border>"
        "<border><left/><right/><top/><bottom style=\"thin\"><color auto=\"1\"/></bottom><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"5\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"3\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"4\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "</cellXfs><cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles><dxfs count=\"0\"/>"
        "<tableStyles count=\"0\" defaultTableStyle=\"TableStyleMedium9\" defaultPivotStyle=\"PivotStyleLight16\"/></styleSheet>";
    ok = ok && z.add("[Content_Types].xml", types);
    ok = ok && z.add("_rels/.rels", hdr + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" "
                                          "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>");
    ok = ok && z.add("xl/workbook.xml", hdr + "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                                              "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><bookViews><workbookView/></bookViews><sheets>" +
                                              wbSheets + "</sheets></workbook>");
    ok = ok && z.add("xl/_rels/workbook.xml.rels", wbRels);
    ok = ok && z.add("xl/styles.xml", styles);
    ok = z.close() && ok;
    if (!ok && error) *error = "Writing the file failed (disk full, or the file is open in Excel).";
    return ok;
}

}  // namespace vlab

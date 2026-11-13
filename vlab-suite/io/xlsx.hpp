// xlsx.hpp -- writes a real Excel workbook (.xlsx) with data sheets AND embedded charts, with no dependency on Excel.
// An .xlsx file is a ZIP of small XML files; this is a minimal, strict writer for exactly what the suite needs.
#pragma once
#include <string>
#include <vector>

namespace vlab {

// ------------------------------------------------------------------ ZIP (deflate via zlib)
class ZipWriter {
public:
    bool open(const std::string& path);
    bool add(const std::string& name, const std::string& data);
    bool close();
private:
    struct Entry { std::string name; unsigned crc; unsigned csize, usize, offset; };
    void* f_ = nullptr;
    std::vector<Entry> entries_;
    unsigned offset_ = 0;
};

// ------------------------------------------------------------------ workbook
struct XCell {
    enum Kind { Empty, Num, Str } kind = Empty;
    double num = 0;
    std::string str;
    int style = 0;                               // 0 normal, 1 header, 2 title, 3 bold, 4 note
    XCell() = default;
    XCell(double v, int st = 0) : kind(Num), num(v), style(st) {}
    XCell(const std::string& s, int st = 0) : kind(Str), str(s), style(st) {}
    XCell(const char* s, int st = 0) : kind(Str), str(s), style(st) {}
};

struct XChartSeries {
    std::string nameSheet;  int nameRow = 0, nameCol = 0;                   // 0-based header cell holding the series name
    std::string sheet;      int xCol = 0, yCol = 0, rowFirst = 1, rowLast = 1;   // 0-based columns/rows of the data
    std::string cachedName;                                                 // text of the header cell (shown even before recalculation)
};
struct XChart {
    std::string title, xTitle, yTitle;
    std::vector<XChartSeries> series;
    int anchorCol = 1, anchorRow = 1;            // 0-based top-left cell on the chart sheet
    int widthCols = 11, heightRows = 20;
};

class XlsxSheet {
public:
    explicit XlsxSheet(std::string name) : name_(std::move(name)) {}
    const std::string& name() const { return name_; }
    void addRow(const std::vector<XCell>& cells);        // appends the next row
    void skipRow() { ++row_; }
    void setColumnWidth(int col, double chars) { if ((int)widths_.size() <= col) widths_.resize(col + 1, 0); widths_[col] = chars; }
    void freezeFirstRow() { freeze_ = true; }
    int nextRow() const { return row_; }                  // 0-based index of the row that the next addRow() writes
    std::string xml(bool selected, bool hasDrawing) const;
    std::vector<XChart> charts;                           // charts anchored on this sheet
private:
    friend class XlsxWorkbook;
    std::string name_, body_;
    int row_ = 0, maxCol_ = 0;
    std::vector<double> widths_;
    bool freeze_ = false;
};

class XlsxWorkbook {
public:
    XlsxSheet& addSheet(const std::string& name);
    bool save(const std::string& path, std::string* error = nullptr) const;
    static std::string columnLetters(int col0);          // 0 -> A, 25 -> Z, 26 -> AA
private:
    std::vector<XlsxSheet> sheets_;
};

}  // namespace vlab

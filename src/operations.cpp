#include "operations.h"
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <QFile>
#include <QBuffer>
#include <QByteArray>
#include <QStringList>
#include <cmath>
#include <memory>
#include <sstream>

namespace Operations {

static std::vector<QPDFPageObjectHelper> pages(QPDF& pdf) {
    return QPDFPageDocumentHelper(pdf).getAllPages();
}

// QPDF Buffer::getBuffer() returns unsigned char* with no str() method.
static std::string bufToStr(const std::shared_ptr<Buffer>& buf) {
    return std::string(reinterpret_cast<const char*>(buf->getBuffer()),
                       buf->getSize());
}

static std::string streamToStr(QPDFObjectHandle obj) {
    std::string out;
    if (obj.isStream())
        out = bufToStr(obj.getStreamData(qpdf_dl_all));
    else if (obj.isArray())
        for (int i = 0; i < obj.getArrayNItems(); ++i)
            out += bufToStr(obj.getArrayItem(i).getStreamData(qpdf_dl_all));
    return out;
}

QByteArray toBytes(QPDF& pdf) {
    QPDFWriter w(pdf);
    w.setOutputMemory();
    w.write();
    auto buf = w.getBuffer();
    return QByteArray(reinterpret_cast<const char*>(buf->getBuffer()),
                      static_cast<int>(buf->getSize()));
}

static void writeTo(QPDF& pdf, const QString& path) {
    QPDFWriter w(pdf, path.toStdString().c_str());
    w.setLinearization(false);
    w.write();
}

void applyPageOrder(QPDF& pdf, const QVector<int>& order) {
    auto src = pages(pdf);
    QPDFPageDocumentHelper pdh(pdf);
    // Remove all pages, re-add in new order
    for (auto& p : src) pdh.removePage(p);
    for (int idx : order) pdh.addPage(src[idx].getObjectHandle(), false);
}

void deletePages(QPDF& pdf, const QVector<int>& indices) {
    auto ps = pages(pdf);
    QPDFPageDocumentHelper pdh(pdf);
    // Iterate in reverse so indices stay valid
    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        if (i >= 0 && i < (int)ps.size())
            pdh.removePage(ps[i]);
    }
}

void rotatePage(QPDF& pdf, int pageIdx, int degrees) {
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;
    auto obj = ps[pageIdx].getObjectHandle();
    int cur = 0;
    if (obj.hasKey("/Rotate") && obj.getKey("/Rotate").isInteger())
        cur = obj.getKey("/Rotate").getIntValueAsInt();
    obj.replaceKey("/Rotate", QPDFObjectHandle::newInteger((cur + degrees + 360) % 360));
}

static QPDFObjectHandle makeRect(double x0,double y0,double x1,double y1) {
    return QPDFObjectHandle::newArray({
        QPDFObjectHandle::newReal(x0), QPDFObjectHandle::newReal(y0),
        QPDFObjectHandle::newReal(x1), QPDFObjectHandle::newReal(y1)
    });
}

void cropPage(QPDF& pdf, int pageIdx,
              double left, double bottom, double right, double top)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;
    auto obj = ps[pageIdx].getObjectHandle();

    // Get current MediaBox
    auto mb = obj.getKey("/MediaBox");
    if (!mb.isArray()) return;
    double px0 = mb.getArrayItem(0).getNumericValue();
    double py0 = mb.getArrayItem(1).getNumericValue();
    double px1 = mb.getArrayItem(2).getNumericValue();
    double py1 = mb.getArrayItem(3).getNumericValue();

    double w = px1 - px0, h = py1 - py0;
    auto crop = makeRect(px0 + left, py0 + bottom, px1 - right, py1 - top);
    obj.replaceKey("/CropBox", crop);
    obj.replaceKey("/MediaBox", crop);
}

void cropAllPages(QPDF& pdf,
                  double left, double bottom, double right, double top)
{
    int n = (int)pages(pdf).size();
    for (int i = 0; i < n; ++i)
        cropPage(pdf, i, left, bottom, right, top);
}

void mergeInto(QPDF& pdf, QPDF& src, int insertAt) {
    QPDFPageDocumentHelper pdh(pdf);
    int insertPos = (insertAt < 0 || insertAt >= (int)pdh.getAllPages().size())
                    ? (int)pdh.getAllPages().size() : insertAt;
    int added = 0;
    for (auto& p : pages(src)) {
        auto copied = pdf.copyForeignObject(p.getObjectHandle());
        if (insertPos + added >= (int)pdh.getAllPages().size())
            pdh.addPage(copied, false);
        else
            pdh.addPageAt(copied, true, pdh.getAllPages()[insertPos + added]);
        ++added;
    }
}

void mergeInto(QPDF& pdf, const QStringList& otherPaths, int insertAt) {
    QPDFPageDocumentHelper pdh(pdf);
    auto existing = pages(pdf);
    int insertPos = (insertAt < 0 || insertAt >= (int)existing.size())
                    ? (int)existing.size() : insertAt;

    int added = 0;
    for (const auto& path : otherPaths) {
        QPDF other;
        other.processFile(path.toStdString().c_str());
        for (auto& p : pages(other)) {
            auto copied = pdf.copyForeignObject(p.getObjectHandle());
            // Insert at position
            if (insertPos + added >= (int)pdh.getAllPages().size())
                pdh.addPage(copied, false);
            else
                pdh.addPageAt(copied, true, pdh.getAllPages()[insertPos + added]);
            ++added;
        }
    }
}

static std::shared_ptr<QPDF> extractPages(QPDF& src, int first, int last) {
    auto out = std::make_shared<QPDF>();
    out->emptyPDF();
    QPDFPageDocumentHelper odh(*out);
    auto ps = pages(src);
    for (int i = first; i <= last && i < (int)ps.size(); ++i)
        odh.addPage(out->copyForeignObject(ps[i].getObjectHandle()), false);
    return out;
}

QStringList splitByRanges(QPDF& pdf, const QVector<Range>& ranges,
                           const QString& outDir, const QString& baseName)
{
    QStringList out;
    int n = 1;
    for (const auto& r : ranges) {
        auto part = extractPages(pdf, r.first, r.last);
        QString path = outDir + "/" + baseName + QString("_%1.pdf").arg(n++);
        writeTo(*part, path);
        out << path;
    }
    return out;
}

QStringList splitEveryN(QPDF& pdf, int n,
                        const QString& outDir, const QString& baseName)
{
    int total = (int)pages(pdf).size();
    QVector<Range> ranges;
    for (int i = 0; i < total; i += n)
        ranges.append({i, std::min(i+n-1, total-1)});
    return splitByRanges(pdf, ranges, outDir, baseName);
}

QStringList splitEachPage(QPDF& pdf,
                          const QString& outDir, const QString& baseName)
{
    int total = (int)pages(pdf).size();
    QVector<Range> ranges;
    for (int i = 0; i < total; ++i) ranges.append({i, i});
    return splitByRanges(pdf, ranges, outDir, baseName);
}

QVector<Range> parseRanges(const QString& text, int total) {
    QVector<Range> out;
    for (const auto& part : text.split(',', Qt::SkipEmptyParts)) {
        auto t = part.trimmed();
        auto dash = t.indexOf('-');
        if (dash > 0) {
            int a = t.left(dash).toInt() - 1;
            int b = t.mid(dash+1).toInt() - 1;
            if (a >= 0 && b >= a && a < total)
                out.append({a, std::min(b, total-1)});
        } else {
            int p = t.toInt() - 1;
            if (p >= 0 && p < total) out.append({p, p});
        }
    }
    return out;
}

void overlaySignatureOnPage(QPDF& pdf, int pageIdx,
                             const QString& sigPdfPath,
                             const QString& position,
                             double margin)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;

    QPDF sigPdf;
    sigPdf.processFile(sigPdfPath.toStdString().c_str());
    auto sigPages = pages(sigPdf);
    if (sigPages.empty()) return;

    auto sigPage = sigPages[0].getObjectHandle();
    auto sigMb   = sigPage.getKey("/MediaBox");
    double sw = sigMb.getArrayItem(2).getNumericValue();
    double sh = sigMb.getArrayItem(3).getNumericValue();

    auto pageObj = ps[pageIdx].getObjectHandle();
    auto mb = pageObj.getKey("/MediaBox");
    double pw = mb.getArrayItem(2).getNumericValue();
    double ph = mb.getArrayItem(3).getNumericValue();

    double x = margin, y = margin;
    if (position == "bottom-right")  { x = pw - sw - margin; y = margin; }
    else if (position == "top-left") { x = margin; y = ph - sh - margin; }
    else if (position == "top-right"){ x = pw - sw - margin; y = ph - sh - margin; }
    else if (position == "center")   { x = (pw-sw)/2; y = (ph-sh)/2; }

    // Build Form XObject from sig page content
    std::string streamData = streamToStr(sigPage.getKey("/Contents"));

    auto form = QPDFObjectHandle::newStream(&pdf, streamData);
    auto formDict = form.getDict();
    formDict.replaceKey("/Type",    QPDFObjectHandle::newName("/XObject"));
    formDict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    formDict.replaceKey("/FormType",QPDFObjectHandle::newInteger(1));
    formDict.replaceKey("/BBox",    makeRect(0,0,sw,sh));

    if (sigPage.hasKey("/Resources")) {
        auto res = sigPage.getKey("/Resources");
        formDict.replaceKey("/Resources",
                            pdf.copyForeignObject(sigPdf.makeIndirectObject(res)));
    }

    auto indForm = pdf.makeIndirectObject(form);
    static int sigCount = 0;
    std::string ns = "PMS" + std::to_string(++sigCount);

    if (!pageObj.hasKey("/Resources"))
        pageObj.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
    auto res = pageObj.getKey("/Resources");
    if (!res.hasKey("/XObject"))
        res.replaceKey("/XObject", QPDFObjectHandle::newDictionary());
    res.getKey("/XObject").replaceKey("/" + ns, indForm);

    std::ostringstream op;
    op << "\nq 1 0 0 1 " << x << " " << y << " cm /" << ns << " Do Q\n";

    std::string existingData = streamToStr(pageObj.getKey("/Contents"));
    existingData += op.str();
    pageObj.replaceKey("/Contents",
                       QPDFObjectHandle::newStream(&pdf, existingData));
}

// ── Port of pdfarranger image_utils.rgba_pil_to_pdf ───────────────────────────
// FlateDecode-compress a QByteArray for use as a PDF stream.
// qCompress() prepends a 4-byte big-endian uncompressed-size header that PDF
// does not understand — skip those first 4 bytes to get a plain zlib stream.
static std::string flateDecode(const QByteArray& data) {
    QByteArray compressed = qCompress(data, 6);
    // Bytes 0-3 = Qt's uncompressed-length header; bytes 4+ = valid zlib stream
    return std::string(compressed.constData() + 4,
                       static_cast<size_t>(compressed.size() - 4));
}

// Exact port of rgba_pil_to_pdf: RGB stream + SMask (alpha) stream → true transparency.
// No white box — signature ink only.
void overlayImageOnPage(QPDF& pdf, int pageIdx, const QImage& img,
                        double sigW, double sigH, double x, double y)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;

    // Work in RGBA8888 — preserves every alpha bit from remove_bg / autocrop
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    int w = rgba.width(), h = rgba.height();
    if (w <= 0 || h <= 0) return;

    // Split channels: interleaved RGBA → separate RGB and A byte arrays
    QByteArray rgbData, alphaData;
    rgbData.reserve(w * h * 3);
    alphaData.reserve(w * h);
    for (int row = 0; row < h; ++row) {
        const uchar* line = rgba.constScanLine(row);
        for (int col = 0; col < w; ++col) {
            const int off = col * 4;
            rgbData.append(static_cast<char>(line[off + 0]));   // R
            rgbData.append(static_cast<char>(line[off + 1]));   // G
            rgbData.append(static_cast<char>(line[off + 2]));   // B
            alphaData.append(static_cast<char>(line[off + 3])); // A
        }
    }

    // Compress both streams (FlateDecode = zlib, same as pikepdf's FlateDecode)
    std::string rgbComp   = flateDecode(rgbData);
    std::string alphaComp = flateDecode(alphaData);

    // Build SMask (soft-mask) stream — grayscale alpha channel
    auto smask = QPDFObjectHandle::newStream(&pdf, alphaComp);
    {
        auto sd = smask.getDict();
        sd.replaceKey("/Type",             QPDFObjectHandle::newName("/XObject"));
        sd.replaceKey("/Subtype",          QPDFObjectHandle::newName("/Image"));
        sd.replaceKey("/Width",            QPDFObjectHandle::newInteger(w));
        sd.replaceKey("/Height",           QPDFObjectHandle::newInteger(h));
        sd.replaceKey("/ColorSpace",       QPDFObjectHandle::newName("/DeviceGray"));
        sd.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        sd.replaceKey("/Filter",           QPDFObjectHandle::newName("/FlateDecode"));
    }
    auto smaskRef = pdf.makeIndirectObject(smask);

    // Build Image XObject — RGB with /SMask reference for transparency
    auto imgStream = QPDFObjectHandle::newStream(&pdf, rgbComp);
    {
        auto d = imgStream.getDict();
        d.replaceKey("/Type",             QPDFObjectHandle::newName("/XObject"));
        d.replaceKey("/Subtype",          QPDFObjectHandle::newName("/Image"));
        d.replaceKey("/Width",            QPDFObjectHandle::newInteger(w));
        d.replaceKey("/Height",           QPDFObjectHandle::newInteger(h));
        d.replaceKey("/ColorSpace",       QPDFObjectHandle::newName("/DeviceRGB"));
        d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        d.replaceKey("/Filter",           QPDFObjectHandle::newName("/FlateDecode"));
        d.replaceKey("/SMask",            smaskRef);   // ← this is the key
    }
    auto indImg = pdf.makeIndirectObject(imgStream);

    static int sigCount = 0;
    std::string ns = "PMSI" + std::to_string(++sigCount);

    auto pageObj = ps[pageIdx].getObjectHandle();

    if (!pageObj.hasKey("/Resources"))
        pageObj.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
    auto res = pageObj.getKey("/Resources");
    if (!res.hasKey("/XObject"))
        res.replaceKey("/XObject", QPDFObjectHandle::newDictionary());
    res.getKey("/XObject").replaceKey("/" + ns, indImg);

    std::ostringstream op;
    op << "\nq " << sigW << " 0 0 " << sigH << " " << x << " " << y
       << " cm /" << ns << " Do Q\n";

    std::string existingData = streamToStr(pageObj.getKey("/Contents"));
    existingData += op.str();
    pageObj.replaceKey("/Contents",
                       QPDFObjectHandle::newStream(&pdf, existingData));
}

// Convenience: place at bottom-right with margin
void overlayImageOnPage(QPDF& pdf, int pageIdx, const QImage& img,
                        double sigW, double sigH, double margin)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;
    auto pageObj = ps[pageIdx].getObjectHandle();
    auto mb = pageObj.getKey("/MediaBox");
    double pw = mb.getArrayItem(2).getNumericValue();
    overlayImageOnPage(pdf, pageIdx, img, sigW, sigH, pw - sigW - margin, margin);
}

// ── Text injection ────────────────────────────────────────────────────────────

// Escape a Latin-1 string for use inside a PDF literal string ( ... ).
static std::string escapePdfLiteral(const QString& text) {
    std::string out;
    out.reserve(static_cast<size_t>(text.size() + 8));
    for (QChar qc : text) {
        const unsigned int u = qc.unicode();
        if (u > 255) {
            out += '?';   // non-Latin-1: substitute; UTF-16BE support is a follow-up
            continue;
        }
        const char c = static_cast<char>(u & 0xFF);
        switch (c) {
        case '(':  out += "\\("; break;
        case ')':  out += "\\)"; break;
        case '\\': out += "\\\\"; break;
        default:   out += c; break;
        }
    }
    return out;
}

// Ensure the page has a /Resources /Font entry for the given standard Type1 font.
// Returns the resource key (e.g. "/Helvetica-Bold") to use in content stream.
static std::string ensureFontResource(QPDF& pdf,
                                       QPDFObjectHandle& pageObj,
                                       const std::string& baseFont)
{
    if (!pageObj.hasKey("/Resources"))
        pageObj.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
    auto res = pageObj.getKey("/Resources");
    if (!res.hasKey("/Font"))
        res.replaceKey("/Font", QPDFObjectHandle::newDictionary());
    auto fontDict = res.getKey("/Font");

    // Use the font name itself as the resource key to keep it idempotent.
    // Strip hyphens so e.g. "Helvetica-Bold" → key "/HelvB" to stay safe with
    // PDF name syntax (hyphens are valid but let's be explicit).
    // Actually hyphens are perfectly fine in PDF names, so use the name directly.
    std::string key = "/" + baseFont;

    if (!fontDict.hasKey(key)) {
        auto fontObj = QPDFObjectHandle::newDictionary();
        fontObj.replaceKey("/Type",     QPDFObjectHandle::newName("/Font"));
        fontObj.replaceKey("/Subtype",  QPDFObjectHandle::newName("/Type1"));
        fontObj.replaceKey("/BaseFont", QPDFObjectHandle::newName("/" + baseFont));
        fontDict.replaceKey(key, pdf.makeIndirectObject(fontObj));
    }
    return key;
}

void injectText(QPDF& pdf, int pageIdx,
                double x, double y,
                const QString& text,
                const QString& pdfFontName,
                double fontSize,
                QColor color)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= static_cast<int>(ps.size())) return;

    auto pageObj  = ps[pageIdx].getObjectHandle();
    const std::string baseFont = pdfFontName.toStdString();
    const std::string fontKey  = ensureFontResource(pdf, pageObj, baseFont);

    // Build the content stream addition.
    std::ostringstream cs;
    cs << "\nq\n";

    // Non-stroking colour (rg = DeviceRGB)
    cs << (color.redF())   << " "
       << (color.greenF()) << " "
       << (color.blueF())  << " rg\n";

    cs << "BT\n";
    cs << fontKey << " " << fontSize << " Tf\n";
    // Leading: 1.2× font size (single-space)
    cs << (fontSize * 1.2) << " TL\n";

    // Split on newlines; each line rendered with T* (move to next line)
    const QStringList lines = text.split('\n');
    bool first = true;
    for (const QString& line : lines) {
        if (first) {
            cs << x << " " << y << " Td\n";
            first = false;
        } else {
            cs << "T*\n";
        }
        cs << "(" << escapePdfLiteral(line) << ") Tj\n";
    }

    cs << "ET\n";
    cs << "Q\n";

    std::string existing = streamToStr(pageObj.getKey("/Contents"));
    existing += cs.str();
    pageObj.replaceKey("/Contents", QPDFObjectHandle::newStream(&pdf, existing));
}

} // namespace Operations

#include "annotations.h"
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <cstdio>

namespace Annotations {

static QPDFObjectHandle makeRect(double x0,double y0,double x1,double y1) {
    return QPDFObjectHandle::newArray({
        QPDFObjectHandle::newReal(x0), QPDFObjectHandle::newReal(y0),
        QPDFObjectHandle::newReal(x1), QPDFObjectHandle::newReal(y1)
    });
}

static QPDFObjectHandle makeColor(QColor c) {
    return QPDFObjectHandle::newArray({
        QPDFObjectHandle::newReal(c.redF()),
        QPDFObjectHandle::newReal(c.greenF()),
        QPDFObjectHandle::newReal(c.blueF())
    });
}

static void ensureAnnots(QPDFObjectHandle& page) {
    if (!page.hasKey("/Annots"))
        page.replaceKey("/Annots", QPDFObjectHandle::newArray());
}

static void appendAnnot(QPDF& pdf, int pageIdx, QPDFObjectHandle annot) {
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    if (pageIdx < 0 || pageIdx >= (int)pages.size()) return;
    auto pageObj = pages[pageIdx].getObjectHandle();
    ensureAnnots(pageObj);
    pageObj.getKey("/Annots").appendItem(pdf.makeIndirectObject(annot));
}

void addHighlight(QPDF& pdf, int pageIdx,
                  const QVector<Quad>& quads,
                  QColor color, double opacity)
{
    QPDFObjectHandle qpArr = QPDFObjectHandle::newArray();
    for (const auto& q : quads) {
        for (double v : {q.x0,q.y0, q.x1,q.y1, q.x2,q.y2, q.x3,q.y3})
            qpArr.appendItem(QPDFObjectHandle::newReal(v));
    }

    double x0=quads[0].x0, y0=quads[0].y0;
    double x1=quads[0].x1, y1=quads[0].y3;

    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",       QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype",    QPDFObjectHandle::newName("/Highlight"));
    annot.replaceKey("/Rect",       makeRect(x0,y0,x1,y1));
    annot.replaceKey("/QuadPoints", qpArr);
    annot.replaceKey("/C",          makeColor(color));
    annot.replaceKey("/CA",         QPDFObjectHandle::newReal(opacity));
    annot.replaceKey("/F",          QPDFObjectHandle::newInteger(4));
    appendAnnot(pdf, pageIdx, annot);
}

void addTextNote(QPDF& pdf, int pageIdx,
                 double x, double y,
                 const QString& contents, QColor color)
{
    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",     QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype",  QPDFObjectHandle::newName("/Text"));
    annot.replaceKey("/Rect",     makeRect(x, y, x+24, y+24));
    annot.replaceKey("/Contents", QPDFObjectHandle::newUnicodeString(contents.toStdString()));
    annot.replaceKey("/C",        makeColor(color));
    annot.replaceKey("/F",        QPDFObjectHandle::newInteger(4));
    annot.replaceKey("/Open",     QPDFObjectHandle::newBool(false));
    appendAnnot(pdf, pageIdx, annot);
}

void addInk(QPDF& pdf, int pageIdx,
            const QVector<QVector<QPointF>>& strokes,
            QColor color, double width)
{
    double x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
    QPDFObjectHandle inkList = QPDFObjectHandle::newArray();
    for (const auto& stroke : strokes) {
        QPDFObjectHandle pts = QPDFObjectHandle::newArray();
        for (const auto& p : stroke) {
            pts.appendItem(QPDFObjectHandle::newReal(p.x()));
            pts.appendItem(QPDFObjectHandle::newReal(p.y()));
            x0=std::min(x0,p.x()); y0=std::min(y0,p.y());
            x1=std::max(x1,p.x()); y1=std::max(y1,p.y());
        }
        inkList.appendItem(pts);
    }

    auto bs = QPDFObjectHandle::newDictionary();
    bs.replaceKey("/W", QPDFObjectHandle::newReal(width));

    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",    QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Ink"));
    annot.replaceKey("/Rect",    makeRect(x0-2,y0-2,x1+2,y1+2));
    annot.replaceKey("/InkList", inkList);
    annot.replaceKey("/C",       makeColor(color));
    annot.replaceKey("/BS",      bs);
    annot.replaceKey("/F",       QPDFObjectHandle::newInteger(4));
    appendAnnot(pdf, pageIdx, annot);
}

// Build a minimal Form XObject appearance stream for a rubber stamp.
// The appearance draws a coloured oval border + the stamp name in red
// text, with a transparent (no-fill) background so underlying page text
// is never hidden.  Without /AP, viewers (Poppler included) synthesise
// their own appearance — typically a solid white-filled box that erases
// all text beneath the stamp rect.
// Locale-safe helper: format a double as "n.nnn" regardless of C locale.
// Qt's QCoreApplication sets the C locale to the system locale on startup,
// which means std::ostringstream and printf use commas in some locales.
// PDF requires dots.  snprintf with a known format is NOT locale-independent
// on all platforms, so we format the integer and fractional parts ourselves.
static std::string d2s(double v)
{
    // Six decimal places, no locale dependence.
    char buf[64];
    int n = std::snprintf(buf, sizeof(buf), "%.6g", v);
    if (n <= 0 || n >= (int)sizeof(buf)) return "0";
    // Replace any comma (European locale) with dot.
    for (int i = 0; i < n; ++i) if (buf[i] == ',') buf[i] = '.';
    return std::string(buf, n);
}

static QPDFObjectHandle makeStampAP(QPDF& pdf,
                                     double w, double h,
                                     const std::string& label)
{
    const double rx = w / 2.0, ry = h / 2.0;
    const double cx = rx, cy = ry;
    const double kx = rx * 0.5523, ky = ry * 0.5523;
    const double fs = h * 0.40;

    // Build content stream using d2s() so floats always use '.' as separator.
    std::string cs;
    cs += "q\n";
    cs += "1 0 0 RG\n";   // red stroke, transparent background
    cs += "1.5 w\n";
    // Ellipse via four Bézier curves
    cs += d2s(cx)       + " " + d2s(cy + ry) + " m\n";
    cs += d2s(cx + kx)  + " " + d2s(cy + ry) + " " +
          d2s(cx + rx)  + " " + d2s(cy + ky) + " " +
          d2s(cx + rx)  + " " + d2s(cy)      + " c\n";
    cs += d2s(cx + rx)  + " " + d2s(cy - ky) + " " +
          d2s(cx + kx)  + " " + d2s(cy - ry) + " " +
          d2s(cx)       + " " + d2s(cy - ry) + " c\n";
    cs += d2s(cx - kx)  + " " + d2s(cy - ry) + " " +
          d2s(cx - rx)  + " " + d2s(cy - ky) + " " +
          d2s(cx - rx)  + " " + d2s(cy)      + " c\n";
    cs += d2s(cx - rx)  + " " + d2s(cy + ky) + " " +
          d2s(cx - kx)  + " " + d2s(cy + ry) + " " +
          d2s(cx)       + " " + d2s(cy + ry) + " c\n";
    cs += "S\n";           // stroke only — no fill

    const double charW = fs * 0.6;
    const double textW = charW * static_cast<double>(label.size());
    const double tx    = (w - textW) / 2.0;
    const double ty    = cy - fs * 0.35;

    cs += "BT\n";
    cs += "/Helvetica-Bold " + d2s(fs) + " Tf\n";
    cs += "1 0 0 rg\n";
    cs += d2s(tx) + " " + d2s(ty) + " Td\n";
    cs += "(" + label + ") Tj\n";
    cs += "ET\n";
    cs += "Q\n";

    auto apStream = QPDFObjectHandle::newStream(&pdf, cs);
    auto d = apStream.getDict();
    d.replaceKey("/Type",    QPDFObjectHandle::newName("/XObject"));
    d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    d.replaceKey("/FormType",QPDFObjectHandle::newInteger(1));
    d.replaceKey("/BBox",    makeRect(0, 0, w, h));

    // Add Helvetica-Bold to the form's /Resources so the text operator works
    auto fontObj = QPDFObjectHandle::newDictionary();
    fontObj.replaceKey("/Type",     QPDFObjectHandle::newName("/Font"));
    fontObj.replaceKey("/Subtype",  QPDFObjectHandle::newName("/Type1"));
    fontObj.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica-Bold"));
    auto fontDict = QPDFObjectHandle::newDictionary();
    fontDict.replaceKey("/Helvetica-Bold", pdf.makeIndirectObject(fontObj));
    auto res = QPDFObjectHandle::newDictionary();
    res.replaceKey("/Font", fontDict);
    d.replaceKey("/Resources", res);

    return apStream;
}

void addStamp(QPDF& pdf, int pageIdx,
              double x, double y, double w, double h,
              const QString& name)
{
    auto apStream = pdf.makeIndirectObject(
        makeStampAP(pdf, w, h, name.toUpper().toStdString()));

    auto apDict = QPDFObjectHandle::newDictionary();
    apDict.replaceKey("/N", apStream);   // /N = normal appearance

    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",    QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Stamp"));
    annot.replaceKey("/Rect",    makeRect(x, y, x+w, y+h));
    annot.replaceKey("/Name",    QPDFObjectHandle::newName("/" + name.toStdString()));
    annot.replaceKey("/F",       QPDFObjectHandle::newInteger(4));
    annot.replaceKey("/AP",      apDict);
    appendAnnot(pdf, pageIdx, annot);
}

void addRedact(QPDF& pdf, int pageIdx,
               double x0, double y0, double x1, double y1)
{
    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",        QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype",     QPDFObjectHandle::newName("/Redact"));
    annot.replaceKey("/Rect",        makeRect(x0,y0,x1,y1));
    annot.replaceKey("/IC",          makeColor(Qt::black));
    annot.replaceKey("/OverlayText", QPDFObjectHandle::newUnicodeString(""));
    annot.replaceKey("/F",           QPDFObjectHandle::newInteger(4));
    appendAnnot(pdf, pageIdx, annot);
}

void applyRedactions(QPDF& pdf, int pageIdx) {
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    if (pageIdx < 0 || pageIdx >= (int)pages.size()) return;
    auto pageObj = pages[pageIdx].getObjectHandle();
    if (!pageObj.hasKey("/Annots")) return;

    auto annots = pageObj.getKey("/Annots");
    QPDFObjectHandle surviving = QPDFObjectHandle::newArray();
    for (int i = 0; i < annots.getArrayNItems(); ++i) {
        auto a = annots.getArrayItem(i);
        if (a.getKey("/Subtype").getName() == "/Redact") continue;
        surviving.appendItem(a);
    }
    pageObj.replaceKey("/Annots", surviving);
}

} // namespace Annotations

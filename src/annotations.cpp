#include "annotations.h"
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

namespace Annotations {

// newReal(double) defaults to decimal_places=0 which rounds to integer.
// Use 6 decimal places so coordinates and colours survive the roundtrip intact.
static constexpr int kRealPrecision = 6;

static QPDFObjectHandle makeRect(double x0,double y0,double x1,double y1) {
    return QPDFObjectHandle::newArray({
        QPDFObjectHandle::newReal(x0, kRealPrecision),
        QPDFObjectHandle::newReal(y0, kRealPrecision),
        QPDFObjectHandle::newReal(x1, kRealPrecision),
        QPDFObjectHandle::newReal(y1, kRealPrecision)
    });
}

static QPDFObjectHandle makeColor(QColor c) {
    return QPDFObjectHandle::newArray({
        QPDFObjectHandle::newReal(c.redF(),   kRealPrecision),
        QPDFObjectHandle::newReal(c.greenF(), kRealPrecision),
        QPDFObjectHandle::newReal(c.blueF(),  kRealPrecision)
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
    annot.replaceKey("/CA",         QPDFObjectHandle::newReal(opacity, kRealPrecision));
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
    bs.replaceKey("/W", QPDFObjectHandle::newReal(width, kRealPrecision));

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

void addStamp(QPDF& pdf, int pageIdx,
              double x, double y, double w, double h,
              const QString& name)
{
    auto annot = QPDFObjectHandle::newDictionary();
    annot.replaceKey("/Type",    QPDFObjectHandle::newName("/Annot"));
    annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Stamp"));
    annot.replaceKey("/Rect",    makeRect(x, y, x+w, y+h));
    annot.replaceKey("/Name",    QPDFObjectHandle::newName("/" + name.toStdString()));
    annot.replaceKey("/F",       QPDFObjectHandle::newInteger(4));
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

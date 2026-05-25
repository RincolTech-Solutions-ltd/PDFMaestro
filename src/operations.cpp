#include "operations.h"
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <QFile>
#include <QBuffer>
#include <cmath>
#include <sstream>

namespace Operations {

static std::vector<QPDFPageObjectHelper> pages(QPDF& pdf) {
    return QPDFPageDocumentHelper(pdf).getAllPages();
}

QByteArray toBytes(QPDF& pdf) {
    QPDFWriter w(pdf);
    w.setOutputMemory();
    w.write();
    auto buf = w.getBuffer();
    return QByteArray(buf->getBuffer(), static_cast<int>(buf->getSize()));
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

static QPDF extractPages(QPDF& src, int first, int last) {
    QPDF out;
    out.emptyPDF();
    QPDFPageDocumentHelper odh(out);
    auto ps = pages(src);
    for (int i = first; i <= last && i < (int)ps.size(); ++i)
        odh.addPage(out.copyForeignObject(ps[i].getObjectHandle()), false);
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
        writeTo(part, path);
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
    auto sigContent = sigPage.getKey("/Contents");
    std::string streamData;
    if (sigContent.isStream())
        streamData = sigContent.getStreamData(qpdf_dl_all)->str();
    else if (sigContent.isArray()) {
        for (int i=0; i<sigContent.getArrayNItems(); ++i)
            streamData += sigContent.getArrayItem(i).getStreamData(qpdf_dl_all)->str();
    }

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

    auto existing = pageObj.getKey("/Contents");
    std::string existingData;
    if (existing.isStream())
        existingData = existing.getStreamData(qpdf_dl_all)->str();
    else if (existing.isArray()) {
        for (int i=0; i<existing.getArrayNItems(); ++i)
            existingData += existing.getArrayItem(i).getStreamData(qpdf_dl_all)->str();
    }
    existingData += op.str();
    pageObj.replaceKey("/Contents",
                       QPDFObjectHandle::newStream(&pdf, existingData));
}

void overlayImageOnPage(QPDF& pdf, int pageIdx, const QImage& img,
                        double sigW, double sigH, double margin)
{
    auto ps = pages(pdf);
    if (pageIdx < 0 || pageIdx >= (int)ps.size()) return;

    // Encode image as JPEG into a QByteArray
    QByteArray imgData;
    QBuffer buf(&imgData);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 90);
    buf.close();
    if (imgData.isEmpty()) return;

    // Build Image XObject
    auto imgStream = QPDFObjectHandle::newStream(&pdf,
        std::string(imgData.constData(), imgData.size()));
    auto d = imgStream.getDict();
    d.replaceKey("/Type",             QPDFObjectHandle::newName("/XObject"));
    d.replaceKey("/Subtype",          QPDFObjectHandle::newName("/Image"));
    d.replaceKey("/Width",            QPDFObjectHandle::newInteger(img.width()));
    d.replaceKey("/Height",           QPDFObjectHandle::newInteger(img.height()));
    d.replaceKey("/ColorSpace",       QPDFObjectHandle::newName("/DeviceRGB"));
    d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    d.replaceKey("/Filter",           QPDFObjectHandle::newName("/DCTDecode"));
    auto indImg = pdf.makeIndirectObject(imgStream);

    static int sigCount = 0;
    std::string ns = "PMSI" + std::to_string(++sigCount);

    auto pageObj = ps[pageIdx].getObjectHandle();
    auto mb = pageObj.getKey("/MediaBox");
    double pw = mb.getArrayItem(2).getNumericValue();
    // Place at bottom-right
    double x = pw - sigW - margin;
    double y = margin;

    if (!pageObj.hasKey("/Resources"))
        pageObj.replaceKey("/Resources", QPDFObjectHandle::newDictionary());
    auto res = pageObj.getKey("/Resources");
    if (!res.hasKey("/XObject"))
        res.replaceKey("/XObject", QPDFObjectHandle::newDictionary());
    res.getKey("/XObject").replaceKey("/" + ns, indImg);

    std::ostringstream op;
    op << "\nq " << sigW << " 0 0 " << sigH << " " << x << " " << y
       << " cm /" << ns << " Do Q\n";

    auto existing = pageObj.getKey("/Contents");
    std::string existingData;
    if (existing.isStream())
        existingData = existing.getStreamData(qpdf_dl_all)->str();
    else if (existing.isArray()) {
        for (int i=0; i<existing.getArrayNItems(); ++i)
            existingData += existing.getArrayItem(i).getStreamData(qpdf_dl_all)->str();
    }
    existingData += op.str();
    pageObj.replaceKey("/Contents",
                       QPDFObjectHandle::newStream(&pdf, existingData));
}

} // namespace Operations

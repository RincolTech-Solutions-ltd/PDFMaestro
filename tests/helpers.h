#pragma once
// Shared test helpers — included by all test translation units (inline to avoid ODR).

#include <memory>
#include <string>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

namespace TestHelpers {

// Build a minimal, valid N-page PDF in memory.
// Each page is letter-size (612×792 pts) with a /MediaBox and a trivial
// /Contents stream that is an indirect object so it survives round-trips.
inline std::shared_ptr<QPDF> makeBlankPdf(int numPages = 3)
{
    auto pdf = std::make_shared<QPDF>();
    pdf->emptyPDF();
    QPDFPageDocumentHelper pdh(*pdf);

    for (int i = 0; i < numPages; ++i) {
        auto page = QPDFObjectHandle::newDictionary();
        page.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));

        QPDFObjectHandle mb = QPDFObjectHandle::newArray();
        mb.appendItem(QPDFObjectHandle::newInteger(0));
        mb.appendItem(QPDFObjectHandle::newInteger(0));
        mb.appendItem(QPDFObjectHandle::newInteger(612));
        mb.appendItem(QPDFObjectHandle::newInteger(792));
        page.replaceKey("/MediaBox", mb);

        // Contents as indirect stream so modifications via replaceKey survive round-trip
        auto contents = QPDFObjectHandle::newStream(pdf.get(), "BT /F1 12 Tf 100 700 Td (Hello World) Tj ET\n");
        page.replaceKey("/Contents", pdf->makeIndirectObject(contents));
        page.replaceKey("/Resources", QPDFObjectHandle::newDictionary());

        pdh.addPage(pdf->makeIndirectObject(page), false);
    }
    return pdf;
}

// Serialize src to bytes, copy into an owned std::string, then reload.
// processMemoryFile does NOT copy the buffer — it keeps a pointer into the
// memory for lazy stream reads.  We copy into a string and capture it in a
// shared_ptr<string> that the QPDF deleter keeps alive.
inline std::shared_ptr<QPDF> roundTrip(QPDF& src)
{
    QPDFWriter w(src);
    w.setOutputMemory();
    w.write();
    auto rawBuf = w.getBuffer();   // raw ptr, owned by w
    auto owned = std::make_shared<std::string>(
        reinterpret_cast<const char*>(rawBuf->getBuffer()),
        rawBuf->getSize());
    // w (and its buffer) destroyed here; we use owned from now on.

    auto reloaded = std::shared_ptr<QPDF>(
        new QPDF(),
        [owned](QPDF* p) { delete p; });   // owned stays alive until QPDF deleted
    reloaded->processMemoryFile(
        "roundtrip",
        owned->data(),
        owned->size(),
        nullptr);
    return reloaded;
}

// Count /Annots entries on a page; 0 if absent, -1 if pageIdx out of range.
inline int annotCount(QPDF& pdf, int pageIdx)
{
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    if (pageIdx < 0 || pageIdx >= static_cast<int>(pages.size())) return -1;
    auto pageObj = pages[pageIdx].getObjectHandle();
    if (!pageObj.hasKey("/Annots")) return 0;
    return pageObj.getKey("/Annots").getArrayNItems();
}

// Return the Nth annotation on a page (0-based).
inline QPDFObjectHandle getAnnot(QPDF& pdf, int pageIdx, int annotIdx)
{
    auto pages   = QPDFPageDocumentHelper(pdf).getAllPages();
    auto pageObj = pages[pageIdx].getObjectHandle();
    return pageObj.getKey("/Annots").getArrayItem(annotIdx);
}

// Read the page's /Contents as a single concatenated string.
inline std::string getContentStream(QPDF& pdf, int pageIdx)
{
    auto pages   = QPDFPageDocumentHelper(pdf).getAllPages();
    auto pageObj = pages[pageIdx].getObjectHandle();
    auto c = pageObj.getKey("/Contents");
    if (c.isStream()) {
        auto data = c.getStreamData(qpdf_dl_all);
        return std::string(reinterpret_cast<const char*>(data->getBuffer()),
                           data->getSize());
    }
    if (c.isArray()) {
        std::string out;
        for (int i = 0; i < c.getArrayNItems(); ++i) {
            auto s = c.getArrayItem(i).getStreamData(qpdf_dl_all);
            out += std::string(reinterpret_cast<const char*>(s->getBuffer()), s->getSize());
        }
        return out;
    }
    return {};
}

// Number of pages in the PDF.
inline int pageCount(QPDF& pdf)
{
    return static_cast<int>(QPDFPageDocumentHelper(pdf).getAllPages().size());
}

} // namespace TestHelpers

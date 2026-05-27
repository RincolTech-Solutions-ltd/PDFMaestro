#pragma once
// Shared test helpers — included by all three test translation units.
// All functions are inline to avoid ODR issues when the header is included
// in multiple executables compiled in the same CTest run.

#include <memory>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

namespace TestHelpers {

// Build a minimal, valid N-page PDF in memory.
// Each page is letter-size (612×792 pts), has a /MediaBox and a trivial
// /Contents stream that is properly stored as an indirect object.
inline std::shared_ptr<QPDF> makeBlankPdf(int numPages = 3)
{
    auto pdf = std::make_shared<QPDF>();
    pdf->emptyPDF();
    QPDFPageDocumentHelper pdh(*pdf);

    for (int i = 0; i < numPages; ++i) {
        auto page = QPDFObjectHandle::newDictionary();
        page.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));

        // MediaBox — letter size
        QPDFObjectHandle mb = QPDFObjectHandle::newArray();
        mb.appendItem(QPDFObjectHandle::newInteger(0));
        mb.appendItem(QPDFObjectHandle::newInteger(0));
        mb.appendItem(QPDFObjectHandle::newInteger(612));
        mb.appendItem(QPDFObjectHandle::newInteger(792));
        page.replaceKey("/MediaBox", mb);

        // Contents must be an indirect stream to be spec-valid
        auto contents = QPDFObjectHandle::newStream(pdf.get(), "q Q\n");
        page.replaceKey("/Contents", pdf->makeIndirectObject(contents));

        page.replaceKey("/Resources", QPDFObjectHandle::newDictionary());

        pdh.addPage(pdf->makeIndirectObject(page), false);
    }
    return pdf;
}

// Serialize src to bytes and reload into a fresh QPDF — identical to the
// in-memory pipeline the real app uses after every mutation.
inline std::shared_ptr<QPDF> roundTrip(QPDF& src)
{
    QPDFWriter w(src);
    w.setOutputMemory();
    w.write();
    auto buf = w.getBufferSharedPointer();

    auto reloaded = std::make_shared<QPDF>();
    reloaded->processMemoryFile(
        "roundtrip",
        reinterpret_cast<const char*>(buf->getBuffer()),
        buf->getSize(),
        nullptr);
    return reloaded;
}

// Count items in page's /Annots array; returns 0 if absent, -1 if out of bounds.
inline int annotCount(QPDF& pdf, int pageIdx)
{
    auto pages = QPDFPageDocumentHelper(pdf).getAllPages();
    if (pageIdx < 0 || pageIdx >= static_cast<int>(pages.size())) return -1;
    auto pageObj = pages[pageIdx].getObjectHandle();
    if (!pageObj.hasKey("/Annots")) return 0;
    return pageObj.getKey("/Annots").getArrayNItems();
}

// Return the Nth annotation dictionary on a page (0-based).
// QPDF auto-dereferences indirect handles when dict/array methods are called,
// so returning the handle directly is safe even if it is an indirect reference.
inline QPDFObjectHandle getAnnot(QPDF& pdf, int pageIdx, int annotIdx)
{
    auto pages   = QPDFPageDocumentHelper(pdf).getAllPages();
    auto pageObj = pages[pageIdx].getObjectHandle();
    return pageObj.getKey("/Annots").getArrayItem(annotIdx);
}

// Count pages in a PDF.
inline int pageCount(QPDF& pdf)
{
    return static_cast<int>(QPDFPageDocumentHelper(pdf).getAllPages().size());
}

// Get page N's /Rotate value (default 0 if key absent).
inline int getRotate(QPDF& pdf, int pageIdx)
{
    auto pages  = QPDFPageDocumentHelper(pdf).getAllPages();
    auto obj    = pages[pageIdx].getObjectHandle();
    if (!obj.hasKey("/Rotate")) return 0;
    return obj.getKey("/Rotate").getIntValueAsInt();
}

} // namespace TestHelpers

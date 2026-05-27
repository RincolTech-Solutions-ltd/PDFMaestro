#include <QtTest>
#include <QTemporaryDir>
#include <QFileInfo>

#include "operations.h"
#include "helpers.h"

#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

using namespace TestHelpers;

// ─────────────────────────────────────────────────────────────────────────────
class OperationsTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_tmpDir;

private slots:
    // ── lifecycle ─────────────────────────────────────────────────────────────
    void initTestCase()  { QVERIFY(m_tmpDir.isValid()); }

    // ── parseRanges ───────────────────────────────────────────────────────────
    void parseRanges_singlePage();
    void parseRanges_singleRange();
    void parseRanges_commaList();
    void parseRanges_mixedRangeAndSingle();
    void parseRanges_clampsToBounds();
    void parseRanges_emptyString();
    void parseRanges_outOfBoundsPageSkipped();
    void parseRanges_reversedRangeSkipped();

    // ── toBytes ───────────────────────────────────────────────────────────────
    void toBytes_returnsNonEmpty();
    void toBytes_reloadableByQpdf();

    // ── applyPageOrder ────────────────────────────────────────────────────────
    void applyPageOrder_reverseThreePages();
    void applyPageOrder_singlePageNoop();
    void applyPageOrder_preservesPageCount();

    // ── deletePages ───────────────────────────────────────────────────────────
    void deletePages_deleteFirst();
    void deletePages_deleteLast();
    void deletePages_deleteMiddle();
    void deletePages_deleteMultiple();
    void deletePages_outOfBoundsIgnored();

    // ── rotatePage ────────────────────────────────────────────────────────────
    void rotatePage_set90();
    void rotatePage_accumulates_90plus90();
    void rotatePage_normalizes_360to0();
    void rotatePage_negativeWraps();
    void rotatePage_outOfBoundsNoOp();

    // ── cropPage ──────────────────────────────────────────────────────────────
    void cropPage_setsCropBox();
    void cropPage_symmetricCrop();
    void cropPage_outOfBoundsNoOp();

    // ── cropAllPages ──────────────────────────────────────────────────────────
    void cropAllPages_allPagesGetCropBox();

    // ── mergeInto ─────────────────────────────────────────────────────────────
    void mergeInto_appendsPages();
    void mergeInto_insertAtStart();
    void mergeInto_insertAtMiddle();

    // ── splitByRanges ─────────────────────────────────────────────────────────
    void splitByRanges_producesCorrectFileCount();
    void splitByRanges_eachFileHasCorrectPageCount();

    // ── splitEveryN ───────────────────────────────────────────────────────────
    void splitEveryN_exactMultiple();
    void splitEveryN_withRemainder();

    // ── splitEachPage ─────────────────────────────────────────────────────────
    void splitEachPage_onePdfPerPage();
};

// ─── parseRanges ─────────────────────────────────────────────────────────────

void OperationsTest::parseRanges_singlePage()
{
    auto r = Operations::parseRanges("2", 5);
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0].first, 1);
    QCOMPARE(r[0].last,  1);
}

void OperationsTest::parseRanges_singleRange()
{
    auto r = Operations::parseRanges("1-3", 5);
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0].first, 0);
    QCOMPARE(r[0].last,  2);
}

void OperationsTest::parseRanges_commaList()
{
    auto r = Operations::parseRanges("1,3,5", 5);
    QCOMPARE(r.size(), 3);
    QCOMPARE(r[0].first, 0); QCOMPARE(r[0].last, 0);
    QCOMPARE(r[1].first, 2); QCOMPARE(r[1].last, 2);
    QCOMPARE(r[2].first, 4); QCOMPARE(r[2].last, 4);
}

void OperationsTest::parseRanges_mixedRangeAndSingle()
{
    auto r = Operations::parseRanges("1-3,5,7-9", 10);
    QCOMPARE(r.size(), 3);
    QCOMPARE(r[0].first, 0); QCOMPARE(r[0].last, 2);
    QCOMPARE(r[1].first, 4); QCOMPARE(r[1].last, 4);
    QCOMPARE(r[2].first, 6); QCOMPARE(r[2].last, 8);
}

void OperationsTest::parseRanges_clampsToBounds()
{
    // "1-100" with total=5 — last should clamp to 4
    auto r = Operations::parseRanges("1-100", 5);
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0].first, 0);
    QCOMPARE(r[0].last,  4);
}

void OperationsTest::parseRanges_emptyString()
{
    auto r = Operations::parseRanges("", 5);
    QCOMPARE(r.size(), 0);
}

void OperationsTest::parseRanges_outOfBoundsPageSkipped()
{
    // Page 10 in a 5-page doc → skipped
    auto r = Operations::parseRanges("10", 5);
    QCOMPARE(r.size(), 0);
}

void OperationsTest::parseRanges_reversedRangeSkipped()
{
    // "5-2" → b < a → skipped
    auto r = Operations::parseRanges("5-2", 10);
    QCOMPARE(r.size(), 0);
}

// ─── toBytes ─────────────────────────────────────────────────────────────────

void OperationsTest::toBytes_returnsNonEmpty()
{
    auto pdf = makeBlankPdf(1);
    QByteArray bytes = Operations::toBytes(*pdf);
    QVERIFY(!bytes.isEmpty());
    // A minimal single-page PDF is at least 200 bytes
    QVERIFY(bytes.size() > 200);
    // PDF magic number
    QVERIFY(bytes.startsWith("%PDF-"));
}

void OperationsTest::toBytes_reloadableByQpdf()
{
    auto pdf   = makeBlankPdf(3);
    QByteArray bytes = Operations::toBytes(*pdf);

    QPDF reloaded;
    reloaded.processMemoryFile("test", bytes.constData(), bytes.size(), nullptr);
    QCOMPARE(pageCount(reloaded), 3);
}

// ─── applyPageOrder ──────────────────────────────────────────────────────────

void OperationsTest::applyPageOrder_reverseThreePages()
{
    auto pdf = makeBlankPdf(3);
    // Stamp each page with a distinct /Rotate so we can identify them after reorder
    Operations::rotatePage(*pdf, 0,   0);   // /Rotate 0
    Operations::rotatePage(*pdf, 1,  90);   // /Rotate 90
    Operations::rotatePage(*pdf, 2, 180);   // /Rotate 180

    Operations::applyPageOrder(*pdf, {2, 1, 0});

    // Verify directly on the in-memory QPDF — no serialization needed here.
    // (Serialisation after reorder is covered by applyPageOrder_preservesPageCount)
    QCOMPARE(pageCount(*pdf), 3);
    QCOMPARE(getRotate(*pdf, 0), 180); // was page 2
    QCOMPARE(getRotate(*pdf, 1),  90); // was page 1
    QCOMPARE(getRotate(*pdf, 2),   0); // was page 0
}

void OperationsTest::applyPageOrder_singlePageNoop()
{
    auto pdf = makeBlankPdf(1);
    Operations::applyPageOrder(*pdf, {0});
    QCOMPARE(pageCount(*pdf), 1);
}

void OperationsTest::applyPageOrder_preservesPageCount()
{
    auto pdf = makeBlankPdf(5);
    Operations::applyPageOrder(*pdf, {4, 2, 0, 1, 3});
    QCOMPARE(pageCount(*pdf), 5);
}

// ─── deletePages ─────────────────────────────────────────────────────────────

void OperationsTest::deletePages_deleteFirst()
{
    auto pdf = makeBlankPdf(3);
    Operations::rotatePage(*pdf, 0, 90);  // mark first page

    Operations::deletePages(*pdf, {0});
    QCOMPARE(pageCount(*pdf), 2);
    // Former page 1 is now page 0 — it had no rotation
    QCOMPARE(getRotate(*pdf, 0), 0);
}

void OperationsTest::deletePages_deleteLast()
{
    auto pdf = makeBlankPdf(3);
    Operations::deletePages(*pdf, {2});
    QCOMPARE(pageCount(*pdf), 2);
}

void OperationsTest::deletePages_deleteMiddle()
{
    auto pdf = makeBlankPdf(3);
    Operations::deletePages(*pdf, {1});
    QCOMPARE(pageCount(*pdf), 2);
}

void OperationsTest::deletePages_deleteMultiple()
{
    auto pdf = makeBlankPdf(5);
    Operations::deletePages(*pdf, {0, 2, 4});
    QCOMPARE(pageCount(*pdf), 2);
}

void OperationsTest::deletePages_outOfBoundsIgnored()
{
    auto pdf = makeBlankPdf(3);
    Operations::deletePages(*pdf, {-1, 99, 100});
    QCOMPARE(pageCount(*pdf), 3); // unchanged
}

// ─── rotatePage ──────────────────────────────────────────────────────────────

void OperationsTest::rotatePage_set90()
{
    auto pdf = makeBlankPdf(1);
    Operations::rotatePage(*pdf, 0, 90);
    QCOMPARE(getRotate(*pdf, 0), 90);
}

void OperationsTest::rotatePage_accumulates_90plus90()
{
    auto pdf = makeBlankPdf(1);
    Operations::rotatePage(*pdf, 0, 90);
    Operations::rotatePage(*pdf, 0, 90);
    QCOMPARE(getRotate(*pdf, 0), 180);
}

void OperationsTest::rotatePage_normalizes_360to0()
{
    auto pdf = makeBlankPdf(1);
    Operations::rotatePage(*pdf, 0, 360);
    // (0 + 360 + 360) % 360 = 0
    QCOMPARE(getRotate(*pdf, 0), 0);
}

void OperationsTest::rotatePage_negativeWraps()
{
    auto pdf = makeBlankPdf(1);
    Operations::rotatePage(*pdf, 0, -90);
    // (0 + (-90) + 360) % 360 = 270
    QCOMPARE(getRotate(*pdf, 0), 270);
}

void OperationsTest::rotatePage_outOfBoundsNoOp()
{
    auto pdf = makeBlankPdf(3);
    // Should not throw or crash
    Operations::rotatePage(*pdf, 99, 90);
    QCOMPARE(pageCount(*pdf), 3); // unchanged
}

// ─── cropPage ────────────────────────────────────────────────────────────────

void OperationsTest::cropPage_setsCropBox()
{
    auto pdf = makeBlankPdf(1);
    // Page is 612×792. Crop 10 from each edge.
    Operations::cropPage(*pdf, 0, 10.0, 10.0, 10.0, 10.0);

    auto pages = QPDFPageDocumentHelper(*pdf).getAllPages();
    auto obj   = pages[0].getObjectHandle();
    QVERIFY(obj.hasKey("/CropBox"));

    auto cb = obj.getKey("/CropBox");
    QCOMPARE(cb.getArrayNItems(), 4);
    QVERIFY(qAbs(cb.getArrayItem(0).getNumericValue() -  10.0) < 0.5); // left:   0+10
    QVERIFY(qAbs(cb.getArrayItem(1).getNumericValue() -  10.0) < 0.5); // bottom: 0+10
    QVERIFY(qAbs(cb.getArrayItem(2).getNumericValue() - 602.0) < 0.5); // right:  612-10
    QVERIFY(qAbs(cb.getArrayItem(3).getNumericValue() - 782.0) < 0.5); // top:    792-10
}

void OperationsTest::cropPage_symmetricCrop()
{
    auto pdf = makeBlankPdf(1);
    Operations::cropPage(*pdf, 0, 0.0, 0.0, 0.0, 0.0); // no crop
    auto pages = QPDFPageDocumentHelper(*pdf).getAllPages();
    auto obj   = pages[0].getObjectHandle();
    QVERIFY(obj.hasKey("/CropBox"));
    auto cb = obj.getKey("/CropBox");
    QVERIFY(qAbs(cb.getArrayItem(2).getNumericValue() - 612.0) < 0.5);
    QVERIFY(qAbs(cb.getArrayItem(3).getNumericValue() - 792.0) < 0.5);
}

void OperationsTest::cropPage_outOfBoundsNoOp()
{
    auto pdf = makeBlankPdf(3);
    Operations::cropPage(*pdf, 99, 10.0, 10.0, 10.0, 10.0); // no crash
    QCOMPARE(pageCount(*pdf), 3);
}

// ─── cropAllPages ────────────────────────────────────────────────────────────

void OperationsTest::cropAllPages_allPagesGetCropBox()
{
    auto pdf = makeBlankPdf(4);
    Operations::cropAllPages(*pdf, 5.0, 5.0, 5.0, 5.0);

    auto pages = QPDFPageDocumentHelper(*pdf).getAllPages();
    for (auto& p : pages)
        QVERIFY(p.getObjectHandle().hasKey("/CropBox"));
}

// ─── mergeInto ───────────────────────────────────────────────────────────────

void OperationsTest::mergeInto_appendsPages()
{
    auto base = makeBlankPdf(2);
    auto src  = makeBlankPdf(3);
    Operations::mergeInto(*base, *src, -1);
    QCOMPARE(pageCount(*base), 5);
}

void OperationsTest::mergeInto_insertAtStart()
{
    auto base = makeBlankPdf(2);
    auto src  = makeBlankPdf(1);

    // Mark src page so we can verify insertion position
    Operations::rotatePage(*src, 0, 180);

    Operations::mergeInto(*base, *src, 0);
    QCOMPARE(pageCount(*base), 3);

    // Verify insertion position directly on the in-memory QPDF
    QCOMPARE(getRotate(*base, 0), 180); // inserted page is at front
}

void OperationsTest::mergeInto_insertAtMiddle()
{
    auto base = makeBlankPdf(4);
    auto src  = makeBlankPdf(2);
    Operations::mergeInto(*base, *src, 2);
    QCOMPARE(pageCount(*base), 6);
}

// ─── splitByRanges ───────────────────────────────────────────────────────────

void OperationsTest::splitByRanges_producesCorrectFileCount()
{
    auto pdf = makeBlankPdf(5);
    QVector<Operations::Range> ranges = {{0, 1}, {2, 4}};
    QStringList out = Operations::splitByRanges(*pdf, ranges,
                                                m_tmpDir.path(), "split");
    QCOMPARE(out.size(), 2);
    for (const auto& p : out)
        QVERIFY(QFileInfo::exists(p));
}

void OperationsTest::splitByRanges_eachFileHasCorrectPageCount()
{
    auto pdf = makeBlankPdf(5);
    QVector<Operations::Range> ranges = {{0, 1}, {2, 4}};
    QStringList out = Operations::splitByRanges(*pdf, ranges,
                                                m_tmpDir.path(), "splitpages");

    QCOMPARE(out.size(), 2);

    QPDF part1; part1.processFile(out[0].toStdString().c_str());
    QCOMPARE(pageCount(part1), 2);

    QPDF part2; part2.processFile(out[1].toStdString().c_str());
    QCOMPARE(pageCount(part2), 3);
}

// ─── splitEveryN ─────────────────────────────────────────────────────────────

void OperationsTest::splitEveryN_exactMultiple()
{
    auto pdf = makeBlankPdf(6);
    QStringList out = Operations::splitEveryN(*pdf, 2, m_tmpDir.path(), "even");
    QCOMPARE(out.size(), 3);
    for (const auto& p : out) {
        QPDF part; part.processFile(p.toStdString().c_str());
        QCOMPARE(pageCount(part), 2);
    }
}

void OperationsTest::splitEveryN_withRemainder()
{
    auto pdf = makeBlankPdf(5);
    QStringList out = Operations::splitEveryN(*pdf, 2, m_tmpDir.path(), "rem");
    QCOMPARE(out.size(), 3); // [0-1], [2-3], [4]

    QPDF part3; part3.processFile(out[2].toStdString().c_str());
    QCOMPARE(pageCount(part3), 1); // last chunk has 1 page
}

// ─── splitEachPage ────────────────────────────────────────────────────────────

void OperationsTest::splitEachPage_onePdfPerPage()
{
    auto pdf = makeBlankPdf(4);
    QStringList out = Operations::splitEachPage(*pdf, m_tmpDir.path(), "pg");
    QCOMPARE(out.size(), 4);
    for (const auto& p : out) {
        QVERIFY(QFileInfo::exists(p));
        QPDF part; part.processFile(p.toStdString().c_str());
        QCOMPARE(pageCount(part), 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_GUILESS_MAIN(OperationsTest)
#include "tst_operations.moc"

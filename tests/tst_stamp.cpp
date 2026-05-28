#include <QtTest>
#include <QColor>

#include "annotations.h"
#include "helpers.h"

using namespace TestHelpers;

// All seven names from the overlay's STAMP_NAMES list
static const QStringList ALL_STAMP_NAMES = {
    "Approved", "Draft", "Confidential", "Final",
    "Void", "ForComment", "NotApproved"
};

class StampTest : public QObject
{
    Q_OBJECT

private slots:
    // ── Basic structure ───────────────────────────────────────────────────────
    void addStamp_approved_addsExactlyOneAnnotation();
    void addStamp_approved_subtypeIsStamp();
    void addStamp_approved_nameIsSlashApproved();
    void addStamp_approved_rectHasFourCoords();
    void addStamp_approved_rectMatchesXYWH();
    void addStamp_approved_hasPrintFlag();

    // ── Appearance stream (regression: text was disappearing) ─────────────────
    void addStamp_approved_hasAppearanceDictionary();
    void addStamp_approved_apNStreamIsPresent();
    void addStamp_approved_apStreamIsNonEmpty();

    // ── Content stream must never be touched by addStamp ──────────────────────
    void addStamp_approved_contentStreamUnchanged();
    void addStamp_approved_contentStreamUnchangedAfterRoundTrip();

    // ── All named stamp variants must not corrupt content ─────────────────────
    void addStamp_allNames_contentStreamPreserved();
    void addStamp_allNames_eachAddsOneAnnot();
    void addStamp_allNames_subtypeAlwaysStamp();
    void addStamp_allNames_nameMatchesPDFName();

    // ── Multiple stamps accumulate ─────────────────────────────────────────────
    void addStamp_twoStamps_bothPresent();
    void addStamp_mixedAnnots_stampDoesNotRemoveOthers();

    // ── Robustness ────────────────────────────────────────────────────────────
    void addStamp_outOfBoundsPage_noOp();
    void addStamp_negativePageIndex_noOp();
    void addStamp_zerosizeRect_annotStillAdded();
};

// ── Basic structure ───────────────────────────────────────────────────────────

void StampTest::addStamp_approved_addsExactlyOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    QCOMPARE(annotCount(*pdf, 0), 0);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void StampTest::addStamp_approved_subtypeIsStamp()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY2(a.getKey("/Subtype").isNameAndEquals("/Stamp"),
             "Expected /Subtype to be /Stamp");
}

void StampTest::addStamp_approved_nameIsSlashApproved()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY2(a.hasKey("/Name"), "Stamp annotation must have /Name key");
    QVERIFY2(a.getKey("/Name").isNameAndEquals("/Approved"),
             "Expected /Name to be /Approved");
}

void StampTest::addStamp_approved_rectHasFourCoords()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 50.0, 100.0, 144.0, 48.0, "Approved");
    auto rect = getAnnot(*pdf, 0, 0).getKey("/Rect");
    QCOMPARE(rect.getArrayNItems(), 4);
}

void StampTest::addStamp_approved_rectMatchesXYWH()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 50.0, 100.0, 144.0, 48.0, "Approved");
    auto rect = getAnnot(*pdf, 0, 0).getKey("/Rect");
    QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() -  50.0) < 0.5); // x0
    QVERIFY(qAbs(rect.getArrayItem(1).getNumericValue() - 100.0) < 0.5); // y0
    QVERIFY(qAbs(rect.getArrayItem(2).getNumericValue() - 194.0) < 0.5); // x0+w
    QVERIFY(qAbs(rect.getArrayItem(3).getNumericValue() - 148.0) < 0.5); // y0+h
}

void StampTest::addStamp_approved_hasPrintFlag()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY2(a.hasKey("/F"), "Stamp annotation must have /F (flags) key");
    // Bit 3 (value 4) = Print flag.  Other bits may be set too.
    QVERIFY2((a.getKey("/F").getIntValueAsInt() & 4) != 0,
             "Print flag (bit 3) must be set");
}

// ── Appearance stream ─────────────────────────────────────────────────────────
// Without /AP the stamp renders with an opaque white synthesized appearance
// that covers underlying text.  These tests enforce that addStamp embeds its
// own /AP so Poppler doesn't synthesize one.

void StampTest::addStamp_approved_hasAppearanceDictionary()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY2(a.hasKey("/AP"),
             "Stamp must have /AP appearance dict — without it viewers "
             "synthesize an opaque white box that hides page text");
}

void StampTest::addStamp_approved_apNStreamIsPresent()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto ap = getAnnot(*pdf, 0, 0).getKey("/AP");
    QVERIFY2(ap.hasKey("/N"), "AP dict must contain /N (normal appearance) stream");
}

void StampTest::addStamp_approved_apStreamIsNonEmpty()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");
    auto n = getAnnot(*pdf, 0, 0).getKey("/AP").getKey("/N");
    QVERIFY2(n.isStream(), "/AP /N must be a stream object");
    auto data = n.getStreamData(qpdf_dl_all);
    QVERIFY2(data->getSize() > 0, "/AP /N stream must not be empty");
}

// ── Content stream integrity ──────────────────────────────────────────────────
// These are the regression tests for the "text disappears" bug.
// addStamp must NEVER modify /Contents — it only touches /Annots.

void StampTest::addStamp_approved_contentStreamUnchanged()
{
    auto pdf = makeBlankPdf(1);
    std::string before = getContentStream(*pdf, 0);
    QVERIFY(!before.empty());

    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");

    std::string after = getContentStream(*pdf, 0);
    QCOMPARE(QString::fromStdString(after), QString::fromStdString(before));
}

void StampTest::addStamp_approved_contentStreamUnchangedAfterRoundTrip()
{
    auto pdf = makeBlankPdf(1);
    std::string before = getContentStream(*pdf, 0);

    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 144.0, 48.0, "Approved");

    // Inline the round-trip so the buffer outlives the QPDF object.
    // processMemoryFile does NOT copy the buffer — the memory must stay alive
    // as long as the reloaded QPDF is used.
    QPDFWriter w(*pdf);
    w.setOutputMemory();
    w.write();
    auto buf = w.getBufferSharedPointer();   // keeps buffer alive

    auto rt = std::make_shared<QPDF>();
    rt->processMemoryFile("roundtrip",
        reinterpret_cast<const char*>(buf->getBuffer()),
        buf->getSize(), nullptr);

    std::string after = getContentStream(*rt, 0);
    QVERIFY2(!after.empty(), "Content stream must not be empty after round-trip");
    QCOMPARE(QString::fromStdString(after).trimmed(),
             QString::fromStdString(before).trimmed());
}

// ── All named variants ────────────────────────────────────────────────────────

void StampTest::addStamp_allNames_contentStreamPreserved()
{
    for (const QString& name : ALL_STAMP_NAMES) {
        auto pdf = makeBlankPdf(1);
        std::string before = getContentStream(*pdf, 0);
        Annotations::addStamp(*pdf, 0, 50.0, 50.0, 144.0, 48.0, name);
        auto rt = roundTrip(*pdf);
        std::string after = getContentStream(*rt, 0);
        QVERIFY2(!after.empty(),
                 qPrintable(QString("Content stream empty after stamp '%1'").arg(name)));
        QVERIFY2(QString::fromStdString(after).trimmed() ==
                 QString::fromStdString(before).trimmed(),
                 qPrintable(QString("Content stream corrupted by stamp '%1'").arg(name)));
    }
}

void StampTest::addStamp_allNames_eachAddsOneAnnot()
{
    for (const QString& name : ALL_STAMP_NAMES) {
        auto pdf = makeBlankPdf(1);
        Annotations::addStamp(*pdf, 0, 50.0, 50.0, 144.0, 48.0, name);
        QCOMPARE(annotCount(*pdf, 0), 1);
    }
}

void StampTest::addStamp_allNames_subtypeAlwaysStamp()
{
    for (const QString& name : ALL_STAMP_NAMES) {
        auto pdf = makeBlankPdf(1);
        Annotations::addStamp(*pdf, 0, 50.0, 50.0, 144.0, 48.0, name);
        auto a = getAnnot(*pdf, 0, 0);
        QVERIFY2(a.getKey("/Subtype").isNameAndEquals("/Stamp"),
                 qPrintable(QString("Expected /Stamp subtype for name '%1'").arg(name)));
    }
}

void StampTest::addStamp_allNames_nameMatchesPDFName()
{
    for (const QString& name : ALL_STAMP_NAMES) {
        auto pdf = makeBlankPdf(1);
        Annotations::addStamp(*pdf, 0, 50.0, 50.0, 144.0, 48.0, name);
        auto a = getAnnot(*pdf, 0, 0);
        const std::string expected = "/" + name.toStdString();
        QVERIFY2(a.getKey("/Name").isNameAndEquals(expected),
                 qPrintable(QString("Expected /Name /%1 but got something else").arg(name)));
    }
}

// ── Multiple / mixed stamps ───────────────────────────────────────────────────

void StampTest::addStamp_twoStamps_bothPresent()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 50.0,  50.0, 144.0, 48.0, "Approved");
    Annotations::addStamp(*pdf, 0, 50.0, 200.0, 144.0, 48.0, "Draft");
    QCOMPARE(annotCount(*pdf, 0), 2);
    QVERIFY(getAnnot(*pdf, 0, 0).getKey("/Name").isNameAndEquals("/Approved"));
    QVERIFY(getAnnot(*pdf, 0, 1).getKey("/Name").isNameAndEquals("/Draft"));
}

void StampTest::addStamp_mixedAnnots_stampDoesNotRemoveOthers()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 100.0, 700.0, "sticky note");
    Annotations::addStamp(*pdf, 0, 50.0, 50.0, 144.0, 48.0, "Approved");
    QCOMPARE(annotCount(*pdf, 0), 2);

    // The first annot (note) must still be there
    auto note = getAnnot(*pdf, 0, 0);
    QVERIFY(note.getKey("/Subtype").isNameAndEquals("/Text"));
}

// ── Robustness ────────────────────────────────────────────────────────────────

void StampTest::addStamp_outOfBoundsPage_noOp()
{
    auto pdf = makeBlankPdf(2);
    Annotations::addStamp(*pdf, 99, 0.0, 0.0, 144.0, 48.0, "Approved");
    QCOMPARE(annotCount(*pdf, 0), 0);
    QCOMPARE(annotCount(*pdf, 1), 0);
}

void StampTest::addStamp_negativePageIndex_noOp()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, -1, 0.0, 0.0, 144.0, 48.0, "Approved");
    QCOMPARE(annotCount(*pdf, 0), 0);
}

void StampTest::addStamp_zerosizeRect_annotStillAdded()
{
    // Even a zero-size rect should produce a structurally valid annotation
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 0.0, 0.0, "Void");
    QCOMPARE(annotCount(*pdf, 0), 1);
}

QTEST_GUILESS_MAIN(StampTest)
#include "tst_stamp.moc"

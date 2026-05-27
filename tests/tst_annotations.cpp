#include <QtTest>
#include <QColor>
#include <QPointF>

#include "annotations.h"
#include "helpers.h"

#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

using namespace TestHelpers;

// Convenient single-quad for highlight tests
static Annotations::Quad makeQuad(double x0, double y0, double x1, double y1)
{
    return {x0, y0, x1, y0, x1, y1, x0, y1};
}

// ─────────────────────────────────────────────────────────────────────────────
class AnnotationsTest : public QObject
{
    Q_OBJECT

private slots:
    // ── addHighlight ──────────────────────────────────────────────────────────
    void addHighlight_addsOneAnnotation();
    void addHighlight_subtypeIsHighlight();
    void addHighlight_hasQuadPoints_8CoordsPerQuad();
    void addHighlight_colorMatchesInput();
    void addHighlight_opacityMatchesInput();
    void addHighlight_multipleCallsAccumulate();
    void addHighlight_twoQuads_16Coords();

    // ── addTextNote ───────────────────────────────────────────────────────────
    void addTextNote_addsOneAnnotation();
    void addTextNote_subtypeIsText();
    void addTextNote_contentsMatchInput();
    void addTextNote_rectPositionedAtXY();
    void addTextNote_isNotOpenByDefault();

    // ── addInk ────────────────────────────────────────────────────────────────
    void addInk_addsOneAnnotation();
    void addInk_subtypeIsInk();
    void addInk_inkListCountMatchesStrokeCount();
    void addInk_firstStrokeCoordsCount();
    void addInk_boundingRectEnclosesPts();

    // ── addStamp ──────────────────────────────────────────────────────────────
    void addStamp_addsOneAnnotation();
    void addStamp_subtypeIsStamp();
    void addStamp_nameMatchesInput();
    void addStamp_rectMatchesPositionAndSize();

    // ── addRedact ─────────────────────────────────────────────────────────────
    void addRedact_addsOneAnnotation();
    void addRedact_subtypeIsRedact();
    void addRedact_rectMatchesInput();

    // ── applyRedactions ───────────────────────────────────────────────────────
    void applyRedactions_removesRedactAnnots();
    void applyRedactions_preservesNonRedactAnnots();
    void applyRedactions_onlyRedactsRemoved_mixedAnnots();
    void applyRedactions_pageWithNoAnnots_noOp();

    // ── boundary / robustness ─────────────────────────────────────────────────
    void allAnnotFunctions_outOfBoundsPageIgnored();
};

// ─── addHighlight ────────────────────────────────────────────────────────────

void AnnotationsTest::addHighlight_addsOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 200, 30)});
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void AnnotationsTest::addHighlight_subtypeIsHighlight()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 200, 30)});
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Highlight"));
}

void AnnotationsTest::addHighlight_hasQuadPoints_8CoordsPerQuad()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 200, 30)});
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/QuadPoints"));
    QCOMPARE(a.getKey("/QuadPoints").getArrayNItems(), 8); // 1 quad × 8 coords
}

void AnnotationsTest::addHighlight_colorMatchesInput()
{
    auto pdf = makeBlankPdf(1);
    QColor color(200, 100, 50);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 100, 30)}, color);
    auto a = getAnnot(*pdf, 0, 0);
    auto c = a.getKey("/C");
    QCOMPARE(c.getArrayNItems(), 3);
    // Color stored as real values in [0,1]; allow for rounding to 6 dp
    QVERIFY(qAbs(c.getArrayItem(0).getNumericValue() - color.redF())   < 0.01);
    QVERIFY(qAbs(c.getArrayItem(1).getNumericValue() - color.greenF()) < 0.01);
    QVERIFY(qAbs(c.getArrayItem(2).getNumericValue() - color.blueF())  < 0.01);
}

void AnnotationsTest::addHighlight_opacityMatchesInput()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 100, 30)}, QColor(255,255,0), 0.75);
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/CA"));
    QVERIFY(qAbs(a.getKey("/CA").getNumericValue() - 0.75) < 0.01);
}

void AnnotationsTest::addHighlight_multipleCallsAccumulate()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addHighlight(*pdf, 0, {makeQuad(10, 10, 100, 30)});
    Annotations::addHighlight(*pdf, 0, {makeQuad(20, 50, 200, 70)});
    QCOMPARE(annotCount(*pdf, 0), 2);
}

void AnnotationsTest::addHighlight_twoQuads_16Coords()
{
    auto pdf = makeBlankPdf(1);
    QVector<Annotations::Quad> quads = {makeQuad(10, 10, 100, 30),
                                         makeQuad(10, 40, 100, 60)};
    Annotations::addHighlight(*pdf, 0, quads);
    auto a = getAnnot(*pdf, 0, 0);
    QCOMPARE(a.getKey("/QuadPoints").getArrayNItems(), 16); // 2 quads × 8 coords
}

// ─── addTextNote ─────────────────────────────────────────────────────────────

void AnnotationsTest::addTextNote_addsOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "test note");
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void AnnotationsTest::addTextNote_subtypeIsText()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "hi");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Text"));
}

void AnnotationsTest::addTextNote_contentsMatchInput()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "Hello World");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/Contents"));
    // newUnicodeString encodes as UTF-16BE; getUTF8Value() decodes it correctly
    QCOMPARE(QString::fromStdString(a.getKey("/Contents").getUTF8Value()),
             QString("Hello World"));
}

void AnnotationsTest::addTextNote_rectPositionedAtXY()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "X");
    auto a    = getAnnot(*pdf, 0, 0);
    auto rect = a.getKey("/Rect");
    QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() -  50.0) < 0.5); // x
    QVERIFY(qAbs(rect.getArrayItem(1).getNumericValue() - 100.0) < 0.5); // y
    QVERIFY(qAbs(rect.getArrayItem(2).getNumericValue() -  74.0) < 0.5); // x+24
    QVERIFY(qAbs(rect.getArrayItem(3).getNumericValue() - 124.0) < 0.5); // y+24
}

void AnnotationsTest::addTextNote_isNotOpenByDefault()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "note");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/Open"));
    QCOMPARE(a.getKey("/Open").getBoolValue(), false);
}

// ─── addInk ──────────────────────────────────────────────────────────────────

void AnnotationsTest::addInk_addsOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    QVector<QVector<QPointF>> strokes = {{{10,10},{20,20},{30,10}}};
    Annotations::addInk(*pdf, 0, strokes);
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void AnnotationsTest::addInk_subtypeIsInk()
{
    auto pdf = makeBlankPdf(1);
    QVector<QVector<QPointF>> strokes = {{{10,10},{20,20}}};
    Annotations::addInk(*pdf, 0, strokes);
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Ink"));
}

void AnnotationsTest::addInk_inkListCountMatchesStrokeCount()
{
    auto pdf = makeBlankPdf(1);
    QVector<QVector<QPointF>> strokes = {
        {{10,10},{20,20},{30,10}},
        {{50,50},{60,60}}
    };
    Annotations::addInk(*pdf, 0, strokes);
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/InkList"));
    QCOMPARE(a.getKey("/InkList").getArrayNItems(), 2); // 2 strokes
}

void AnnotationsTest::addInk_firstStrokeCoordsCount()
{
    auto pdf = makeBlankPdf(1);
    QVector<QVector<QPointF>> strokes = {{{10,10},{20,20},{30,10}}}; // 3 pts
    Annotations::addInk(*pdf, 0, strokes);
    auto a = getAnnot(*pdf, 0, 0);
    // 3 points × 2 coords = 6 values in first stroke array
    QCOMPARE(a.getKey("/InkList").getArrayItem(0).getArrayNItems(), 6);
}

void AnnotationsTest::addInk_boundingRectEnclosesPts()
{
    auto pdf = makeBlankPdf(1);
    QVector<QVector<QPointF>> strokes = {{{10,20},{60,80}}};
    Annotations::addInk(*pdf, 0, strokes);
    auto a    = getAnnot(*pdf, 0, 0);
    auto rect = a.getKey("/Rect");
    // Implementation adds 2pt padding: rect = [x0-2, y0-2, x1+2, y1+2]
    QVERIFY(rect.getArrayItem(0).getNumericValue() <=  10.0); // left
    QVERIFY(rect.getArrayItem(1).getNumericValue() <=  20.0); // bottom
    QVERIFY(rect.getArrayItem(2).getNumericValue() >=  60.0); // right
    QVERIFY(rect.getArrayItem(3).getNumericValue() >=  80.0); // top
}

// ─── addStamp ────────────────────────────────────────────────────────────────

void AnnotationsTest::addStamp_addsOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 80.0, 30.0, "Approved");
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void AnnotationsTest::addStamp_subtypeIsStamp()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 80.0, 30.0, "Draft");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Stamp"));
}

void AnnotationsTest::addStamp_nameMatchesInput()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 100.0, 200.0, 80.0, 30.0, "Approved");
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.hasKey("/Name"));
    QVERIFY(a.getKey("/Name").isNameAndEquals("/Approved"));
}

void AnnotationsTest::addStamp_rectMatchesPositionAndSize()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addStamp(*pdf, 0, 50.0, 100.0, 120.0, 40.0, "Final");
    auto a    = getAnnot(*pdf, 0, 0);
    auto rect = a.getKey("/Rect");
    QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() -  50.0) < 0.5); // x
    QVERIFY(qAbs(rect.getArrayItem(1).getNumericValue() - 100.0) < 0.5); // y
    QVERIFY(qAbs(rect.getArrayItem(2).getNumericValue() - 170.0) < 0.5); // x+w
    QVERIFY(qAbs(rect.getArrayItem(3).getNumericValue() - 140.0) < 0.5); // y+h
}

// ─── addRedact ───────────────────────────────────────────────────────────────

void AnnotationsTest::addRedact_addsOneAnnotation()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addRedact(*pdf, 0, 10.0, 20.0, 200.0, 50.0);
    QCOMPARE(annotCount(*pdf, 0), 1);
}

void AnnotationsTest::addRedact_subtypeIsRedact()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addRedact(*pdf, 0, 10.0, 20.0, 200.0, 50.0);
    auto a = getAnnot(*pdf, 0, 0);
    QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Redact"));
}

void AnnotationsTest::addRedact_rectMatchesInput()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addRedact(*pdf, 0, 10.0, 20.0, 200.0, 50.0);
    auto a    = getAnnot(*pdf, 0, 0);
    auto rect = a.getKey("/Rect");
    QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() -  10.0) < 0.5);
    QVERIFY(qAbs(rect.getArrayItem(1).getNumericValue() -  20.0) < 0.5);
    QVERIFY(qAbs(rect.getArrayItem(2).getNumericValue() - 200.0) < 0.5);
    QVERIFY(qAbs(rect.getArrayItem(3).getNumericValue() -  50.0) < 0.5);
}

// ─── applyRedactions ─────────────────────────────────────────────────────────

void AnnotationsTest::applyRedactions_removesRedactAnnots()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addRedact(*pdf, 0, 0, 0, 100, 50);
    QCOMPARE(annotCount(*pdf, 0), 1);

    Annotations::applyRedactions(*pdf, 0);
    QCOMPARE(annotCount(*pdf, 0), 0);
}

void AnnotationsTest::applyRedactions_preservesNonRedactAnnots()
{
    auto pdf = makeBlankPdf(1);
    Annotations::addTextNote(*pdf, 0, 50.0, 100.0, "keep me");
    Annotations::addRedact(*pdf, 0, 0, 0, 100, 50);
    QCOMPARE(annotCount(*pdf, 0), 2);

    Annotations::applyRedactions(*pdf, 0);
    QCOMPARE(annotCount(*pdf, 0), 1);

    auto surviving = getAnnot(*pdf, 0, 0);
    QVERIFY(surviving.getKey("/Subtype").isNameAndEquals("/Text"));
}

void AnnotationsTest::applyRedactions_onlyRedactsRemoved_mixedAnnots()
{
    auto pdf = makeBlankPdf(1);
    // 2 text notes + 3 redacts
    Annotations::addTextNote(*pdf, 0, 10, 10, "note1");
    Annotations::addTextNote(*pdf, 0, 20, 20, "note2");
    Annotations::addRedact(*pdf, 0,  0,  0, 100, 50);
    Annotations::addRedact(*pdf, 0, 50, 50, 200, 80);
    Annotations::addRedact(*pdf, 0, 10, 10,  30, 30);
    QCOMPARE(annotCount(*pdf, 0), 5);

    Annotations::applyRedactions(*pdf, 0);
    QCOMPARE(annotCount(*pdf, 0), 2);

    // Verify all survivors are Text, not Redact
    for (int i = 0; i < 2; ++i) {
        auto a = getAnnot(*pdf, 0, i);
        QVERIFY(a.getKey("/Subtype").isNameAndEquals("/Text"));
    }
}

void AnnotationsTest::applyRedactions_pageWithNoAnnots_noOp()
{
    auto pdf = makeBlankPdf(1);
    // No annotations added — calling applyRedactions should not crash
    Annotations::applyRedactions(*pdf, 0);
    QCOMPARE(annotCount(*pdf, 0), 0);
}

// ─── boundary / robustness ────────────────────────────────────────────────────

void AnnotationsTest::allAnnotFunctions_outOfBoundsPageIgnored()
{
    auto pdf = makeBlankPdf(2);

    // None of these should throw or crash; all target page 99 (out of range)
    Annotations::addHighlight(*pdf, 99, {makeQuad(0,0,10,10)});
    Annotations::addTextNote(*pdf, 99, 0, 0, "oob");
    Annotations::addInk(*pdf, 99, {{{0,0},{10,10}}});
    Annotations::addStamp(*pdf, 99, 0, 0, 10, 10, "Test");
    Annotations::addRedact(*pdf, 99, 0, 0, 10, 10);
    Annotations::applyRedactions(*pdf, 99);

    // Legitimate pages should be unmodified
    QCOMPARE(annotCount(*pdf, 0), 0);
    QCOMPARE(annotCount(*pdf, 1), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_GUILESS_MAIN(AnnotationsTest)
#include "tst_annotations.moc"

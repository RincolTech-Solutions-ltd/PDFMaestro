#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QDir>

#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
class ConfigTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_tmpDir;

private slots:
    // ── lifecycle ─────────────────────────────────────────────────────────────
    void initTestCase();
    void cleanupTestCase();
    void cleanup(); // runs after EACH test method — clears settings

    // ── defaults ──────────────────────────────────────────────────────────────
    void defaults_maxRecentIs8();
    void defaults_restoreLastIsFalse();
    void defaults_defaultZoomIs100Percent();
    void defaults_showTocPanelIsTrue();
    void defaults_showPagePanelIsTrue();
    void defaults_windowGeometryIsEmpty();
    void defaults_windowStateIsEmpty();
    void defaults_lastOpenDirIsHomePath();

    // ── recentFiles / addRecentFile ───────────────────────────────────────────
    void recentFiles_emptyOnFresh();
    void addRecentFile_firstFile();
    void addRecentFile_deduplicates_movesToFront();
    void addRecentFile_prependsNewEntry();
    void addRecentFile_evictsOldestWhenFull();
    void clearRecentFiles_emptiesList();

    // ── coerceList bug-fix ────────────────────────────────────────────────────
    void coerceList_handlesBareSingleStringFromQSettings();
    void coerceList_handlesEmptyStringFromQSettings();

    // ── maxRecentFiles ────────────────────────────────────────────────────────
    void maxRecentFiles_roundTrips();
    void maxRecentFiles_enforcedOnNextAdd();

    // ── bool settings ─────────────────────────────────────────────────────────
    void restoreLastDocument_roundTrips();
    void showTocPanel_roundTrips();
    void showPagePanel_roundTrips();

    // ── string settings ───────────────────────────────────────────────────────
    void defaultZoom_roundTrips();
    void lastOpenDir_roundTrips();

    // ── byte array settings ───────────────────────────────────────────────────
    void windowGeometry_roundTrips();
    void windowState_roundTrips();
};

// ─── lifecycle ───────────────────────────────────────────────────────────────

void ConfigTest::initTestCase()
{
    QVERIFY(m_tmpDir.isValid());
    // Redirect QSettings to a temp directory so tests never touch real user config.
    // Config hardcodes QSettings("RincolTech","PDFMaestro") — this intercepts it.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmpDir.path());
    // On Linux the default format IS IniFormat, so this covers the constructor.
}

void ConfigTest::cleanupTestCase()
{
    // QTemporaryDir cleans itself up on destruction.
}

void ConfigTest::cleanup()
{
    // Wipe all Config settings between test methods to guarantee isolation.
    QSettings("RincolTech", "PDFMaestro").clear();
}

// ─── defaults ────────────────────────────────────────────────────────────────

void ConfigTest::defaults_maxRecentIs8()
{
    Config c;
    QCOMPARE(c.maxRecentFiles(), 8);
}

void ConfigTest::defaults_restoreLastIsFalse()
{
    Config c;
    QCOMPARE(c.restoreLastDocument(), false);
}

void ConfigTest::defaults_defaultZoomIs100Percent()
{
    Config c;
    QCOMPARE(c.defaultZoom(), QString("100%"));
}

void ConfigTest::defaults_showTocPanelIsTrue()
{
    Config c;
    QCOMPARE(c.showTocPanel(), true);
}

void ConfigTest::defaults_showPagePanelIsTrue()
{
    Config c;
    QCOMPARE(c.showPagePanel(), true);
}

void ConfigTest::defaults_windowGeometryIsEmpty()
{
    Config c;
    QVERIFY(c.windowGeometry().isEmpty());
}

void ConfigTest::defaults_windowStateIsEmpty()
{
    Config c;
    QVERIFY(c.windowState().isEmpty());
}

void ConfigTest::defaults_lastOpenDirIsHomePath()
{
    Config c;
    QCOMPARE(c.lastOpenDir(), QDir::homePath());
}

// ─── recentFiles / addRecentFile ─────────────────────────────────────────────

void ConfigTest::recentFiles_emptyOnFresh()
{
    Config c;
    QVERIFY(c.recentFiles().isEmpty());
}

void ConfigTest::addRecentFile_firstFile()
{
    Config c;
    c.addRecentFile("/docs/a.pdf");
    QCOMPARE(c.recentFiles(), QStringList{"/docs/a.pdf"});
}

void ConfigTest::addRecentFile_deduplicates_movesToFront()
{
    Config c;
    c.addRecentFile("/a.pdf");
    c.addRecentFile("/b.pdf");
    c.addRecentFile("/a.pdf"); // re-add /a.pdf — should move to front

    auto r = c.recentFiles();
    QCOMPARE(r.size(), 2);
    QCOMPARE(r[0], QString("/a.pdf")); // moved to front
    QCOMPARE(r[1], QString("/b.pdf"));
}

void ConfigTest::addRecentFile_prependsNewEntry()
{
    Config c;
    c.addRecentFile("/first.pdf");
    c.addRecentFile("/second.pdf");

    auto r = c.recentFiles();
    QCOMPARE(r.size(), 2);
    QCOMPARE(r[0], QString("/second.pdf")); // newest first
    QCOMPARE(r[1], QString("/first.pdf"));
}

void ConfigTest::addRecentFile_evictsOldestWhenFull()
{
    Config c;
    c.setMaxRecentFiles(3);
    c.addRecentFile("/1.pdf");
    c.addRecentFile("/2.pdf");
    c.addRecentFile("/3.pdf");
    c.addRecentFile("/4.pdf"); // should evict "/1.pdf"

    auto r = c.recentFiles();
    QCOMPARE(r.size(), 3);
    QVERIFY(!r.contains("/1.pdf"));
    QCOMPARE(r[0], QString("/4.pdf")); // newest at front
}

void ConfigTest::clearRecentFiles_emptiesList()
{
    Config c;
    c.addRecentFile("/a.pdf");
    c.addRecentFile("/b.pdf");
    c.clearRecentFiles();
    QVERIFY(c.recentFiles().isEmpty());
}

// ─── coerceList bug-fix ──────────────────────────────────────────────────────

void ConfigTest::coerceList_handlesBareSingleStringFromQSettings()
{
    // Simulate what QSettings does when it reads back a single-item list that
    // was written as a bare string (not a list) — the coerceList() fix handles this.
    QSettings raw("RincolTech", "PDFMaestro");
    raw.setValue("recent_files", QString("/only/one.pdf")); // bare string, NOT QStringList
    raw.sync();

    Config c;
    auto r = c.recentFiles();
    QCOMPARE(r.size(), 1);
    QCOMPARE(r[0], QString("/only/one.pdf"));
}

void ConfigTest::coerceList_handlesEmptyStringFromQSettings()
{
    QSettings raw("RincolTech", "PDFMaestro");
    raw.setValue("recent_files", QString("")); // empty bare string
    raw.sync();

    Config c;
    QVERIFY(c.recentFiles().isEmpty());
}

// ─── maxRecentFiles ──────────────────────────────────────────────────────────

void ConfigTest::maxRecentFiles_roundTrips()
{
    Config c;
    c.setMaxRecentFiles(15);
    QCOMPARE(c.maxRecentFiles(), 15);
}

void ConfigTest::maxRecentFiles_enforcedOnNextAdd()
{
    Config c;
    c.setMaxRecentFiles(2);
    c.addRecentFile("/1.pdf");
    c.addRecentFile("/2.pdf");
    c.addRecentFile("/3.pdf");
    QCOMPARE(c.recentFiles().size(), 2);
}

// ─── bool settings ───────────────────────────────────────────────────────────

void ConfigTest::restoreLastDocument_roundTrips()
{
    Config c;
    c.setRestoreLastDocument(true);
    QCOMPARE(c.restoreLastDocument(), true);
    c.setRestoreLastDocument(false);
    QCOMPARE(c.restoreLastDocument(), false);
}

void ConfigTest::showTocPanel_roundTrips()
{
    Config c;
    c.setShowTocPanel(false);
    QCOMPARE(c.showTocPanel(), false);
    c.setShowTocPanel(true);
    QCOMPARE(c.showTocPanel(), true);
}

void ConfigTest::showPagePanel_roundTrips()
{
    Config c;
    c.setShowPagePanel(false);
    QCOMPARE(c.showPagePanel(), false);
    c.setShowPagePanel(true);
    QCOMPARE(c.showPagePanel(), true);
}

// ─── string settings ─────────────────────────────────────────────────────────

void ConfigTest::defaultZoom_roundTrips()
{
    Config c;
    c.setDefaultZoom("150%");
    QCOMPARE(c.defaultZoom(), QString("150%"));
}

void ConfigTest::lastOpenDir_roundTrips()
{
    Config c;
    c.setLastOpenDir("/home/genius/Documents");
    QCOMPARE(c.lastOpenDir(), QString("/home/genius/Documents"));
}

// ─── byte array settings ─────────────────────────────────────────────────────

void ConfigTest::windowGeometry_roundTrips()
{
    Config c;
    QByteArray geo("fake-geometry-data", 18);
    c.setWindowGeometry(geo);
    QCOMPARE(c.windowGeometry(), geo);
}

void ConfigTest::windowState_roundTrips()
{
    Config c;
    QByteArray state("fake-state-data", 15);
    c.setWindowState(state);
    QCOMPARE(c.windowState(), state);
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_GUILESS_MAIN(ConfigTest)
#include "tst_config.moc"

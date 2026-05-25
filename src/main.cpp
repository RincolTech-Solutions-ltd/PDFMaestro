#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("PDFMaestro");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("RincolTech");
    app.setWindowIcon(QIcon::fromTheme("application-pdf"));

    QCommandLineParser parser;
    parser.setApplicationDescription("PDF viewer, editor, and annotation tool");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "PDF file to open", "[file]");
    parser.process(app);

    MainWindow win;
    win.show();

    const auto args = parser.positionalArguments();
    if (!args.isEmpty()) win.openFile(args.first());

    return app.exec();
}

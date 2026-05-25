#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>

SettingsDialog::SettingsDialog(Config* config, QWidget* parent)
    : QDialog(parent), m_config(config)
{
    setWindowTitle("Settings");
    setMinimumWidth(340);

    auto* root = new QVBoxLayout(this);

    // General group
    auto* genGroup  = new QGroupBox("General", this);
    auto* genLayout = new QFormLayout(genGroup);

    m_maxRecent = new QSpinBox(this);
    m_maxRecent->setRange(1, 20);
    m_maxRecent->setValue(m_config->maxRecentFiles());
    genLayout->addRow("Max recent files:", m_maxRecent);

    m_restoreLastDoc = new QCheckBox(this);
    m_restoreLastDoc->setChecked(m_config->restoreLastDocument());
    genLayout->addRow("Restore last document on launch:", m_restoreLastDoc);

    // View group
    auto* viewGroup  = new QGroupBox("View", this);
    auto* viewLayout = new QFormLayout(viewGroup);

    m_defaultZoom = new QComboBox(this);
    m_defaultZoom->addItems({ "Fit Page", "Fit Width", "100%", "150%", "200%" });
    m_defaultZoom->setCurrentText(m_config->defaultZoom());
    viewLayout->addRow("Default zoom:", m_defaultZoom);

    m_showToc = new QCheckBox(this);
    m_showToc->setChecked(m_config->showTocPanel());
    viewLayout->addRow("Show table of contents panel:", m_showToc);

    m_showPagePanel = new QCheckBox(this);
    m_showPagePanel->setChecked(m_config->showPagePanel());
    viewLayout->addRow("Show page panel:", m_showPagePanel);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(genGroup);
    root->addWidget(viewGroup);
    root->addWidget(btns);
}

void SettingsDialog::onAccept() {
    m_config->setMaxRecentFiles(m_maxRecent->value());
    m_config->setRestoreLastDocument(m_restoreLastDoc->isChecked());
    m_config->setDefaultZoom(m_defaultZoom->currentText());
    m_config->setShowTocPanel(m_showToc->isChecked());
    m_config->setShowPagePanel(m_showPagePanel->isChecked());
    accept();
}

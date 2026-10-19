#include "SettingsDialog.h"

#include "../application/Application.h"
#include "../application/Settings.h"
#include "../core/Logging.h"
#include "../core/Paths.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace eda {

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    m_settings = Application::instance()->settings();

    setWindowTitle(tr("Settings"));
    setModal(true);
    resize(820, 560);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_pageList = new QListWidget(this);
    m_pageList->setFixedWidth(190);
    m_pageList->setIconSize(QSize(16, 16));
    layout->addWidget(m_pageList);

    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages, 1);

    struct PageEntry { QString title; QWidget *page; };
    const QList<PageEntry> pages = {
        { tr("Identity"), createIdentityPage() },
        { tr("Appearance"), createAppearancePage() },
        { tr("Language"), createLanguagePage() },
        { tr("Directories"), createDirectoriesPage() },
        { tr("AI Provider"), createAiPage() },
        { tr("Privacy & Network"), createPrivacyPage() },
        { tr("Database"), createDatabasePage() },
        { tr("Logging"), createLoggingPage() },
        { tr("Import Options"), createImportPage() },
    };
    for (const PageEntry &entry : pages) {
        m_pageList->addItem(entry.title);
        m_pages->addWidget(entry.page);
    }

    auto *buttonRow = new QVBoxLayout();
    buttonRow->addStretch(1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply
                                             | QDialogButtonBox::Cancel,
                                         Qt::Vertical, this);
    auto *resetButton = buttons->addButton(tr("Reset to defaults"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::apply);
    buttonRow->addWidget(buttons);
    layout->addLayout(buttonRow);

    connect(m_pageList, &QListWidget::currentRowChanged, m_pages,
            &QStackedWidget::setCurrentIndex);

    loadFromSettings();
    m_pageList->setCurrentRow(0);
}

void SettingsDialog::setCurrentPage(int page)
{
    if (page < 0 || page >= m_pageList->count())
        return;
    m_pageList->setCurrentRow(page);
    m_pages->setCurrentIndex(page);
}

// ---------------------------------------------------------------------------
// pages
// ---------------------------------------------------------------------------
QWidget *SettingsDialog::createIdentityPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Application identity"), page);
    auto *form = new QFormLayout(group);

    m_nameEdit = new QLineEdit(group);
    m_nameEdit->setToolTip(tr("Shown in the window title, the About dialog and report headers."));
    form->addRow(tr("Application name:"), m_nameEdit);

    m_subtitleEdit = new QLineEdit(group);
    form->addRow(tr("Subtitle:"), m_subtitleEdit);

    m_organizationEdit = new QLineEdit(group);
    form->addRow(tr("Organization:"), m_organizationEdit);

    auto *iconRow = new QHBoxLayout();
    m_iconEdit = new QLineEdit(group);
    m_iconEdit->setPlaceholderText(tr("(built-in placeholder icon)"));
    iconRow->addWidget(m_iconEdit, 1);
    auto *browseButton = new QPushButton(tr("Browse..."), group);
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::chooseIcon);
    iconRow->addWidget(browseButton);
    m_iconPreview = new QLabel(group);
    m_iconPreview->setFixedSize(32, 32);
    iconRow->addWidget(m_iconPreview);
    form->addRow(tr("Icon:"), iconRow);

    m_reportTitleEdit = new QLineEdit(group);
    m_reportTitleEdit->setPlaceholderText(tr("(derived from the application name)"));
    form->addRow(tr("Report title:"), m_reportTitleEdit);

    m_reportFooterEdit = new QLineEdit(group);
    m_reportFooterEdit->setPlaceholderText(tr("(derived from the organization)"));
    form->addRow(tr("Report footer:"), m_reportFooterEdit);

    auto *accentRow = new QHBoxLayout();
    m_accentSwatch = new QLabel(group);
    m_accentSwatch->setFixedSize(28, 20);
    m_accentSwatch->setFrameShape(QFrame::Box);
    auto *accentButton = new QPushButton(tr("Primary accent..."), group);
    connect(accentButton, &QPushButton::clicked, this,
            [this]() { chooseColor(m_accentSwatch, &m_accent); });
    accentRow->addWidget(m_accentSwatch);
    accentRow->addWidget(accentButton);
    accentRow->addStretch(1);
    form->addRow(tr("Accent colour:"), accentRow);

    auto *accent2Row = new QHBoxLayout();
    m_accentSecondarySwatch = new QLabel(group);
    m_accentSecondarySwatch->setFixedSize(28, 20);
    m_accentSecondarySwatch->setFrameShape(QFrame::Box);
    auto *accent2Button = new QPushButton(tr("Secondary accent..."), group);
    connect(accent2Button, &QPushButton::clicked, this,
            [this]() { chooseColor(m_accentSecondarySwatch, &m_accentSecondary); });
    accent2Row->addWidget(m_accentSecondarySwatch);
    accent2Row->addWidget(accent2Button);
    accent2Row->addStretch(1);
    form->addRow(tr("Secondary colour:"), accent2Row);

    layout->addWidget(group);
    layout->addStretch(1);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &SettingsDialog::refreshIdentityPreview);
    return page;
}

QWidget *SettingsDialog::createAppearancePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Appearance"), page);
    auto *form = new QFormLayout(group);

    m_themeCombo = new QComboBox(group);
    m_themeCombo->addItem(tr("Dark industrial (default)"), QStringLiteral("dark"));
    m_themeCombo->addItem(tr("Light technical"), QStringLiteral("light"));
    m_themeCombo->setToolTip(tr("The light theme reduces contrast for bright rooms; the dark "
                                "theme is the default for workshop use."));
    form->addRow(tr("Theme:"), m_themeCombo);

    m_fontSizeCombo = new QComboBox(group);
    m_fontSizeCombo->addItem(tr("Small"), 8);
    m_fontSizeCombo->addItem(tr("Normal"), 9);
    m_fontSizeCombo->addItem(tr("Large"), 11);
    m_fontSizeCombo->addItem(tr("Very large"), 13);
    form->addRow(tr("Interface text size:"), m_fontSizeCombo);

    m_showGridCheck = new QCheckBox(tr("Show the millimetre grid in the board view"), group);
    form->addRow(QString(), m_showGridCheck);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createLanguagePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Language"), page);
    auto *form = new QFormLayout(group);
    m_languageCombo = new QComboBox(group);
    m_languageCombo->addItem(tr("English"), QStringLiteral("en"));
    form->addRow(tr("Interface language:"), m_languageCombo);

    auto *note = new QLabel(tr("The interface language is English. Additional translations can be "
                               "added as Qt translation files; the selection is stored per user."),
                            group);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    form->addRow(QString(), note);
    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createDirectoriesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Directories"), page);
    auto *form = new QFormLayout(group);

    auto addPath = [this, form](QLineEdit **edit, const QString &label, const QString &toolTip) {
        auto *row = new QHBoxLayout();
        *edit = new QLineEdit();
        (*edit)->setToolTip(toolTip);
        row->addWidget(*edit, 1);
        auto *button = new QPushButton(tr("Browse..."));
        connect(button, &QPushButton::clicked, this, [this, edit]() {
            const QString directory = browseDirectory((*edit)->text(), tr("Choose directory"));
            if (!directory.isEmpty())
                (*edit)->setText(directory);
        });
        row->addWidget(button);
        form->addRow(label, row);
    };

    addPath(&m_projectDirEdit, tr("Default project directory:"),
            tr("Where new projects are created and where the file dialogs start."));
    addPath(&m_databaseEdit, tr("Knowledge database:"),
           tr("SQLite file that holds the component and document knowledge base."));
    addPath(&m_logDirEdit, tr("Log directory:"), tr("Where the rotating log files are written."));
    addPath(&m_backupDirEdit, tr("Backup directory:"),
           tr("Where database and project backups are stored."));

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createAiPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("AI provider"), page);
    auto *form = new QFormLayout(group);

    m_providerCombo = new QComboBox(group);
    m_providerCombo->addItem(tr("None (offline)"), QStringLiteral("none"));
    m_providerCombo->addItem(tr("Ollama (local)"), QStringLiteral("ollama"));
    m_providerCombo->addItem(tr("OpenAI compatible HTTP API (local or cloud)"),
                             QStringLiteral("openai-compatible"));
    form->addRow(tr("Provider:"), m_providerCombo);

    m_endpointEdit = new QLineEdit(group);
    m_endpointEdit->setPlaceholderText(tr("http://localhost:11434"));
    form->addRow(tr("Endpoint:"), m_endpointEdit);

    m_modelEdit = new QLineEdit(group);
    m_modelEdit->setPlaceholderText(tr("model name"));
    form->addRow(tr("Model:"), m_modelEdit);

    m_timeoutSpin = new QSpinBox(group);
    m_timeoutSpin->setRange(5, 600);
    m_timeoutSpin->setSuffix(tr(" s"));
    form->addRow(tr("Request timeout:"), m_timeoutSpin);

    m_maxTokensSpin = new QSpinBox(group);
    m_maxTokensSpin->setRange(256, 32768);
    m_maxTokensSpin->setSingleStep(256);
    form->addRow(tr("Maximum response size:"), m_maxTokensSpin);

    m_aiStatusLabel = new QLabel(group);
    m_aiStatusLabel->setWordWrap(true);
    m_aiStatusLabel->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    form->addRow(QString(), m_aiStatusLabel);

    auto *note = new QLabel(
        tr("A local provider keeps every board detail on this machine. Cloud providers are only "
           "used when they are enabled below and are always shown as such in the status bar."),
        group);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    form->addRow(QString(), note);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createPrivacyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Privacy and network access"), page);
    auto *form = new QVBoxLayout(group);

    m_allowNetworkCheck = new QCheckBox(tr("Allow the application to use the network"), group);
    form->addWidget(m_allowNetworkCheck);
    m_allowCloudCheck = new QCheckBox(tr("Allow cloud AI providers"), group);
    form->addWidget(m_allowCloudCheck);
    m_onlineResearchCheck = new QCheckBox(tr("Allow online research for datasheets"), group);
    form->addWidget(m_onlineResearchCheck);
    m_sendBoardDataCheck = new QCheckBox(
        tr("Allow board data (references, nets, measurements) to be sent to a provider"), group);
    form->addWidget(m_sendBoardDataCheck);

    auto *note = new QLabel(
        tr("Nothing is sent anywhere while these options are disabled. The application starts and "
           "stays usable with every network option turned off."),
        group);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    form->addWidget(note);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createDatabasePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Knowledge database"), page);
    auto *form = new QVBoxLayout(group);
    auto *info = new QLabel(
        tr("The local knowledge base stores component records, indexed documents and their "
           "provenance. It is created on first use; the file can be moved with the "
           "Directories page."),
        group);
    info->setWordWrap(true);
    form->addWidget(info);

    auto *status = new QLabel(tr("Location: %1").arg(QDir::toNativeSeparators(Paths::databaseFile())),
                              group);
    status->setWordWrap(true);
    status->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    form->addWidget(status);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createLoggingPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Logging and diagnostics"), page);
    auto *form = new QFormLayout(group);

    m_logLevelCombo = new QComboBox(group);
    m_logLevelCombo->addItem(tr("Debug"), QStringLiteral("debug"));
    m_logLevelCombo->addItem(tr("Info"), QStringLiteral("info"));
    m_logLevelCombo->addItem(tr("Warning"), QStringLiteral("warning"));
    m_logLevelCombo->addItem(tr("Error"), QStringLiteral("error"));
    form->addRow(tr("Log level:"), m_logLevelCombo);

    m_logHistorySpin = new QSpinBox(group);
    m_logHistorySpin->setRange(100, 50000);
    m_logHistorySpin->setSingleStep(500);
    form->addRow(tr("Entries kept in memory:"), m_logHistorySpin);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createImportPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(tr("Import and parsing"), page);
    auto *form = new QFormLayout(group);

    m_importUnitsCombo = new QComboBox(group);
    m_importUnitsCombo->addItem(tr("Millimetres"), QStringLiteral("mm"));
    m_importUnitsCombo->addItem(tr("Inches"), QStringLiteral("inch"));
    form->addRow(tr("Board units:"), m_importUnitsCombo);

    auto *vbox = new QVBoxLayout();
    m_importMissingRefsCheck = new QCheckBox(
        tr("Mark components without a reference designator instead of numbering them"), group);
    m_importMissingRefsCheck->setToolTip(
        tr("An importer never invents a reference designator. When this option is off the "
           "component is still shown, with an empty reference."));
    vbox->addWidget(m_importMissingRefsCheck);
    m_importSkipUnverifiedCheck = new QCheckBox(
        tr("Do not import connectivity that cannot be verified from the source file"), group);
    vbox->addWidget(m_importSkipUnverifiedCheck);
    form->addRow(QString(), vbox);

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

// ---------------------------------------------------------------------------
// load / save
// ---------------------------------------------------------------------------
void SettingsDialog::loadFromSettings()
{
    const Identity identity = Application::instance()->identity()->identity();
    m_nameEdit->setText(identity.name);
    m_subtitleEdit->setText(identity.subtitle);
    m_organizationEdit->setText(identity.organization);
    m_iconEdit->setText(identity.iconPath);
    m_reportTitleEdit->setText(identity.reportTitle);
    m_reportFooterEdit->setText(identity.reportFooter);
    m_accent = identity.accent;
    m_accentSecondary = identity.accentSecondary;

    m_themeCombo->setCurrentIndex(
        m_themeCombo->findData(m_settings->value(QStringLiteral("appearance/theme"),
                                                 QStringLiteral("dark")).toString()));
    m_fontSizeCombo->setCurrentIndex(m_fontSizeCombo->findData(
        m_settings->value(QStringLiteral("appearance/fontSize"), 9).toInt()));
    m_showGridCheck->setChecked(
        m_settings->value(QStringLiteral("appearance/showGrid"), true).toBool());

    m_languageCombo->setCurrentIndex(
        m_languageCombo->findData(m_settings->value(QStringLiteral("language"), QStringLiteral("en")).toString()));
    if (m_languageCombo->currentIndex() < 0)
        m_languageCombo->setCurrentIndex(0);

    m_projectDirEdit->setText(
        m_settings->value(QStringLiteral("paths/projectDirectory"), Paths::defaultProjectDir()).toString());
    m_databaseEdit->setText(
        m_settings->value(QStringLiteral("paths/database"), Paths::databaseFile()).toString());
    m_logDirEdit->setText(m_settings->value(QStringLiteral("logging/directory"), Paths::logDir()).toString());
    m_backupDirEdit->setText(
        m_settings->value(QStringLiteral("paths/backups"), Paths::backupDir()).toString());

    m_providerCombo->setCurrentIndex(
        m_providerCombo->findData(m_settings->value(QStringLiteral("ai/provider"), QStringLiteral("none")).toString()));
    m_endpointEdit->setText(
        m_settings->value(QStringLiteral("ai/endpoint"), QStringLiteral("http://localhost:11434")).toString());
    m_modelEdit->setText(m_settings->value(QStringLiteral("ai/model")).toString());
    m_timeoutSpin->setValue(m_settings->value(QStringLiteral("ai/timeoutSeconds"), 120).toInt());
    m_maxTokensSpin->setValue(m_settings->value(QStringLiteral("ai/maxTokens"), 4096).toInt());

    m_allowNetworkCheck->setChecked(
        m_settings->value(QStringLiteral("privacy/allowNetwork"), false).toBool());
    m_allowCloudCheck->setChecked(
        m_settings->value(QStringLiteral("privacy/allowCloud"), false).toBool());
    m_onlineResearchCheck->setChecked(
        m_settings->value(QStringLiteral("privacy/onlineResearch"), false).toBool());
    m_sendBoardDataCheck->setChecked(
        m_settings->value(QStringLiteral("privacy/sendBoardData"), false).toBool());

    m_logLevelCombo->setCurrentIndex(
        m_logLevelCombo->findData(m_settings->value(QStringLiteral("logging/level"), QStringLiteral("info")).toString()));
    m_logHistorySpin->setValue(
        m_settings->value(QStringLiteral("logging/historyEntries"), 5000).toInt());

    m_importUnitsCombo->setCurrentIndex(
        m_importUnitsCombo->findData(m_settings->value(QStringLiteral("import/units"), QStringLiteral("mm")).toString()));
    m_importMissingRefsCheck->setChecked(
        m_settings->value(QStringLiteral("import/flagMissingReferences"), true).toBool());
    m_importSkipUnverifiedCheck->setChecked(
        m_settings->value(QStringLiteral("import/skipUnverifiedConnectivity"), true).toBool());

    refreshIdentityPreview();
}

void SettingsDialog::saveToSettings()
{
    Identity identity;
    identity.name = m_nameEdit->text();
    identity.subtitle = m_subtitleEdit->text();
    identity.organization = m_organizationEdit->text();
    identity.iconPath = m_iconEdit->text().trimmed();
    identity.accent = m_accent;
    identity.accentSecondary = m_accentSecondary;
    identity.reportTitle = m_reportTitleEdit->text();
    identity.reportFooter = m_reportFooterEdit->text();
    Application::instance()->identity()->setIdentity(identity);
    Application::instance()->identity()->save(m_settings);

    m_settings->setValue(QStringLiteral("appearance/theme"), m_themeCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("appearance/fontSize"), m_fontSizeCombo->currentData().toInt());
    m_settings->setValue(QStringLiteral("appearance/showGrid"), m_showGridCheck->isChecked());

    m_settings->setValue(QStringLiteral("language"), m_languageCombo->currentData().toString());

    m_settings->setValue(QStringLiteral("paths/projectDirectory"), m_projectDirEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("paths/database"), m_databaseEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("paths/backups"), m_backupDirEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("logging/directory"), m_logDirEdit->text().trimmed());

    m_settings->setValue(QStringLiteral("ai/provider"), m_providerCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("ai/endpoint"), m_endpointEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("ai/model"), m_modelEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("ai/timeoutSeconds"), m_timeoutSpin->value());
    m_settings->setValue(QStringLiteral("ai/maxTokens"), m_maxTokensSpin->value());

    m_settings->setValue(QStringLiteral("privacy/allowNetwork"), m_allowNetworkCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/allowCloud"), m_allowCloudCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/onlineResearch"), m_onlineResearchCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/sendBoardData"), m_sendBoardDataCheck->isChecked());

    m_settings->setValue(QStringLiteral("logging/level"), m_logLevelCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("logging/historyEntries"), m_logHistorySpin->value());

    m_settings->setValue(QStringLiteral("import/units"), m_importUnitsCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("import/flagMissingReferences"),
                         m_importMissingRefsCheck->isChecked());
    m_settings->setValue(QStringLiteral("import/skipUnverifiedConnectivity"),
                         m_importSkipUnverifiedCheck->isChecked());
}

void SettingsDialog::apply()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Settings"),
                             tr("The application name cannot be empty."));
        return;
    }

    saveToSettings();
    QString error;
    if (!m_settings->save(&error))
        QMessageBox::critical(this, tr("Settings"), error);

    Logger::instance().setLogDirectory(m_logDirEdit->text().trimmed());
    Logger::instance().setMinimumLevel(logLevelFromName(m_logLevelCombo->currentData().toString()));
    Logger::instance().setMaxHistoryEntries(m_logHistorySpin->value());

    logInfo(QStringLiteral("settings"), QStringLiteral("settings applied"));
    emit settingsChanged();
}

void SettingsDialog::onAccepted()
{
    apply();
    accept();
}

void SettingsDialog::resetDefaults()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Reset to defaults"),
        tr("Reset every setting to its default value? The application name, the directories and "
           "the AI configuration are all affected."));
    if (answer != QMessageBox::Yes)
        return;

    m_settings->reset();
    m_settings->save();
    Application::instance()->identity()->setIdentity(Identity());
    loadFromSettings();
    emit settingsChanged();
}

void SettingsDialog::chooseIcon()
{
    const QString path = browseFile(m_iconEdit->text(), tr("Choose an icon"),
                                    tr("Images (*.png *.svg *.svgz *.ico);;All files (*)"));
    if (!path.isEmpty())
        m_iconEdit->setText(path);
    refreshIdentityPreview();
}

void SettingsDialog::chooseColor(QLabel *swatch, QColor *target)
{
    const QColor color = QColorDialog::getColor(*target, this, tr("Choose colour"),
                                                QColorDialog::ShowAlphaChannel);
    if (!color.isValid())
        return;
    *target = color;
    swatch->setStyleSheet(QStringLiteral("background: %1;").arg(color.name()));
}

void SettingsDialog::refreshIdentityPreview()
{
    Identity preview;
    preview.name = m_nameEdit->text();
    preview.subtitle = m_subtitleEdit->text();
    preview.organization = m_organizationEdit->text();
    preview.iconPath = m_iconEdit->text().trimmed();
    preview.accent = m_accent;
    preview.accentSecondary = m_accentSecondary;

    AppIdentity temp;
    temp.setIdentity(preview);
    m_iconPreview->setPixmap(temp.icon().pixmap(32, 32));

    m_accentSwatch->setStyleSheet(QStringLiteral("background: %1;").arg(m_accent.name()));
    m_accentSecondarySwatch->setStyleSheet(
        QStringLiteral("background: %1;").arg(m_accentSecondary.name()));

    const QString provider = m_providerCombo ? m_providerCombo->currentData().toString() : QString();
    if (m_aiStatusLabel) {
        if (provider == QLatin1String("none")) {
            m_aiStatusLabel->setText(tr("No provider selected: the assistant stays offline."));
        } else if (!m_allowNetworkCheck || !m_allowNetworkCheck->isChecked()) {
            m_aiStatusLabel->setText(
                tr("Network access is disabled in the privacy settings, so only providers on this "
                   "machine can be reached."));
        } else {
            m_aiStatusLabel->setText(tr("Provider requests will go to %1.").arg(m_endpointEdit->text()));
        }
    }
}

QString SettingsDialog::browseDirectory(const QString &start, const QString &title)
{
    return QFileDialog::getExistingDirectory(this, title, start.isEmpty() ? QDir::homePath() : start);
}

QString SettingsDialog::browseFile(const QString &start, const QString &title, const QString &filter)
{
    return QFileDialog::getOpenFileName(this, title, start.isEmpty() ? QDir::homePath() : start,
                                        filter);
}

} // namespace eda

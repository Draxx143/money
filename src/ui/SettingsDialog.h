// Settings dialog: application identity, appearance, language, directories,
// AI providers, privacy, database, logging and import options.
//
// Every page writes into the Settings object; nothing else in the application
// talks to QSettings. Changing the identity or the accent colour takes effect
// immediately, and the dialog never requires a restart.
#pragma once

#include <QDialog>

#include "../application/AppIdentity.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QListWidget;
class QSpinBox;
class QStackedWidget;

namespace eda {

class Application;
class Database;
class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // The enumerators carry the Page prefix so that they cannot shadow the
    // type names they describe (Identity, Logging, ... are classes too).
    enum Page { PageIdentity = 0, PageAppearance, PageLanguage, PageDirectories,
                PageAiProvider, PagePrivacy, PageDatabase, PageLogging,
                PageImport, PageCount };

    void setCurrentPage(int page);

signals:
    void settingsChanged();

private slots:
    void apply();
    void onAccepted();
    void chooseIcon();
    void chooseColor(QLabel *swatch, QColor *target);
    void resetDefaults();
    void refreshIdentityPreview();

private:
    QWidget *createIdentityPage();
    QWidget *createAppearancePage();
    QWidget *createLanguagePage();
    QWidget *createDirectoriesPage();
    QWidget *createAiPage();
    QWidget *createPrivacyPage();
    QWidget *createDatabasePage();
    void refreshDatabaseStatus();
    QWidget *createLoggingPage();
    QWidget *createImportPage();

    void loadFromSettings();
    void saveToSettings();
    QString browseDirectory(const QString &start, const QString &title);
    QString browseFile(const QString &start, const QString &title, const QString &filter);

    Settings *m_settings = nullptr;
    QListWidget *m_pageList = nullptr;
    QStackedWidget *m_pages = nullptr;

    // identity
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_subtitleEdit = nullptr;
    QLineEdit *m_organizationEdit = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QLineEdit *m_reportTitleEdit = nullptr;
    QLineEdit *m_reportFooterEdit = nullptr;
    QLabel *m_accentSwatch = nullptr;
    QLabel *m_accentSecondarySwatch = nullptr;
    QLabel *m_iconPreview = nullptr;
    QColor m_accent;
    QColor m_accentSecondary;

    // appearance / language
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_fontSizeCombo = nullptr;
    QCheckBox *m_showGridCheck = nullptr;

    // directories
    QLineEdit *m_projectDirEdit = nullptr;
    QLineEdit *m_databaseEdit = nullptr;
    QLabel *m_databaseStatus = nullptr;
    QPushButton *m_databaseCheck = nullptr;
    QPushButton *m_databaseCompact = nullptr;
    QLineEdit *m_logDirEdit = nullptr;
    QLineEdit *m_backupDirEdit = nullptr;

    // ai
    QComboBox *m_providerCombo = nullptr;
    QLineEdit *m_endpointEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
    QSpinBox *m_maxTokensSpin = nullptr;
    QLabel *m_aiStatusLabel = nullptr;

    // privacy
    QCheckBox *m_allowNetworkCheck = nullptr;
    QCheckBox *m_allowCloudCheck = nullptr;
    QCheckBox *m_onlineResearchCheck = nullptr;
    QCheckBox *m_sendBoardDataCheck = nullptr;

    // logging / import
    QComboBox *m_logLevelCombo = nullptr;
    QSpinBox *m_logHistorySpin = nullptr;
    QComboBox *m_importUnitsCombo = nullptr;
    QCheckBox *m_importMissingRefsCheck = nullptr;
    QCheckBox *m_importSkipUnverifiedCheck = nullptr;
};

} // namespace eda

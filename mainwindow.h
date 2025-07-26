#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QGridLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QSettings>
#include <QKeySequence>
#include <QShortcut>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>

#include "screenshotcapture.h"
#include "selectionoverlay.h"
#include "screenshotpreview.h"
#include "globalhotkey.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QTextEdit;
QT_END_NAMESPACE

class SelectionOverlay;
class ScreenshotCapture;
class ScreenshotPreview;
class GlobalHotkey;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartScrollScreenshot();
    void onStopScrollScreenshot();
    void onSelectionConfirmed(const QRect& rect);
    void onSelectionCancelled();
    void onCaptureStatusChanged(const QString& status);
    void onNewImageCaptured(const QPixmap& image);
    void onCaptureFinished(const QPixmap& combinedImage);
    void onCaptureFinishedFromOverlay();  // 来自选择覆盖层的完成信号
    void onSaveRequested();
    void onPreviewCloseRequested();
    void onIntervalChanged(int value);
    void onStartupDelayFinished();
    void onHotkeyTriggered();  // 快捷键触发
    void onShowSettings();     // 显示设置对话框

private:
    void setupUI();
    void setupConnections();
    void updateStatus(const QString& message);
    void enableControls(bool enabled);
    void saveScreenshot();
    void logMessage(const QString& message);
    void hideToolWindows();
    void showToolWindows();
    void startCaptureWithDelay();
    void showPreviewOutsideCaptureArea();
    void loadSettings();
    void saveSettings();
    void setupHotkey();
    void createMenuBar();

    // UI 组件
    QWidget *m_centralWidget;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QSpinBox *m_intervalSpinBox;
    QSpinBox *m_delaySpinBox;
    QLabel *m_intervalLabel;
    QLabel *m_statusLabel;
    QTextEdit *m_logTextEdit;
    QTimer *m_startupDelayTimer;
    
    SelectionOverlay *m_selectionOverlay;
    ScreenshotCapture *m_screenshotCapture;
    ScreenshotPreview *m_previewWindow;
    
    QRect m_selectedRect;
    bool m_isCapturing;
    int m_startupDelaySeconds;
    QString m_lastSavePath;
    
    // 快捷键相关
    GlobalHotkey *m_globalHotkey;
    QKeySequence m_hotkeySequence;
    
    // 设置相关
    QSettings *m_settings;
    bool m_hotkeyEnabled;
};

#endif // MAINWINDOW_H

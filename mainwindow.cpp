#include "mainwindow.h"
#include "selectionoverlay.h"
#include "screenshotcapture.h"
#include "screenshotpreview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QApplication>
#include <QScreen>
#include <QSettings>
#include <QKeySequence>
#include <QShortcut>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QKeySequenceEdit>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>
#include <QTextCursor>
#include <QDebug> // Added for qDebug

// 设置对话框类
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("RabbitShot 设置");
        setModal(true);
        resize(400, 300);
        
        setupUI();
    }
    
    QKeySequence getHotkey() const { return m_hotkeyEdit->keySequence(); }
    bool isHotkeyEnabled() const { return m_hotkeyCheckBox->isChecked(); }
    
    void setHotkey(const QKeySequence& sequence) { m_hotkeyEdit->setKeySequence(sequence); }
    void setHotkeyEnabled(bool enabled) { m_hotkeyCheckBox->setChecked(enabled); }

private:
    void setupUI()
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        
        // 快捷键设置组
        QGroupBox *hotkeyGroup = new QGroupBox("快捷键设置");
        QFormLayout *hotkeyLayout = new QFormLayout(hotkeyGroup);
        
        m_hotkeyCheckBox = new QCheckBox("启用快捷键截图");
        m_hotkeyEdit = new QKeySequenceEdit();
        m_hotkeyEdit->setKeySequence(QKeySequence("Ctrl+Shift+A"));
        
        hotkeyLayout->addRow(m_hotkeyCheckBox, m_hotkeyEdit);
        
        // 说明文字
        QLabel *hotkeyHint = new QLabel("快捷键将直接启动区域选择，无需点击开始按钮");
        hotkeyHint->setWordWrap(true);
        hotkeyHint->setStyleSheet("color: gray; font-size: 11px;");
        
        mainLayout->addWidget(hotkeyGroup);
        mainLayout->addWidget(hotkeyHint);
        mainLayout->addStretch();
        
        // 按钮
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        mainLayout->addWidget(buttonBox);
    }
    
    QCheckBox *m_hotkeyCheckBox;
    QKeySequenceEdit *m_hotkeyEdit;
};

#include "mainwindow.moc"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_selectionOverlay(nullptr)
    , m_screenshotCapture(nullptr)
    , m_previewWindow(nullptr)
    , m_isCapturing(false)
    , m_startupDelaySeconds(3)
    , m_globalHotkey(nullptr)
    , m_hotkeyEnabled(true)
    , m_settings(new QSettings("RabbitShot", "RabbitShot", this))
{
    setWindowTitle("RabbitShot - 滚动截图工具");
    resize(600, 500);
    
    setupUI();
    
    // 创建功能类
    m_screenshotCapture = new ScreenshotCapture(this);
    m_selectionOverlay = new SelectionOverlay(this);
    m_previewWindow = new ScreenshotPreview(this);
    m_globalHotkey = new GlobalHotkey(this);
    
    // 创建定时器
    m_startupDelayTimer = new QTimer(this);
    m_startupDelayTimer->setSingleShot(true);
    
    setupConnections();
    createMenuBar();
    loadSettings();
    setupHotkey();
    
    // 设置保存路径
    m_lastSavePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    
    updateStatus("就绪 - 点击开始截图或使用快捷键 Ctrl+Shift+A");
    logMessage("RabbitShot 已启动，使用 Ctrl+Shift+A 快捷键快速截图");
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    
    // 控制区域
    QHBoxLayout *controlLayout = new QHBoxLayout();
    
    m_startButton = new QPushButton("开始截图", this);
    m_stopButton = new QPushButton("停止截图", this);
    m_stopButton->setEnabled(false);
    
    controlLayout->addWidget(m_startButton);
    controlLayout->addWidget(m_stopButton);
    controlLayout->addStretch();
    
    mainLayout->addLayout(controlLayout);
    
    // 参数设置区域
    QHBoxLayout *paramLayout = new QHBoxLayout();
    
    m_intervalLabel = new QLabel("检测间隔:", this);
    m_intervalSpinBox = new QSpinBox(this);
    m_intervalSpinBox->setRange(50, 1000);
    m_intervalSpinBox->setValue(100);
    m_intervalSpinBox->setSuffix(" ms");
    
    QLabel *delayLabel = new QLabel("启动延迟:", this);
    m_delaySpinBox = new QSpinBox(this);
    m_delaySpinBox->setRange(1, 10);
    m_delaySpinBox->setValue(m_startupDelaySeconds);
    m_delaySpinBox->setSuffix(" s");
    
    paramLayout->addWidget(m_intervalLabel);
    paramLayout->addWidget(m_intervalSpinBox);
    paramLayout->addWidget(delayLabel);
    paramLayout->addWidget(m_delaySpinBox);
    paramLayout->addStretch();
    
    mainLayout->addLayout(paramLayout);
    
    // 状态显示
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    mainLayout->addWidget(m_statusLabel);
    
    // 日志区域
    QLabel *logLabel = new QLabel("运行日志:", this);
    mainLayout->addWidget(logLabel);
    
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setMaximumHeight(200);
    m_logTextEdit->setReadOnly(true);
    mainLayout->addWidget(m_logTextEdit);
}

void MainWindow::setupConnections()
{
    // 按钮连接
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartScrollScreenshot);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopScrollScreenshot);
    
    // 参数连接
    connect(m_intervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onIntervalChanged);
    connect(m_delaySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_startupDelaySeconds = value;
    });
    
    // 定时器连接
    connect(m_startupDelayTimer, &QTimer::timeout, this, &MainWindow::onStartupDelayFinished);
    
    // 截图捕获连接
    connect(m_screenshotCapture, &ScreenshotCapture::captureStatusChanged, this, &MainWindow::onCaptureStatusChanged);
    connect(m_screenshotCapture, &ScreenshotCapture::newImageCaptured, this, &MainWindow::onNewImageCaptured);
    connect(m_screenshotCapture, &ScreenshotCapture::captureFinished, this, &MainWindow::onCaptureFinished);
    connect(m_screenshotCapture, &ScreenshotCapture::scrollDetected, this, [this](ScrollDirection direction, int offset) {
        QString dirStr = (direction == ScrollDirection::Down) ? "向下" : "向上";
        logMessage(QString("检测到滚动：%1，偏移：%2px").arg(dirStr).arg(offset));
    });
    
    // 选择覆盖层连接
    connect(m_selectionOverlay, &SelectionOverlay::selectionConfirmed, this, &MainWindow::onSelectionConfirmed);
    connect(m_selectionOverlay, &SelectionOverlay::selectionCancelled, this, &MainWindow::onSelectionCancelled);
    connect(m_selectionOverlay, &SelectionOverlay::captureFinished, this, &MainWindow::onCaptureFinishedFromOverlay);
    connect(m_selectionOverlay, &SelectionOverlay::saveRequested, this, &MainWindow::onSaveRequested);  // 添加保存请求连接
    
    // 预览窗口连接
    connect(m_previewWindow, &ScreenshotPreview::saveRequested, this, &MainWindow::onSaveRequested);
    connect(m_previewWindow, &ScreenshotPreview::closeRequested, this, &MainWindow::onPreviewCloseRequested);
    
    // 全局快捷键连接
    connect(m_globalHotkey, &GlobalHotkey::activated, this, &MainWindow::onHotkeyTriggered);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    QAction *exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // 工具菜单
    QMenu *toolsMenu = menuBar->addMenu("工具(&T)");
    QAction *settingsAction = toolsMenu->addAction("设置(&S)");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onShowSettings);
    
    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    QAction *aboutAction = helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "关于 RabbitShot", 
            "RabbitShot - 滚动截图工具\n\n"
            "版本: 1.0\n"
            "支持智能滚动检测和图像拼接\n\n"
            "快捷键: Ctrl+Shift+A (可在设置中修改)");
    });
}

void MainWindow::loadSettings()
{
    // 加载快捷键设置
    QString hotkeyStr = m_settings->value("hotkey", "Ctrl+Shift+A").toString();
    m_hotkeySequence = QKeySequence(hotkeyStr);
    m_hotkeyEnabled = m_settings->value("hotkeyEnabled", true).toBool();
    
    // 加载其他设置
    m_startupDelaySeconds = m_settings->value("startupDelay", 3).toInt();
    m_intervalSpinBox->setValue(m_settings->value("detectionInterval", 100).toInt());
    m_delaySpinBox->setValue(m_startupDelaySeconds);
}

void MainWindow::saveSettings()
{
    // 保存快捷键设置
    m_settings->setValue("hotkey", m_hotkeySequence.toString());
    m_settings->setValue("hotkeyEnabled", m_hotkeyEnabled);
    
    // 保存其他设置
    m_settings->setValue("startupDelay", m_startupDelaySeconds);
    m_settings->setValue("detectionInterval", m_intervalSpinBox->value());
    m_settings->sync();
}

void MainWindow::setupHotkey()
{
    if (!m_globalHotkey) {
        return;
    }
    
    // 先取消注册现有的快捷键
    m_globalHotkey->unregisterHotkey();
    
    if (m_hotkeyEnabled && !m_hotkeySequence.isEmpty()) {
        if (m_globalHotkey->registerHotkey(m_hotkeySequence)) {
            logMessage(QString("全局快捷键已设置: %1").arg(m_hotkeySequence.toString()));
            
            // 调试信息
            qDebug() << "快捷键设置完成:" << m_hotkeySequence.toString();
        } else {
            logMessage(QString("全局快捷键设置失败: %1").arg(m_hotkeySequence.toString()));
        }
    } else {
        logMessage("快捷键已禁用");
    }
}

void MainWindow::onHotkeyTriggered()
{
    qDebug() << "快捷键被触发!";
    logMessage("快捷键触发，开始区域选择");
    
    // 确保窗口显示并激活
    show();
    raise();
    activateWindow();
    
    onStartScrollScreenshot();
}

void MainWindow::onShowSettings()
{
    SettingsDialog dialog(this);
    dialog.setHotkey(m_hotkeySequence);
    dialog.setHotkeyEnabled(m_hotkeyEnabled);
    
    if (dialog.exec() == QDialog::Accepted) {
        m_hotkeySequence = dialog.getHotkey();
        m_hotkeyEnabled = dialog.isHotkeyEnabled();
        
        setupHotkey();
        saveSettings();
        
        logMessage(QString("设置已保存，快捷键: %1").arg(m_hotkeySequence.toString()));
    }
}

void MainWindow::onStartScrollScreenshot()
{
    // 每次开始截图都清空选择区域，强制用户重新选择范围
    m_selectedRect = QRect();
    
    if (m_selectedRect.isEmpty()) {
        // 如果没有选择区域，启动选择覆盖层
        logMessage("启动区域选择模式");
        m_selectionOverlay->startSelection();
        return;
    }
    
    if (m_isCapturing) {
        return;
    }
    
    m_isCapturing = true;
    enableControls(false);
    
    // 显示延迟提示
    updateStatus(QString("准备开始截图，%1秒后开始...").arg(m_startupDelaySeconds));
    logMessage(QString("开始截图倒计时：%1秒").arg(m_startupDelaySeconds));
    
    // 启动延迟定时器
    m_startupDelayTimer->start(m_startupDelaySeconds * 1000);
}

void MainWindow::onStartupDelayFinished()
{
    if (!m_isCapturing) {
        return;
    }
    
    // 隐藏工具窗口
    hideToolWindows();
    
    // 延迟一小段时间确保窗口完全隐藏
    QTimer::singleShot(100, this, &MainWindow::startCaptureWithDelay);
}

void MainWindow::startCaptureWithDelay()
{
    if (!m_isCapturing) {
        return;
    }
    
    // 现在通过调整截图区域避开红色边框，不再频繁隐藏/显示覆盖层
    
    // 设置截图区域和参数
    m_screenshotCapture->setCapturezone(m_selectedRect);
    m_screenshotCapture->setDetectionInterval(m_intervalSpinBox->value());
    
    // 开始捕获
    m_screenshotCapture->startScrollCapture();
    
    // 安装应用级事件过滤器，监听滚轮事件来更及时地触发拼接
    qApp->installEventFilter(m_screenshotCapture);
    
    // 预览窗口已经在选择确认时显示，这里不需要再次显示
    // showPreviewOutsideCaptureArea();
    
    logMessage("开始滚动截图，截图区域已自动调整避开边框");
}

void MainWindow::onStopScrollScreenshot()
{
    if (!m_isCapturing) {
        return;
    }
    
    // 移除事件过滤器，避免在非截图状态下拦截事件
    qApp->removeEventFilter(m_screenshotCapture);

    m_screenshotCapture->stopScrollCapture();
    
    m_isCapturing = false;
    enableControls(true);
    
    // 选择覆盖层的显示现在通过信号机制自动管理
    
    // 显示工具窗口
    showToolWindows();
    
    // 确保预览窗口显示最终结果
    QPixmap finalImage = m_screenshotCapture->getCombinedImage();
    if (!finalImage.isNull() && m_previewWindow) {
        m_previewWindow->setFinalImage(finalImage);
        if (!m_previewWindow->isVisible()) {
            m_previewWindow->show();
            m_previewWindow->raise();
        }
    }
    
    // 清空选择区域，确保下次开始截图时重新选择范围
    m_selectedRect = QRect();
    
    logMessage("停止滚动截图，预览显示最终结果，已清空选择区域");
}

void MainWindow::onSelectionConfirmed(const QRect& rect)
{
    m_selectedRect = rect;
    
    // 立即显示预览窗口在选择框旁边
    showPreviewOutsideCaptureArea();
    
    // 立即开始滚动截图，不等待延迟
    if (!m_isCapturing) {
        m_isCapturing = true;
        enableControls(false);
        
        updateStatus("立即开始截图...");
        logMessage("区域已选择，立即开始截图");
        
        // 隐藏工具窗口
        hideToolWindows();
        
        // 立即开始截图，不使用延迟定时器
        QTimer::singleShot(100, this, &MainWindow::startCaptureWithDelay);
    }
}

void MainWindow::onSelectionCancelled()
{
    m_selectedRect = QRect();
    
    // 如果正在截图，也要停止
    if (m_isCapturing) {
        onStopScrollScreenshot();
    }
    
    // 隐藏预览窗口，因为没有选择区域了
    if (m_previewWindow) {
        m_previewWindow->hide();
    }
    
    updateStatus("区域选择已取消");
    logMessage("区域选择已取消，预览窗口已隐藏");
}

void MainWindow::onCaptureStatusChanged(const QString& status)
{
    updateStatus(status);
    
    // 如果状态显示开始监听滚动或截图进行中，且预览窗口可见，更新预览内容
    if ((status.contains("监听滚动") || status.contains("截图")) && 
        m_previewWindow && m_previewWindow->isVisible()) {
        QPixmap currentCombined = m_screenshotCapture->getCurrentCombinedImage();
        if (!currentCombined.isNull()) {
            m_previewWindow->updateRealTimePreview(currentCombined);
            logMessage("状态变更时更新预览");
        }
    }
}

void MainWindow::onNewImageCaptured(const QPixmap& image)
{
    Q_UNUSED(image)
    
    // 获取当前拼接的图片并显示在预览窗口
    QPixmap currentCombined = m_screenshotCapture->getCurrentCombinedImage();
    if (!currentCombined.isNull()) {
        // 更新预览窗口的实时预览
        m_previewWindow->updateRealTimePreview(currentCombined);
        
        // 确保预览窗口可见
        if (!m_previewWindow->isVisible()) {
            m_previewWindow->show();
            m_previewWindow->raise();
        }
    }
    
    logMessage("捕获新图片片段，预览已更新");
}

void MainWindow::onCaptureFinished(const QPixmap& combinedImage)
{
    // 截图完成后显示最终结果
    m_previewWindow->setFinalImage(combinedImage);
    m_previewWindow->show();
    m_previewWindow->raise();
    m_previewWindow->activateWindow();
    
    // 自动保存
    // if (m_autoSaveCheckBox->isChecked()) {
    //     saveScreenshot();
    // }
    
    logMessage(QString("截图完成，尺寸：%1x%2").arg(combinedImage.width()).arg(combinedImage.height()));
}

void MainWindow::onCaptureFinishedFromOverlay()
{
    // 来自选择覆盖层的完成信号，停止截图捕获
    if (m_isCapturing) {
        onStopScrollScreenshot();
    }
    
    // 清空选择区域，确保下次开始截图时重新选择范围
    m_selectedRect = QRect();
    logMessage("截图完成，已清空选择区域，下次将重新选择范围");
}

void MainWindow::onSaveRequested()
{
    saveScreenshot();
}

void MainWindow::onPreviewCloseRequested()
{
    if (m_isCapturing) {
        onStopScrollScreenshot();
    }
}

void MainWindow::onIntervalChanged(int value)
{
    if (m_screenshotCapture) {
        m_screenshotCapture->setDetectionInterval(value);
    }
}

void MainWindow::updateStatus(const QString& status)
{
    m_statusLabel->setText(status);
    logMessage(status);
}

void MainWindow::enableControls(bool enable)
{
    m_startButton->setEnabled(enable);
    m_intervalSpinBox->setEnabled(enable);
    m_delaySpinBox->setEnabled(enable);
    m_stopButton->setEnabled(!enable);
}

void MainWindow::hideToolWindows()
{
    // 隐藏主窗口
    hide();
    
    // 不隐藏预览窗口，让它在截图期间保持显示
    // m_previewWindow->hide();
    
    logMessage("隐藏工具窗口（保持预览窗口显示）");
}

void MainWindow::showToolWindows()
{
    // 显示主窗口
    show();
    raise();
    activateWindow();
    
    logMessage("显示工具窗口");
}

void MainWindow::saveScreenshot()
{
    QPixmap finalImage = m_screenshotCapture->getCombinedImage();
    
    // 如果没有最终图片，但正在捕获中，尝试获取当前的实时拼接图片
    if (finalImage.isNull() && m_isCapturing) {
        finalImage = m_screenshotCapture->getCurrentCombinedImage();
        logMessage("保存当前截图进度");
    }
    
    if (finalImage.isNull()) {
        QMessageBox::warning(this, "警告", "没有可保存的截图");
        logMessage("保存失败：没有可用的截图数据");
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString defaultName = QString("screenshot_%1.png").arg(timestamp);
    QString filePath = QFileDialog::getSaveFileName(
        this, 
        "保存截图", 
        m_lastSavePath + "/" + defaultName,
        "PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*)"
    );
    
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        m_lastSavePath = fileInfo.absolutePath();
        
        if (finalImage.save(filePath)) {
            logMessage(QString("截图已保存: %1，尺寸: %2x%3").arg(filePath).arg(finalImage.width()).arg(finalImage.height()));
            QMessageBox::information(this, "成功", "截图保存成功！");
            
            // 询问是否打开文件位置
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "打开位置", "是否打开文件所在位置？",
                QMessageBox::Yes | QMessageBox::No
            );
            
            if (reply == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
            }
        } else {
            logMessage("保存截图失败");
            QMessageBox::critical(this, "错误", "保存截图失败！");
        }
    }
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logTextEdit->append(QString("[%1] %2").arg(timestamp).arg(message));
    
    // 自动滚动到底部
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

void MainWindow::showPreviewOutsideCaptureArea()
{
    if (m_selectedRect.isEmpty()) {
        m_previewWindow->showPreview(QRect());
        return;
    }
    
    // 获取屏幕尺寸
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) return;
    
    QRect screenRect = screen->geometry();
    QSize previewSize(400, 600);  // 预览窗口的默认尺寸
    
    // 计算可能的位置（优先级：右侧、左侧、下方、上方）
    QPoint newPos;
    
    // 尝试放在截图区域右侧
    if (m_selectedRect.right() + 20 + previewSize.width() <= screenRect.right()) {
        newPos = QPoint(m_selectedRect.right() + 20, m_selectedRect.top());
    }
    // 尝试放在截图区域左侧  
    else if (m_selectedRect.left() - 20 - previewSize.width() >= screenRect.left()) {
        newPos = QPoint(m_selectedRect.left() - 20 - previewSize.width(), m_selectedRect.top());
    }
    // 尝试放在截图区域下方
    else if (m_selectedRect.bottom() + 20 + previewSize.height() <= screenRect.bottom()) {
        newPos = QPoint(m_selectedRect.left(), m_selectedRect.bottom() + 20);
    }
    // 尝试放在截图区域上方
    else if (m_selectedRect.top() - 20 - previewSize.height() >= screenRect.top()) {
        newPos = QPoint(m_selectedRect.left(), m_selectedRect.top() - 20 - previewSize.height());
    }
    // 如果都不行，放在屏幕右下角
    else {
        newPos = QPoint(screenRect.right() - previewSize.width() - 20, 
                       screenRect.bottom() - previewSize.height() - 20);
    }
    
    // 确保位置在屏幕范围内
    newPos.setX(qMax(screenRect.left(), qMin(newPos.x(), screenRect.right() - previewSize.width())));
    newPos.setY(qMax(screenRect.top(), qMin(newPos.y(), screenRect.bottom() - previewSize.height())));
    
    // 设置预览窗口位置和大小
    m_previewWindow->resize(previewSize);
    m_previewWindow->move(newPos);
    m_previewWindow->showPreview(m_selectedRect);
    
    // 如果已经有拼接的图片，立即显示
    QPixmap currentCombined = m_screenshotCapture->getCurrentCombinedImage();
    if (!currentCombined.isNull()) {
        m_previewWindow->updateRealTimePreview(currentCombined);
    }
    
    logMessage(QString("预览窗口已移动到截图区域外: (%1, %2)").arg(newPos.x()).arg(newPos.y()));
}

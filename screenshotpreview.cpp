#include "screenshotpreview.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDebug>

ScreenshotPreview::ScreenshotPreview(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_scrollArea(nullptr)
    , m_imageLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_progressBar(nullptr)
    , m_isCapturing(false)
    , m_imageCount(0)
{
    setupUI();
    
    // 设置窗口属性
    setWindowTitle("截图预览");
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // 设置窗口大小和位置
    resize(400, 600);
    
    // 移动到屏幕右侧
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    move(screenGeometry.right() - width() - 50, screenGeometry.top() + 100);
}

ScreenshotPreview::~ScreenshotPreview()
{
}

void ScreenshotPreview::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // 信息标签
    m_infoLabel = new QLabel("截图预览", this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setStyleSheet(
        "QLabel { "
        "font-size: 14px; "
        "font-weight: bold; "
        "padding: 8px; "
        "background-color: #f0f0f0; "
        "border: 1px solid #ddd; "
        "border-radius: 4px; "
        "}"
    );
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    
    // 滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 图片标签
    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        "QLabel { "
        "background-color: white; "
        "border: 1px solid #ddd; "
        "}"
    );
    m_imageLabel->setText("等待截图...");
    
    m_scrollArea->setWidget(m_imageLabel);
    
    // 主布局 - 移除按钮布局
    m_mainLayout->addWidget(m_infoLabel);
    m_mainLayout->addWidget(m_progressBar);
    m_mainLayout->addWidget(m_scrollArea);
    
    // 不再创建和连接按钮
    updateButtons();
}

void ScreenshotPreview::showPreview(const QRect& captureRect)
{
    m_captureRect = captureRect;
    m_isCapturing = true;
    m_imageCount = 0;
    
    m_infoLabel->setText(QString("捕获区域: %1x%2").arg(captureRect.width()).arg(captureRect.height()));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // 无限进度条
    
    clearPreview();
    updateButtons();
    show();
    raise();
    activateWindow();
}

void ScreenshotPreview::hidePreview()
{
    hide();
    m_isCapturing = false;
    m_progressBar->setVisible(false);
}

void ScreenshotPreview::updatePreview(const QList<QPixmap>& images)
{
    m_capturedImages = images;
    m_imageCount = images.size();
    
    updateImageDisplay();
    updateButtons();
    
    // 更新信息标签
    m_infoLabel->setText(QString("已捕获 %1 张图片").arg(m_imageCount));
}

void ScreenshotPreview::updateRealTimePreview(const QPixmap& image)
{
    if (!image.isNull()) {
        // 缩放图片以适应预览，但保持原始尺寸信息
        QSize previewSize = m_scrollArea->size() - QSize(20, 20);
        QPixmap scaledImage = image.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        m_imageLabel->setPixmap(scaledImage);
        m_imageLabel->resize(scaledImage.size());
        
        // 更新信息标签显示实时进度
        m_infoLabel->setText(QString("实时预览 - 当前尺寸: %1x%2").arg(image.width()).arg(image.height()));
        
        // 在实时预览期间，启用保存按钮
        // m_saveButton->setEnabled(true); // This line is removed as per the edit hint
    }
}

void ScreenshotPreview::setFinalImage(const QPixmap& image)
{
    m_finalImage = image;
    m_isCapturing = false;
    m_progressBar->setVisible(false);
    
    if (!image.isNull()) {
        // 缩放图片以适应预览
        QSize previewSize = m_scrollArea->size() - QSize(20, 20);
        QPixmap scaledImage = image.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaledImage);
        m_imageLabel->resize(scaledImage.size());
        
        m_infoLabel->setText(QString("截图完成！尺寸: %1x%2").arg(image.width()).arg(image.height()));
        
        // 确保保存按钮在最终完成时也是启用的
        // m_saveButton->setEnabled(true); // This line is removed as per the edit hint
    }
    
    updateButtons();
}

void ScreenshotPreview::clearPreview()
{
    m_capturedImages.clear();
    m_finalImage = QPixmap();
    m_imageCount = 0;
    
    m_imageLabel->clear();
    m_imageLabel->setText("等待截图...");
    m_infoLabel->setText("截图预览");
    
    updateButtons();
}

void ScreenshotPreview::updateImageDisplay()
{
    if (m_capturedImages.isEmpty()) {
        m_imageLabel->setText("等待截图...");
        return;
    }
    
    // 合并图片并显示
    QPixmap combinedImage = combineImages(m_capturedImages);
    
    if (!combinedImage.isNull()) {
        // 缩放图片以适应预览窗口
        QPixmap scaledImage = combinedImage.scaled(m_scrollArea->size() - QSize(20, 20), 
                                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaledImage);
        m_imageLabel->resize(scaledImage.size());
    }
}

void ScreenshotPreview::updateButtons()
{
    bool hasImages = !m_capturedImages.isEmpty() || !m_finalImage.isNull();
    bool hasValidFinalImage = !m_finalImage.isNull();
    bool hasValidPreview = !m_imageLabel->pixmap().isNull();
    
    // 如果有最终图片或者有有效的预览图片，就启用保存按钮
    // m_saveButton->setEnabled(hasValidFinalImage || hasValidPreview); // This line is removed as per the edit hint
    // m_clearButton->setEnabled(hasImages || hasValidPreview); // This line is removed as per the edit hint
}

QPixmap ScreenshotPreview::combineImages(const QList<QPixmap>& images)
{
    if (images.isEmpty()) {
        return QPixmap();
    }
    
    if (images.size() == 1) {
        return images.first();
    }
    
    // 计算合并后的尺寸
    int maxWidth = 0;
    int totalHeight = 0;
    
    for (const QPixmap& img : images) {
        maxWidth = qMax(maxWidth, img.width());
        totalHeight += img.height();
    }
    
    // 创建合并后的图片
    QPixmap combinedPixmap(maxWidth, totalHeight);
    combinedPixmap.fill(Qt::white);
    
    QPainter painter(&combinedPixmap);
    int currentY = 0;
    
    for (const QPixmap& img : images) {
        painter.drawPixmap(0, currentY, img);
        currentY += img.height();
    }
    
    return combinedPixmap;
} 
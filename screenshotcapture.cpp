#include "screenshotcapture.h"
#include <QPainter>
#include <QDateTime>
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <cmath>
#include <algorithm>
#include <QCryptographicHash> // Added for content fingerprinting
#include <QThread>            // Added for msleep function

// 静态常量定义
const int ScreenshotCapture::DEFAULT_DETECTION_INTERVAL;
const int ScreenshotCapture::MIN_SCROLL_DISTANCE;
const int ScreenshotCapture::OVERLAP_SEARCH_HEIGHT;

ScreenshotCapture::ScreenshotCapture(QObject *parent)
    : QObject(parent)
    , m_detectionTimer(nullptr)
    , m_primaryScreen(nullptr)
    , m_isCapturing(false)
    , m_captureCount(0)
    , m_detectionInterval(DEFAULT_DETECTION_INTERVAL)
    , m_hashSampleStep(2)           // 默认采样步长
    , m_maxCoveredRegions(200)      // 最大覆盖区域数量
    , m_lastCleanupTime(0)
    , m_duplicateSkipCount(0)
    , m_consecutiveDuplicates(0)
    , m_lastDuplicateTime(0)
{
    m_primaryScreen = QApplication::primaryScreen();
    
    if (!m_primaryScreen) {
        qDebug() << "警告：无法获取主屏幕";
        emit captureStatusChanged("错误：无法获取主屏幕");
        return;
    }
    
    qDebug() << "主屏幕信息：" << m_primaryScreen->name() << "尺寸：" << m_primaryScreen->size();
    
    m_detectionTimer = new QTimer(this);
    connect(m_detectionTimer, &QTimer::timeout, this, &ScreenshotCapture::onScrollDetectionTimer);
}

ScreenshotCapture::~ScreenshotCapture()
{
    stopScrollCapture();
}

void ScreenshotCapture::setCapturezone(const QRect& rect)
{
    m_captureRect = rect;
    qDebug() << "设置截图区域:" << rect;
}

void ScreenshotCapture::startScrollCapture()
{
    if (m_isCapturing || m_captureRect.isEmpty()) {
        // 移除无用调试输出
        return;
    }
    
    if (!m_primaryScreen) {
        emit captureStatusChanged("错误：无法访问屏幕");
        return;
    }
    
    clearCapturedImages();
    m_isCapturing = true;
    m_captureCount = 0;
    
    // 测试截图权限
    QPixmap testCapture = m_primaryScreen->grabWindow(0, 0, 0, 100, 100);
    
    if (testCapture.isNull()) {
        // 权限错误保留输出
        emit captureStatusChanged("错误：无法截图，请检查屏幕录制权限");
        m_isCapturing = false;
        
        // 显示权限提示
        QMessageBox::warning(nullptr, "权限错误", 
                           "无法进行屏幕截图！\n\n"
                           "请确保：\n"
                           "1. 在系统设置 → 隐私与安全性 → 屏幕录制中\n"
                           "2. 已勾选 RabbitShot.app\n"
                           "3. 重启应用程序\n\n"
                           "如果已经授权，请重启应用程序。");
        return;
    }
    
    // 捕获初始图片作为基础
    m_baseImage = captureRegion(m_captureRect);
    m_lastScreenshot = m_baseImage;
    
    if (!m_baseImage.isNull()) {
        // 初始化基础图片的段信息
        ContentSegment baseSegment;
        baseSegment.image = m_baseImage;
        baseSegment.yOffset = 0;
        baseSegment.overlapHeight = 0;
        baseSegment.isBaseImage = true;
        m_segments.append(baseSegment);
        
        // 初始化全局区域管理
        m_globalBounds = QRect(0, 0, m_baseImage.width(), m_baseImage.height());
        m_currentScrollPos = m_baseImage.height();
        
        // 将基础图片添加到全局区域
        QRect baseRect = QRect(0, 0, m_baseImage.width(), m_baseImage.height());
        updateGlobalRegion(m_baseImage, baseRect);
        
        // 将基础图片记录到已覆盖区域（重要：防止重复截取基础内容）
        addToCoveredRegions(m_baseImage, baseRect, ScrollDirection::None, 0);
        
        m_captureCount++;
        emit newImageCaptured(m_baseImage);
        emit captureStatusChanged("正在监听滚动...");
        
        // 输出关键的开始信息
        qDebug() << "📸 开始滚动截图 - 基础图片尺寸:" << m_baseImage.size() << "捕获区域:" << m_captureRect;
        
        // 启动检测定时器
        m_detectionTimer->start(m_detectionInterval);
    } else {
        // 错误信息保留
        emit captureStatusChanged("无法捕获初始截图");
        m_isCapturing = false;
    }
}

void ScreenshotCapture::stopScrollCapture()
{
    if (!m_isCapturing) {
        return;
    }
    
    m_isCapturing = false;
    m_detectionTimer->stop();
    m_duplicateSkipCount = 0;
    
    // 重置连续重复计数器
    m_consecutiveDuplicates = 0;
    
    // 清理过期的覆盖区域（性能优化）
    cleanupOldCoveredRegions();
    
    // 输出性能指标
    logPerformanceMetrics();
    
    // 合并所有图片
    m_combinedImage = combineImages();
    
    if (!m_combinedImage.isNull()) {
        emit captureFinished(m_combinedImage);
        
        // 打印拼接统计信息
        qDebug() << "🏁 截图结束统计:";
        qDebug() << "   总片段数:" << (m_newContents.size() + 1);
        qDebug() << "   跳过重复:" << m_duplicateSkipCount << "次";
        qDebug() << "   最终长图尺寸:" << m_combinedImage.size();
        qDebug() << "   Y轴总范围:" << m_globalBounds.height() << "像素";
        
        emit captureStatusChanged(QString("截图完成！总共 %1 个片段，跳过 %2 个重复")
                                 .arg(m_newContents.size() + 1).arg(m_duplicateSkipCount));
    } else {
        emit captureStatusChanged("合并图片失败");
    }
}

QPixmap ScreenshotCapture::getCombinedImage() const
{
    return m_combinedImage;
}

QPixmap ScreenshotCapture::getCurrentCombinedImage() const
{
    // 实时拼接当前已捕获的图片
    return combineImages();
}

QList<QPixmap> ScreenshotCapture::getCapturedImages() const
{
    QList<QPixmap> allImages;
    if (!m_baseImage.isNull()) {
        allImages.append(m_baseImage);
    }
    allImages.append(m_newContents);
    return allImages;
}

void ScreenshotCapture::clearCapturedImages()
{
    m_newContents.clear();
    m_segments.clear();
    m_globalRegions.clear();
    m_coveredRegions.clear();  // 清理已覆盖区域
    m_combinedImage = QPixmap();
    m_lastScreenshot = QPixmap();
    m_baseImage = QPixmap();
    m_captureCount = 0;
    m_globalBounds = QRect();
    m_currentScrollPos = 0;
    m_duplicateSkipCount = 0;  // 重置重复计数器
    m_consecutiveDuplicates = 0;  // 重置连续重复计数器
    m_lastDuplicateTime = 0;   // 重置最后重复时间
    m_lastCleanupTime = 0;
}

void ScreenshotCapture::setDetectionInterval(int intervalMs)
{
    m_detectionInterval = intervalMs;
    if (m_detectionTimer && m_detectionTimer->isActive()) {
        m_detectionTimer->start(m_detectionInterval);
    }
}

bool ScreenshotCapture::isCapturing() const
{
    return m_isCapturing;
}

int ScreenshotCapture::getCaptureCount() const
{
    return m_captureCount;
}

void ScreenshotCapture::onScrollDetectionTimer()
{
    if (!m_isCapturing) {
        m_detectionTimer->stop();
        return;
    }

    // 捕获当前屏幕区域
    QPixmap currentScreenshot = captureRegion(m_captureRect);
    if (currentScreenshot.isNull()) {
        return;
    }

    // 检测滚动
    ScrollInfo scrollInfo = detectScroll(m_lastScreenshot.toImage(), currentScreenshot.toImage());
    
    if (scrollInfo.hasScroll) {
        // 验证新内容是否有效
        if (scrollInfo.newContentRect.height() < MIN_NEW_CONTENT_HEIGHT) {
            qDebug() << "新内容高度过小，跳过此次捕获：" << scrollInfo.newContentRect.height();
            return;
        }

        // 提取新内容
        QImage newContent = extractNewContent(currentScreenshot.toImage(), scrollInfo);

        // 计算逻辑区域位置（基于滚动方向）
        QRect logicalRect;
        if (scrollInfo.direction == ScrollDirection::Down) {
            logicalRect = QRect(0, m_currentScrollPos, newContent.width(), newContent.height());
        } else if (scrollInfo.direction == ScrollDirection::Up) {
            int currentMinY = m_globalBounds.isEmpty() ? 0 : m_globalBounds.top();
            logicalRect = QRect(0, currentMinY - newContent.height(), newContent.width(), newContent.height());
        }

        // 使用改进的重复检测系统
        if (!isContentAlreadyCovered(QPixmap::fromImage(newContent), logicalRect)) {
            // 添加新内容到全局区域
            addNewContent(newContent, scrollInfo);
            
            // 更新最后截图（仅在成功添加内容后）
            m_lastScreenshot = currentScreenshot;
            
            // 发出新图片信号
            emit newImageCaptured(getCombinedImage());
        } else {
            qDebug() << "❌ 跳过重复内容 - 位置:" << logicalRect << "方向:" << (scrollInfo.direction == ScrollDirection::Down ? "↓" : "↑");
        }
    }
}

QPixmap ScreenshotCapture::captureRegion(const QRect& rect)
{
    if (!m_primaryScreen || rect.isEmpty()) {
        // 只保留错误信息
        return QPixmap();
    }
    
    double devicePixelRatio = m_primaryScreen->devicePixelRatio();
    
    QRect screenGeometry = m_primaryScreen->geometry();
    
    // 调整截图区域，向内缩小4个像素以避开红色边框（边框宽度2px+余量）
    QRect adjustedRect = rect.adjusted(4, 4, -4, -4);
    
    // 确保截图区域在屏幕范围内
    QRect validRect = adjustedRect.intersected(screenGeometry);
    if (validRect.isEmpty()) {
        // 错误信息保留
        return QPixmap();
    }
    
    // 截取全屏
    QPixmap fullScreen = m_primaryScreen->grabWindow(0);
    if (fullScreen.isNull()) {
        // 错误信息保留
        return QPixmap();
    }
    
    // 高DPI支持
    QRect finalRect;
    if (devicePixelRatio > 1.0) {
        finalRect = QRect(
            static_cast<int>(validRect.x() * devicePixelRatio),
            static_cast<int>(validRect.y() * devicePixelRatio),
            static_cast<int>(validRect.width() * devicePixelRatio),
            static_cast<int>(validRect.height() * devicePixelRatio)
        );
    } else {
        finalRect = validRect;
    }
    
    // 确保最终区域有效
    if (finalRect.x() < 0 || finalRect.y() < 0 || 
        finalRect.right() >= fullScreen.width() || 
        finalRect.bottom() >= fullScreen.height() ||
        finalRect.isEmpty()) {
        // 错误信息保留
        return QPixmap();
    }
    
    QPixmap result = fullScreen.copy(finalRect);
    
    // 只在截图失败时输出错误信息
    if (result.isNull()) {
        qDebug() << "❌ 截图失败 - 区域:" << finalRect << "全屏尺寸:" << fullScreen.size();
    }
    
    return result;
}

ScrollInfo ScreenshotCapture::detectScroll(const QImage& lastImg, const QImage& newImg) {
    ScrollInfo info;
    info.hasScroll = false;

    if (lastImg.isNull() || newImg.isNull() || lastImg.size() != newImg.size()) {
        return info;
    }

    OverlapResult downResult = findOverlapRegion(lastImg, newImg, ScrollDirection::Down);
    OverlapResult upResult = findOverlapRegion(lastImg, newImg, ScrollDirection::Up);

    // 选择相似度更高、且满足最小滚动距离的那个作为滚动方向
    bool downIsValid = downResult.similarity > SIMILARITY_THRESHOLD && downResult.rect.height() >= MIN_SCROLL_DISTANCE;
    bool upIsValid = upResult.similarity > SIMILARITY_THRESHOLD && upResult.rect.height() >= MIN_SCROLL_DISTANCE;

    if (downIsValid && (!upIsValid || downResult.similarity > upResult.similarity)) {
        info.direction = ScrollDirection::Down;
        info.offset = downResult.rect.height();  // 滚动距离
        info.hasScroll = true;
        
        // 向下滚动：新截图的顶部是新内容，底部是重叠
        info.overlapRect = QRect(0, info.offset, newImg.width(), newImg.height() - info.offset);  // 新截图中的重叠区域
        info.newContentRect = QRect(0, 0, newImg.width(), info.offset);  // 新截图中的新内容区域
        
        qDebug() << "检测到向下滚动，相似度：" << downResult.similarity << "滚动距离：" << info.offset;
    } else if (upIsValid && (!downIsValid || upResult.similarity > downResult.similarity)) {
        info.direction = ScrollDirection::Up;
        info.offset = upResult.rect.height();  // 滚动距离
        info.hasScroll = true;
        
        // 向上滚动：新截图的底部是新内容，顶部是重叠
        info.overlapRect = QRect(0, 0, newImg.width(), newImg.height() - info.offset);  // 新截图中的重叠区域
        info.newContentRect = QRect(0, newImg.height() - info.offset, newImg.width(), info.offset);  // 新截图中的新内容区域
        
        qDebug() << "检测到向上滚动，相似度：" << upResult.similarity << "滚动距离：" << info.offset;
    }

    return info;
}

double ScreenshotCapture::calculateImageSimilarity(const QImage& img1, const QImage& img2, const QRect& rect)
{
    if (img1.size() != img2.size() || rect.isEmpty()) {
        return 0.0;
    }
    
    QRect validRect = rect.intersected(QRect(0, 0, img1.width(), img1.height()));
    if (validRect.isEmpty()) {
        return 0.0;
    }
    
    int totalPixels = validRect.width() * validRect.height();
    int similarPixels = 0;
    
    for (int y = validRect.top(); y <= validRect.bottom(); y += 2) { // 采样优化
        for (int x = validRect.left(); x <= validRect.right(); x += 2) {
            QRgb pixel1 = img1.pixel(x, y);
            QRgb pixel2 = img2.pixel(x, y);
            
            // 计算RGB差异
            int rDiff = abs(qRed(pixel1) - qRed(pixel2));
            int gDiff = abs(qGreen(pixel1) - qGreen(pixel2));
            int bDiff = abs(qBlue(pixel1) - qBlue(pixel2));
            
            // 如果差异很小，认为是相似的
            if (rDiff + gDiff + bDiff < 30) {
                similarPixels++;
            }
        }
    }
    
    return double(similarPixels) / (totalPixels / 4); // 采样了1/4的像素
}

double ScreenshotCapture::calculateImageSimilarity(const QImage& img1, const QImage& img2, const QRect& rect1, const QRect& rect2)
{
    if (img1.isNull() || img2.isNull() || rect1.isEmpty() || rect2.isEmpty()) {
        return 0.0;
    }
    
    // 确保两个区域尺寸相同
    if (rect1.size() != rect2.size()) {
        return 0.0;
    }
    
    // 确保区域在图像范围内
    QRect validRect1 = rect1.intersected(QRect(0, 0, img1.width(), img1.height()));
    QRect validRect2 = rect2.intersected(QRect(0, 0, img2.width(), img2.height()));
    
    if (validRect1.isEmpty() || validRect2.isEmpty() || validRect1.size() != validRect2.size()) {
        return 0.0;
    }
    
    int totalPixels = validRect1.width() * validRect1.height();
    int similarPixels = 0;
    
    // 比较两个区域的像素
    for (int y = 0; y < validRect1.height(); y += 2) { // 采样优化
        for (int x = 0; x < validRect1.width(); x += 2) {
            QRgb pixel1 = img1.pixel(validRect1.x() + x, validRect1.y() + y);
            QRgb pixel2 = img2.pixel(validRect2.x() + x, validRect2.y() + y);
            
            // 计算RGB差异
            int rDiff = abs(qRed(pixel1) - qRed(pixel2));
            int gDiff = abs(qGreen(pixel1) - qGreen(pixel2));
            int bDiff = abs(qBlue(pixel1) - qBlue(pixel2));
            
            // 如果差异很小，认为是相似的
            if (rDiff + gDiff + bDiff < 30) {
                similarPixels++;
            }
        }
    }
    
    return double(similarPixels) / (totalPixels / 4); // 采样了1/4的像素
}

OverlapResult ScreenshotCapture::findOverlapRegion(const QImage& img1, const QImage& img2, ScrollDirection direction)
{
    OverlapResult result;
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull()) {
        return result;
    }

    int width = img1.width();
    int height = img1.height();
    // 限制搜索范围，避免检测到过大的滚动距离
    int maxSearchHeight = qMin(100, height / 4);  // 最多搜索100像素或1/4屏幕高度

    for (int offset = MIN_SCROLL_DISTANCE; offset <= maxSearchHeight; ++offset) {
        QRect region1, region2;
        if (direction == ScrollDirection::Down) {
            region1 = QRect(0, height - offset, width, offset);
            region2 = QRect(0, 0, width, offset);
        } else { // ScrollDirection::Up
            region1 = QRect(0, 0, width, offset);
            region2 = QRect(0, height - offset, width, offset);
        }

        double similarity = calculateImageSimilarity(img1, img2, region1, region2);
        
        // 一旦找到足够好的匹配（相似度超过阈值），立即返回，不再寻找更大的偏移
        if (similarity > SIMILARITY_THRESHOLD) {
            result.similarity = similarity;
            result.rect = (direction == ScrollDirection::Down) ? 
                          QRect(0, height - offset, width, offset) : 
                          QRect(0, 0, width, offset);
            qDebug() << "找到滚动匹配 - 距离:" << offset << "像素，相似度:" << similarity;
            break;  // 找到合适匹配就停止搜索
        }
        
        // 如果当前相似度更高，更新结果（但继续搜索更小的偏移）
        if (similarity > result.similarity) {
            result.similarity = similarity;
            result.rect = (direction == ScrollDirection::Down) ? 
                          QRect(0, height - offset, width, offset) : 
                          QRect(0, 0, width, offset);
        }
    }

    if (result.similarity < SIMILARITY_THRESHOLD || result.rect.height() < MIN_OVERLAP_HEIGHT) {
        result.rect = QRect(); // 不满足条件，返回空区域
        qDebug() << "未找到有效的滚动匹配，最高相似度:" << result.similarity;
    }

    return result;
}

QImage ScreenshotCapture::extractNewContent(const QImage& newImage, const ScrollInfo& scrollInfo) {
    if (newImage.isNull() || !scrollInfo.hasScroll) {
        return QImage();
    }

    // 直接根据scrollInfo中计算好的newContentRect提取新内容
    QImage newContent = newImage.copy(scrollInfo.newContentRect);
    qDebug() << "✂️ 提取新内容 - 区域:" << scrollInfo.newContentRect << "结果尺寸:" << newContent.size();
    return newContent;
}

bool ScreenshotCapture::isContentDuplicate(const QPixmap& newContent, const ScrollInfo& scrollInfo)
{
    if (newContent.isNull() || m_globalRegions.isEmpty()) {
        return false;
    }
    
    QImage newImg = newContent.toImage();
    
    // 直接与所有已存在的图像进行比较
    for (const GlobalContentRegion& region : m_globalRegions) {
        QImage existingImg = region.image.toImage();
        
        // 如果尺寸相同，直接比较整个图像
        if (newImg.size() == existingImg.size()) {
            double similarity = calculateImageSimilarity(newImg, existingImg, 
                QRect(0, 0, newImg.width(), newImg.height()));
            
            if (similarity > 0.7) {  // 70%以上相似度认为是重复
                qDebug() << "检测到完全重复内容，相似度：" << similarity 
                         << "新内容尺寸：" << newImg.size() 
                         << "已存在区域：" << region.logicalRect;
                return true;
            }
        }
        
        // 检查部分重叠
        if (newImg.height() <= region.image.height() && newImg.width() == region.image.width()) {
            // 检查新内容是否是现有内容的一部分
            for (int yOffset = 0; yOffset <= region.image.height() - newImg.height(); yOffset += 10) {
                QRect checkRect(0, yOffset, newImg.width(), newImg.height());
                QRect newRect(0, 0, newImg.width(), newImg.height());
                
                double similarity = calculateImageSimilarity(newImg, existingImg, 
                                                           newRect, checkRect);
                
                if (similarity > 0.7) {
                    qDebug() << "检测到部分重复内容，相似度：" << similarity 
                             << "Y偏移：" << yOffset 
                             << "新内容尺寸：" << newImg.size();
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool ScreenshotCapture::isContentInGlobalRegion(const QPixmap& newContent, const QRect& logicalRect)
{
    if (newContent.isNull() || m_globalRegions.isEmpty()) {
        return false;
    }
    
    QImage newImg = newContent.toImage();
    
    // 检查与所有全局区域的重叠
    for (const GlobalContentRegion& region : m_globalRegions) {
        // 检查逻辑坐标是否有重叠
        QRect intersection = logicalRect.intersected(region.logicalRect);
        if (intersection.isEmpty()) {
            continue;
        }
        
        // 计算重叠区域的相似度
        int overlapHeight = intersection.height();
        if (overlapHeight < 20) {  // 重叠太小，忽略
            continue;
        }
        
        // 提取重叠区域进行比较
        QRect newContentOverlap = QRect(0, intersection.y() - logicalRect.y(), 
                                       intersection.width(), overlapHeight);
        QRect existingOverlap = QRect(0, intersection.y() - region.logicalRect.y(),
                                     intersection.width(), overlapHeight);
        
        // 确保区域有效
        if (newContentOverlap.y() < 0 || newContentOverlap.bottom() > newImg.height() ||
            existingOverlap.y() < 0 || existingOverlap.bottom() > region.image.height()) {
            continue;
        }
        
        // 计算相似度
        QImage existingImg = region.image.toImage();
        double similarity = calculateImageSimilarity(newImg, existingImg, 
                                                   newContentOverlap, existingOverlap);
        
        if (similarity > 0.8) {  // 80%以上相似度认为是重复（提高阈值）
            return true;
        }
    }
    
    return false;
}

void ScreenshotCapture::updateGlobalRegion(const QPixmap& image, const QRect& logicalRect)
{
    GlobalContentRegion newRegion;
    newRegion.image = image;
    newRegion.logicalRect = logicalRect;
    newRegion.order = m_globalRegions.size() + 1;
    m_globalRegions.append(newRegion);
    
    // 扩展全局边界
    if (m_globalBounds.isEmpty()) {
        m_globalBounds = logicalRect;
    } else {
        m_globalBounds = m_globalBounds.united(logicalRect);
    }
}

void ScreenshotCapture::addNewContent(const QImage& newContent, const ScrollInfo& scrollInfo)
{
    if (!newContent.isNull()) {
        // 现在newContent已经是纯净的新内容，不包含重叠部分
        QRect logicalRect;
        
        if (scrollInfo.direction == ScrollDirection::Down) {
            // 向下滚动：新内容添加到当前内容的底部，确保连续无重叠
            logicalRect = QRect(0, m_currentScrollPos, newContent.width(), newContent.height());
            m_currentScrollPos += newContent.height();  // 更新当前位置到新内容的底部
            
        } else if (scrollInfo.direction == ScrollDirection::Up) {
            // 向上滚动：新内容添加到现有内容的顶部（负Y值）
            int currentMinY = m_globalBounds.isEmpty() ? 0 : m_globalBounds.top();
            int newTopY = currentMinY - newContent.height();
            logicalRect = QRect(0, newTopY, newContent.width(), newContent.height());
            
        } else {
            // 初始内容或未知方向
            if (m_globalBounds.isEmpty()) {
                logicalRect = QRect(0, 0, newContent.width(), newContent.height());
                m_currentScrollPos = newContent.height();
            } else {
                // 默认向下添加
                logicalRect = QRect(0, m_currentScrollPos, newContent.width(), newContent.height());
                m_currentScrollPos += newContent.height();
            }
        }
        
        // 添加到覆盖区域管理
        addToCoveredRegions(QPixmap::fromImage(newContent), logicalRect, scrollInfo.direction, 0);

        // 创建新的内容段并添加到全局区域
        GlobalContentRegion newSegment;
        newSegment.logicalRect = logicalRect;
        newSegment.image = QPixmap::fromImage(newContent);
        newSegment.overlapHeight = 0;  // 新内容没有重叠
        newSegment.scrollDirection = scrollInfo.direction;
        newSegment.order = m_globalRegions.size() + 1;

        m_globalRegions.append(newSegment);
        updateGlobalBounds(logicalRect);

        qDebug() << "✅ 添加新内容片段" << newSegment.order << ": \"" << 
                    (scrollInfo.direction == ScrollDirection::Down ? "向下滚动↓" : 
                     scrollInfo.direction == ScrollDirection::Up ? "向上滚动↑" : "初始内容") << "\" | " <<
                    "纯净尺寸:" << newContent.width() << "x" << newContent.height() << "| " <<
                    "位置Y:" << logicalRect.y() << "| " <<
                    "滚动偏移:" << scrollInfo.offset;

        updateCaptureStatus();
    }
}

QPixmap ScreenshotCapture::combineImages() const
{
    if (m_globalRegions.isEmpty()) {
        return m_baseImage;
    }
    
    // 使用全局区域创建真正的连贯长图
    return createGlobalCombinedImage();
}

QPixmap ScreenshotCapture::createGlobalCombinedImage() const
{
    if (m_globalRegions.isEmpty()) {
        return QPixmap();
    }

    // 计算所有全局区域的逻辑边界
    QRect finalLogicalBounds;
    for (const GlobalContentRegion& region : m_globalRegions) {
        if (finalLogicalBounds.isEmpty()) {
            finalLogicalBounds = region.logicalRect;
        } else {
            finalLogicalBounds = finalLogicalBounds.united(region.logicalRect);
        }
    }

    // 创建最终图片，使用透明背景
    QPixmap finalImage(finalLogicalBounds.width(), finalLogicalBounds.height());
    finalImage.fill(Qt::transparent);

    QPainter painter(&finalImage);
    painter.setRenderHint(QPainter::Antialiasing, false);  // 关闭抗锯齿以保持像素精确
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 按Y坐标排序绘制区域，确保正确的层次
    QList<GlobalContentRegion> sortedRegions = m_globalRegions;
    std::sort(sortedRegions.begin(), sortedRegions.end(), 
              [](const GlobalContentRegion& a, const GlobalContentRegion& b) {
                  return a.logicalRect.y() < b.logicalRect.y();
              });

    // 打印拼接结构信息
    qDebug() << "🔧 拼接结构分析 - 总片段数:" << sortedRegions.size() << "最终尺寸:" << finalImage.size();
    for (int i = 0; i < sortedRegions.size(); ++i) {
        const GlobalContentRegion& region = sortedRegions[i];
        int relativeY = region.logicalRect.y() - finalLogicalBounds.y();
        qDebug() << QString("   片段%1: Y位置=%2 尺寸=%3x%4 相对位置=%5")
                    .arg(i + 1)
                    .arg(region.logicalRect.y())
                    .arg(region.image.width())
                    .arg(region.image.height())
                    .arg(relativeY);
    }

    for (const GlobalContentRegion& region : sortedRegions) {
        // 计算在最终图片中的相对位置
        int relativeX = region.logicalRect.x() - finalLogicalBounds.x();
        int relativeY = region.logicalRect.y() - finalLogicalBounds.y();
        
        // 确保绘制位置在有效范围内
        if (relativeX >= 0 && relativeY >= 0 && 
            relativeX < finalImage.width() && relativeY < finalImage.height()) {
            
            // 计算实际需要绘制的区域
            QRect drawRect(relativeX, relativeY, region.image.width(), region.image.height());
            QRect finalRect(0, 0, finalImage.width(), finalImage.height());
            QRect clippedRect = drawRect.intersected(finalRect);
            
            if (!clippedRect.isEmpty()) {
                // 计算源图像的对应区域
                QRect sourceRect(clippedRect.x() - relativeX, clippedRect.y() - relativeY,
                               clippedRect.width(), clippedRect.height());
                
                painter.drawPixmap(clippedRect, region.image, sourceRect);
            }
        }
    }

    qDebug() << "✅ 拼接完成 - 最终长图尺寸:" << finalImage.size() << "Y范围:" << finalLogicalBounds.y() << "到" << finalLogicalBounds.bottom();
    return finalImage;
}

void ScreenshotCapture::updateCaptureStatus()
{
    emit captureStatusChanged(QString("滚动中... 已捕获 %1 个片段").arg(m_newContents.size() + 1));
}

bool ScreenshotCapture::isContentAlreadyCovered(const QPixmap& newContent, const QRect& logicalRect)
{
    if (newContent.isNull() || m_coveredRegions.isEmpty()) {
        // 重置连续重复计数
        m_consecutiveDuplicates = 0;
        return false;
    }
    
    // 检查连续重复次数
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (m_consecutiveDuplicates >= MAX_ALLOWED_DUPLICATES) {
        // 如果连续重复次数过多，增加等待时间
        if (currentTime - m_lastDuplicateTime < 1000) {  // 1秒内不再检测
            qDebug() << "连续重复过多，暂停检测";
            return true;
        } else {
            // 重置计数器
            m_consecutiveDuplicates = 0;
        }
    }
    
    // 创建新内容的指纹
    QString newFingerprint = createContentFingerprint(newContent);
    
    // 检查新内容区域是否与已覆盖区域重叠
    for (const CoveredRegion& covered : m_coveredRegions) {
        // 快速指纹比较
        if (newFingerprint == covered.contentFingerprint) {
            qDebug() << "指纹匹配：发现完全相同的内容";
            m_duplicateSkipCount++;
            m_consecutiveDuplicates++;
            m_lastDuplicateTime = currentTime;
            return true;
        }
        
        // 检查区域重叠 - 提高重叠阈值
        if (!isOverlapSignificant(logicalRect, covered.logicalRect, 0.7)) {
            continue;
        }
        
        // 计算内容相似度
        QPixmap coveredPixmap = QPixmap::fromImage(covered.contentHash);
        double similarity = calculateContentSimilarity(newContent, coveredPixmap);
        
        // 提高相似度阈值到85%
        if (similarity > 0.85) {
            qDebug() << "发现重复内容：相似度" << similarity 
                     << "重叠区域" << logicalRect.intersected(covered.logicalRect)
                     << "捕获方向" << (int)covered.captureDirection;
            m_duplicateSkipCount++;
            m_consecutiveDuplicates++;
            m_lastDuplicateTime = currentTime;
            return true;
        }
        
        // 特别处理：如果滚动方向相反，可能是回滚，需要更严格的检查
        if ((covered.captureDirection == ScrollDirection::Down && m_currentScrollPos < covered.logicalRect.bottom()) ||
            (covered.captureDirection == ScrollDirection::Up && m_currentScrollPos > covered.logicalRect.top())) {
            
            // 回滚时提高阈值到80%
            if (similarity > 0.80) {
                qDebug() << "检测到回滚重复内容：相似度" << similarity;
                m_duplicateSkipCount++;
                m_consecutiveDuplicates++;
                m_lastDuplicateTime = currentTime;
                return true;
            }
        }
        
        // 额外检查：如果是相邻的内容且相似度很高，也认为是重复
        if (abs(logicalRect.top() - covered.logicalRect.bottom()) < 50 || 
            abs(logicalRect.bottom() - covered.logicalRect.top()) < 50) {
            if (similarity > 0.75) {
                qDebug() << "检测到相邻重复内容：相似度" << similarity;
                m_duplicateSkipCount++;
                m_consecutiveDuplicates++;
                m_lastDuplicateTime = currentTime;
                return true;
            }
        }
    }
    
    // 如果没有发现重复，重置连续重复计数
    m_consecutiveDuplicates = 0;
    return false;
}

QString ScreenshotCapture::createContentFingerprint(const QPixmap& content)
{
    if (content.isNull()) {
        return QString();
    }
    
    // 创建更精确的内容指纹
    QImage img = content.toImage();
    QCryptographicHash hash(QCryptographicHash::Md5);
    
    // 缩放到固定尺寸以提高比较效率，但保持更高精度
    QImage scaledImg = img.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // 减少采样步长，提高精度
    int sampleStep = 1;  // 每个像素都采样
    
    // 添加图像尺寸信息到指纹中
    hash.addData(QByteArray::number(scaledImg.width()));
    hash.addData(QByteArray::number(scaledImg.height()));
    
    // 按采样步长处理像素
    for (int y = 0; y < scaledImg.height(); y += sampleStep) {
        for (int x = 0; x < scaledImg.width(); x += sampleStep) {
            QRgb pixel = scaledImg.pixel(x, y);
            hash.addData(QByteArrayView(reinterpret_cast<const char*>(&pixel), sizeof(QRgb)));
        }
    }
    
    // 添加额外的统计信息以提高唯一性
    qint64 totalBrightness = 0;
    for (int y = 0; y < scaledImg.height(); y++) {
        for (int x = 0; x < scaledImg.width(); x++) {
            QRgb pixel = scaledImg.pixel(x, y);
            totalBrightness += qRed(pixel) + qGreen(pixel) + qBlue(pixel);
        }
    }
    hash.addData(QByteArray::number(totalBrightness));
    
    return hash.result().toHex();
}

double ScreenshotCapture::calculateContentSimilarity(const QPixmap& content1, const QPixmap& content2)
{
    if (content1.isNull() || content2.isNull()) {
        return 0.0;
    }
    
    // 快速检查：如果尺寸差异太大，直接返回低相似度
    if (abs(content1.width() - content2.width()) > 30 || 
        abs(content1.height() - content2.height()) > 30) {
        return 0.0;
    }
    
    // 使用指纹进行快速比较
    QString fingerprint1 = createContentFingerprint(content1);
    QString fingerprint2 = createContentFingerprint(content2);
    
    if (fingerprint1 == fingerprint2) {
        return 1.0;  // 完全相同
    }
    
    // 进行更详细的像素级比较 - 使用更高精度
    QImage img1 = content1.toImage().scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage img2 = content2.toImage().scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    int totalPixels = 0;
    int similarPixels = 0;
    
    // 使用更密集的采样 - 每个像素都检查
    for (int y = 0; y < qMin(img1.height(), img2.height()); y++) {
        for (int x = 0; x < qMin(img1.width(), img2.width()); x++) {
            QRgb pixel1 = img1.pixel(x, y);
            QRgb pixel2 = img2.pixel(x, y);
            
            int rDiff = abs(qRed(pixel1) - qRed(pixel2));
            int gDiff = abs(qGreen(pixel1) - qGreen(pixel2));
            int bDiff = abs(qBlue(pixel1) - qBlue(pixel2));
            
            totalPixels++;
            // 降低容差，提高精度
            if (rDiff + gDiff + bDiff < 30) {
                similarPixels++;
            }
        }
    }
    
    return totalPixels > 0 ? double(similarPixels) / totalPixels : 0.0;
}

bool ScreenshotCapture::isOverlapSignificant(const QRect& rect1, const QRect& rect2, double threshold)
{
    QRect intersection = rect1.intersected(rect2);
    if (intersection.isEmpty()) {
        return false;
    }
    
    // 计算重叠面积相对于较小矩形的比例
    int area1 = rect1.width() * rect1.height();
    int area2 = rect2.width() * rect2.height();
    int intersectionArea = intersection.width() * intersection.height();
    
    int smallerArea = qMin(area1, area2);
    double overlapRatio = double(intersectionArea) / smallerArea;
    
    return overlapRatio > threshold;
}

void ScreenshotCapture::addToCoveredRegions(const QPixmap& newContent, const QRect& logicalRect, ScrollDirection direction, int captureOrder)
{
    if (newContent.isNull() || logicalRect.isEmpty()) {
        return;
    }
    
    CoveredRegion newCovered;
    newCovered.logicalRect = logicalRect;
    newCovered.contentHash = createContentHash(newContent);
    newCovered.contentFingerprint = createContentFingerprint(newContent);
    newCovered.captureDirection = direction;
    newCovered.captureOrder = captureOrder;
    newCovered.captureTimestamp = QDateTime::currentMSecsSinceEpoch();
    newCovered.actualScreenRect = logicalRect;  // 简化处理
    
    m_coveredRegions.append(newCovered);
    
    // 定期清理旧的覆盖区域
    if (m_coveredRegions.size() > m_maxCoveredRegions) {
        cleanupOldCoveredRegions();
    }
    
    // 记录关键信息
    qDebug() << "覆盖区域管理：总数" << m_coveredRegions.size() 
             << "方向" << (direction == ScrollDirection::Down ? "↓" : direction == ScrollDirection::Up ? "↑" : "初始")
             << "区域" << logicalRect;
}

void ScreenshotCapture::cleanupOldCoveredRegions()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // 只保留最近的区域，或者按时间清理
    if (m_coveredRegions.size() > m_maxCoveredRegions) {
        // 移除最旧的区域
        int removeCount = m_coveredRegions.size() - m_maxCoveredRegions + 20;
        m_coveredRegions.erase(m_coveredRegions.begin(), m_coveredRegions.begin() + removeCount);
        
        qDebug() << "清理了" << removeCount << "个旧的覆盖区域，当前数量：" << m_coveredRegions.size();
    }
    
    m_lastCleanupTime = currentTime;
}

void ScreenshotCapture::logPerformanceMetrics()
{
    qDebug() << "性能指标：已覆盖区域数" << m_coveredRegions.size() 
             << "跳过重复次数" << m_duplicateSkipCount
             << "采样步长" << m_hashSampleStep;
}

QImage ScreenshotCapture::createContentHash(const QPixmap& content)
{
    if (content.isNull()) {
        return QImage();
    }
    
    // 创建一个小的缩略图作为内容哈希
    QImage img = content.toImage();
    QImage hash = img.scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    return hash;
} 

void ScreenshotCapture::updateGlobalBounds(const QRect& rect) {
    if (m_globalBounds.isEmpty()) {
        m_globalBounds = rect;
    } else {
        m_globalBounds = m_globalBounds.united(rect);
    }
} 
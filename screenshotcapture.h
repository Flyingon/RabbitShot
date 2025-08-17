#ifndef SCREENSHOTCAPTURE_H
#define SCREENSHOTCAPTURE_H

#include <QObject>
#include <QPixmap>
#include <QRect>
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QList>
#include <QImage>
#include <QDateTime>
#include <QCryptographicHash>

enum class ScrollDirection {
    None,
    Up,
    Down,
    Left,
    Right
};

struct ScrollInfo {
    ScrollDirection direction = ScrollDirection::None;
    int offset = 0;
    bool hasScroll = false;
    QRect overlapRect;
    QRect newContentRect;
};

struct ContentSegment {
    QPixmap image;
    int yOffset;        // 在最终图像中的Y偏移
    int overlapHeight;  // 与前一个片段的重叠高度
    bool isBaseImage;   // 是否为基础图片
};

// 用于返回重叠区域检测结果的结构体
struct OverlapResult {
    QRect rect;
    double similarity = 0.0;
};

// 已覆盖区域结构，用于精确记录已截取的内容
struct CoveredRegion {
    QRect logicalRect;          // 逻辑坐标系中的区域
    QImage contentHash;         // 内容的哈希表示（缩略图）
    QString contentFingerprint; // 内容指纹（字符串哈希）
    ScrollDirection captureDirection; // 截取时的滚动方向
    int captureOrder;           // 截取顺序
    qint64 captureTimestamp;    // 截取时间戳
    QRect actualScreenRect;     // 实际屏幕坐标（用于重叠检测）
};

// 全局内容区域结构
struct GlobalContentRegion {
    QRect logicalRect;
    QPixmap image;
    int overlapHeight = 0;
    ScrollDirection scrollDirection = ScrollDirection::None;
    int order = 0;
};

class ScreenshotCapture : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshotCapture(QObject *parent = nullptr);
    ~ScreenshotCapture();

    // 设置截图区域
    void setCapturezone(const QRect& rect);
    
    // 开始/停止滚动截图
    void startScrollCapture();
    void stopScrollCapture();
    
    // 获取合并后的截图
    QPixmap getCombinedImage() const;
    
    // 获取当前拼接的图片（实时）
    QPixmap getCurrentCombinedImage() const;
    
    // 获取捕获的图片列表
    QList<QPixmap> getCapturedImages() const;
    
    // 清空已捕获的图片
    void clearCapturedImages();
    
    // 设置检测间隔
    void setDetectionInterval(int intervalMs);
    
    // 获取状态
    bool isCapturing() const;
    int getCaptureCount() const;
    
    // 测试截图功能
    QPixmap captureRegion(const QRect& rect);

signals:
    void imagesCaptured(const QList<QPixmap>& images);
    void newImageCaptured(const QPixmap& image);
    void captureFinished(const QPixmap& combinedImage);
    void captureStatusChanged(const QString& status);
    void scrollDetected(ScrollDirection direction, int offset);

private slots:
    void onScrollDetectionTimer();

private:
    void updateGlobalBounds(const QRect& rect);
    ScrollInfo detectScroll(const QImage& lastImg, const QImage& newImg);
    double calculateImageSimilarity(const QImage& img1, const QImage& img2, const QRect& rect);
    double calculateImageSimilarity(const QImage& img1, const QImage& img2, const QRect& rect1, const QRect& rect2);
    OverlapResult findOverlapRegion(const QImage& img1, const QImage& img2, ScrollDirection direction);
    QImage extractNewContent(const QImage& newImage, const ScrollInfo& scrollInfo);
    bool isContentDuplicate(const QPixmap& newContent, const ScrollInfo& scrollInfo);
    bool isContentInGlobalRegion(const QPixmap& newContent, const QRect& logicalRect);
    bool isContentAlreadyCovered(const QPixmap& newContent, const QRect& logicalRect);
    void addToCoveredRegions(const QPixmap& newContent, const QRect& logicalRect);
    void addToCoveredRegions(const QPixmap& newContent, const QRect& logicalRect, ScrollDirection direction);
    QImage createContentHash(const QPixmap& content);
    QString createContentFingerprint(const QPixmap& content);
    double calculateContentSimilarity(const QPixmap& content1, const QPixmap& content2);
    bool isOverlapSignificant(const QRect& rect1, const QRect& rect2, double threshold = 0.6);
    void cleanupOldCoveredRegions();
    void logPerformanceMetrics();
    QPixmap extractNewContentOnly(const QPixmap& newImage, const ScrollInfo& scrollInfo);
    void updateGlobalRegion(const QPixmap& newContent, const QRect& logicalRect);
    QPixmap createGlobalCombinedImage() const;
    void addNewContent(const QImage& newContent, const ScrollInfo& scrollInfo);
    void addToCoveredRegions(const QPixmap& newContent, const QRect& logicalRect, ScrollDirection direction, int captureOrder);
    QPixmap combineImages() const;
    void updateCaptureStatus();

    QTimer* m_detectionTimer;
    QScreen* m_primaryScreen;
    
    QRect m_captureRect;
    QPixmap m_lastScreenshot;
    QPixmap m_baseImage;        // 基础图片（第一张或当前完整图片）
    QList<QPixmap> m_newContents;  // 新内容片段
    QList<ContentSegment> m_segments;  // 片段拼接信息
    QList<GlobalContentRegion> m_globalRegions;  // 全局内容区域
    QList<CoveredRegion> m_coveredRegions;  // 已覆盖区域记录
    QPixmap m_combinedImage;
    
    // 全局坐标系管理
    QRect m_globalBounds;     // 全局内容的边界
    int m_currentScrollPos;   // 当前滚动位置（逻辑坐标）
    
    // 性能监控和配置
    int m_hashSampleStep;     // 哈希计算采样步长
    int m_maxCoveredRegions;  // 最大覆盖区域数量
    qint64 m_lastCleanupTime; // 上次清理时间
    int m_duplicateSkipCount; // 跳过重复内容的次数
    int m_consecutiveDuplicates;  // 连续重复计数
    qint64 m_lastDuplicateTime;   // 上次重复检测时间
    
    bool m_isCapturing;
    int m_captureCount;
    int m_detectionInterval;
    
    // 滚动检测参数
    static const int DEFAULT_DETECTION_INTERVAL = 200;  // 200ms检测间隔（降低频率，提高稳定性）
    static constexpr double SIMILARITY_THRESHOLD = 0.75;    // 相似度阈值（适度放宽，提升匹配成功率）
     static const int MIN_SCROLL_DISTANCE = 15;          // 最小滚动距离（降低最小距离）
     static const int OVERLAP_SEARCH_HEIGHT = 200;       // 重叠搜索高度（增加搜索范围）
     static const int MIN_NEW_CONTENT_HEIGHT = 10;       // 最小新内容高度（允许较小的滚动）
     static const int MIN_OVERLAP_HEIGHT = 10;           // 最小重叠高度
     static const int MAX_ALLOWED_DUPLICATES = 3;        // 最大允许连续重复次数
};

#endif // SCREENSHOTCAPTURE_H
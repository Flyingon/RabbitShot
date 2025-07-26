#ifndef SCREENSHOTPREVIEW_H
#define SCREENSHOTPREVIEW_H

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPixmap>
#include <QList>
#include <QProgressBar>
#include <QTimer>

class ScreenshotPreview : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenshotPreview(QWidget *parent = nullptr);
    ~ScreenshotPreview();

    void showPreview(const QRect& captureRect);
    void hidePreview();
    void updatePreview(const QList<QPixmap>& images);
    void updateRealTimePreview(const QPixmap& image);
    void setFinalImage(const QPixmap& image);
    void clearPreview();

signals:
    void saveRequested();
    void closeRequested();

private:
    void setupUI();
    void updateImageDisplay();
    void updateButtons();
    QPixmap combineImages(const QList<QPixmap>& images);

    QVBoxLayout* m_mainLayout;
    
    QScrollArea* m_scrollArea;
    QLabel* m_imageLabel;
    QLabel* m_infoLabel;
    QProgressBar* m_progressBar;
    
    QList<QPixmap> m_capturedImages;
    QPixmap m_finalImage;
    QRect m_captureRect;
    
    bool m_isCapturing;
    int m_imageCount;
};

#endif // SCREENSHOTPREVIEW_H 
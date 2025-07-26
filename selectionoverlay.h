#ifndef SELECTIONOVERLAY_H
#define SELECTIONOVERLAY_H

#include <QWidget>
#include <QRect>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>

class SelectionOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit SelectionOverlay(QWidget *parent = nullptr);
    ~SelectionOverlay();

    void startSelection();
    void cancelSelection();
    QRect getSelectedRect() const;
    void showCaptureUI();  // 显示截图后的界面
    void hideCaptureUI();  // 隐藏截图界面

signals:
    void selectionConfirmed(const QRect& rect);
    void selectionCancelled();
    void saveRequested();  // 保存请求
    void captureFinished(); // 截图完成

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onConfirmClicked();
    void onCancelClicked();
    void onSaveClicked();
    void onFinishClicked();

private:
    void setupUI();
    void setupCaptureUI(); // 设置截图后的UI
    void updateButtonPosition();
    void drawDashedRect(QPainter& painter, const QRect& rect);
    void showButtons();
    void hideButtons();
    void switchToCaptureMode(); // 切换到截图模式

    QRect m_selectedRect;
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_isSelecting;
    bool m_isSelected;
    bool m_isCapturing;  // 是否正在截图
    
    // 屏幕信息
    QRect m_screenGeometry;
    qreal m_devicePixelRatio;

    // UI 组件 - 选择模式
    QWidget *m_buttonContainer;
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_confirmButton;
    QPushButton *m_cancelButton;
    QLabel *m_infoLabel;
    
    // UI 组件 - 截图模式
    QWidget *m_captureContainer;
    QHBoxLayout *m_captureLayout;
    QPushButton *m_saveButton;
    QPushButton *m_finishButton;
    QLabel *m_captureInfoLabel;
    
    static const int BUTTON_HEIGHT = 40;
    static const int BUTTON_SPACING = 10;
};

#endif // SELECTIONOVERLAY_H 
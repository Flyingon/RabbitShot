#include "selectionoverlay.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QDebug>

SelectionOverlay::SelectionOverlay(QWidget *parent)
    : QWidget(parent)
    , m_isSelecting(false)
    , m_isSelected(false)
    , m_isCapturing(false)
    , m_buttonContainer(nullptr)
    , m_buttonLayout(nullptr)
    , m_confirmButton(nullptr)
    , m_cancelButton(nullptr)
    , m_infoLabel(nullptr)
    , m_captureContainer(nullptr)
    , m_captureLayout(nullptr)
    , m_saveButton(nullptr)
    , m_finishButton(nullptr)
    , m_captureInfoLabel(nullptr)
{
    setupUI();
    setupCaptureUI();
    
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // 获取主屏幕信息
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        m_screenGeometry = screen->geometry();
        m_devicePixelRatio = screen->devicePixelRatio();
        
        qDebug() << "设置选择覆盖层几何：" << m_screenGeometry;
        qDebug() << "屏幕设备像素比：" << m_devicePixelRatio;
        
        // 覆盖整个屏幕
        setGeometry(m_screenGeometry);
        
        // 验证设置是否成功
        qDebug() << "实际覆盖层几何：" << geometry();
    } else {
        qDebug() << "无法获取主屏幕信息";
        m_screenGeometry = QRect(0, 0, 1920, 1080);  // 默认值
        m_devicePixelRatio = 1.0;
    }
    
    setCursor(Qt::CrossCursor);
}

SelectionOverlay::~SelectionOverlay()
{
}

void SelectionOverlay::setupUI()
{
    // 创建按钮容器
    m_buttonContainer = new QWidget(this);
    m_buttonLayout = new QHBoxLayout(m_buttonContainer);
    
    m_confirmButton = new QPushButton("确认", m_buttonContainer);
    m_cancelButton = new QPushButton("取消", m_buttonContainer);
    m_infoLabel = new QLabel("拖拽选择截图区域", m_buttonContainer);
    
    // 设置按钮样式
    m_confirmButton->setStyleSheet(
        "QPushButton { "
        "background-color: #4CAF50; "
        "color: white; "
        "border: none; "
        "padding: 10px 20px; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "font-size: 14px; "
        "min-width: 60px; "
        "min-height: 32px; "
        "} "
        "QPushButton:hover { "
        "background-color: #45a049; "
        "} "
        "QPushButton:pressed { "
        "background-color: #3d8b40; "
        "}"
    );
    
    m_cancelButton->setStyleSheet(
        "QPushButton { "
        "background-color: #f44336; "
        "color: white; "
        "border: none; "
        "padding: 10px 20px; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "font-size: 14px; "
        "min-width: 60px; "
        "min-height: 32px; "
        "} "
        "QPushButton:hover { "
        "background-color: #da190b; "
        "} "
        "QPushButton:pressed { "
        "background-color: #b71c1c; "
        "}"
    );
    
    m_infoLabel->setStyleSheet(
        "QLabel { "
        "color: white; "
        "background-color: rgba(0, 0, 0, 128); "
        "border-radius: 4px; "
        "padding: 8px 12px; "
        "font-size: 14px; "
        "}"
    );
    
    // 布局
    m_buttonLayout->addWidget(m_infoLabel);
    m_buttonLayout->addWidget(m_confirmButton);
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->setSpacing(BUTTON_SPACING);
    m_buttonLayout->setContentsMargins(5, 5, 5, 5);
    
    // 设置按钮容器的最小高度
    m_buttonContainer->setMinimumHeight(50);
    
    // 连接信号
    connect(m_confirmButton, &QPushButton::clicked, this, &SelectionOverlay::onConfirmClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &SelectionOverlay::onCancelClicked);
    
    // 初始隐藏按钮
    hideButtons();
}

void SelectionOverlay::setupCaptureUI()
{
    // 创建截图模式的按钮容器
    m_captureContainer = new QWidget(this);
    m_captureLayout = new QHBoxLayout(m_captureContainer);
    
    m_saveButton = new QPushButton("保存截图", m_captureContainer);
    m_finishButton = new QPushButton("结束截图", m_captureContainer);
    m_captureInfoLabel = new QLabel("正在滚动截图...", m_captureContainer);
    
    // 设置按钮样式
    m_saveButton->setStyleSheet(
        "QPushButton { "
        "background-color: #4CAF50; "
        "color: white; "
        "border: none; "
        "padding: 10px 20px; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "font-size: 14px; "
        "min-width: 80px; "
        "min-height: 32px; "
        "} "
        "QPushButton:hover { "
        "background-color: #45a049; "
        "} "
        "QPushButton:pressed { "
        "background-color: #3d8b40; "
        "}"
    );
    
    m_finishButton->setStyleSheet(
        "QPushButton { "
        "background-color: #f44336; "
        "color: white; "
        "border: none; "
        "padding: 10px 20px; "
        "border-radius: 4px; "
        "font-weight: bold; "
        "font-size: 14px; "
        "min-width: 80px; "
        "min-height: 32px; "
        "} "
        "QPushButton:hover { "
        "background-color: #da190b; "
        "} "
        "QPushButton:pressed { "
        "background-color: #b71c1c; "
        "}"
    );
    
    m_captureInfoLabel->setStyleSheet(
        "QLabel { "
        "color: white; "
        "background-color: rgba(0, 0, 0, 128); "
        "border-radius: 4px; "
        "padding: 8px 12px; "
        "font-size: 14px; "
        "}"
    );
    
    // 布局
    m_captureLayout->addWidget(m_captureInfoLabel);
    m_captureLayout->addWidget(m_saveButton);
    m_captureLayout->addWidget(m_finishButton);
    m_captureLayout->setSpacing(BUTTON_SPACING);
    m_captureLayout->setContentsMargins(5, 5, 5, 5);
    
    // 设置按钮容器的最小高度
    m_captureContainer->setMinimumHeight(50);
    
    // 连接信号 - 保存截图按钮保存当前截图，结束截图按钮结束截图
    connect(m_saveButton, &QPushButton::clicked, this, &SelectionOverlay::onSaveClicked);
    connect(m_finishButton, &QPushButton::clicked, this, &SelectionOverlay::onFinishClicked);
    
    // 初始隐藏截图UI
    m_captureContainer->hide();
}

void SelectionOverlay::startSelection()
{
    m_isSelecting = false;
    m_isSelected = false;
    m_isCapturing = false;
    m_selectedRect = QRect();
    m_startPoint = QPoint();
    m_endPoint = QPoint();
    
    hideButtons();
    hideCaptureUI();
    setCursor(Qt::CrossCursor);
    show();
    setFocus();
    update();
}

void SelectionOverlay::cancelSelection()
{
    m_isCapturing = false;
    hideCaptureUI();
    hide();
    emit selectionCancelled();
}

QRect SelectionOverlay::getSelectedRect() const
{
    if (m_selectedRect.isEmpty()) {
        return QRect();
    }
    
    // 将widget坐标转换为屏幕坐标
    QRect screenRect = m_selectedRect;
    
    // 添加屏幕偏移量
    screenRect.translate(m_screenGeometry.topLeft());
    
    qDebug() << "Widget坐标：" << m_selectedRect;
    qDebug() << "屏幕坐标：" << screenRect;
    qDebug() << "屏幕偏移：" << m_screenGeometry.topLeft();
    
    return screenRect;
}

void SelectionOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    
    // 在截图模式下，减少背景遮罩的透明度，让用户可以更清楚看到内容
    if (m_isCapturing) {
        painter.fillRect(rect(), QColor(0, 0, 0, 50));  // 更透明的遮罩
    } else {
        painter.fillRect(rect(), QColor(0, 0, 0, 100)); // 选择模式下正常遮罩
    }
    
    // 如果有选择区域，绘制透明区域和虚线框
    if (!m_selectedRect.isEmpty()) {
        // 完全清除选择区域的遮罩，使其完全透明
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(m_selectedRect, Qt::transparent);
        
        // 始终绘制虚线框，让用户可以看到选择区域
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawDashedRect(painter, m_selectedRect);
        
        // 只在选择模式下显示区域信息，截图模式下不显示
        if (!m_isCapturing) {
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 12));
            QString info = QString("%1 x %2").arg(m_selectedRect.width()).arg(m_selectedRect.height());
            QRect textRect = painter.fontMetrics().boundingRect(info);
            
            QPoint textPos = m_selectedRect.topLeft() + QPoint(5, -5);
            if (textPos.y() < textRect.height()) {
                textPos.setY(m_selectedRect.bottom() + textRect.height() + 5);
            }
            
            painter.fillRect(textPos.x() - 2, textPos.y() - textRect.height() - 2,
                            textRect.width() + 4, textRect.height() + 4,
                            QColor(0, 0, 0, 128));
            painter.drawText(textPos, info);
        }
    }
    
    // 绘制当前拖拽的矩形
    if (m_isSelecting && !m_startPoint.isNull() && !m_endPoint.isNull()) {
        QRect currentRect(m_startPoint, m_endPoint);
        currentRect = currentRect.normalized();
        
        // 完全清除拖拽区域，使其完全透明
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(currentRect, Qt::transparent);
        
        // 始终绘制拖拽虚线框
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        drawDashedRect(painter, currentRect);
    }
}

void SelectionOverlay::mousePressEvent(QMouseEvent *event)
{
    // 在截图模式下禁用鼠标选择
    if (m_isCapturing) {
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_isSelecting = true;
        m_isSelected = false;
        m_selectedRect = QRect();
        hideButtons();
        update();
    }
    QWidget::mousePressEvent(event);
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent *event)
{
    // 在截图模式下禁用鼠标选择
    if (m_isCapturing || !m_isSelecting) {
        return;
    }
    
    m_endPoint = event->pos();
    m_selectedRect = QRect(m_startPoint, m_endPoint).normalized();
    update();
    QWidget::mouseMoveEvent(event);
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    // 在截图模式下禁用鼠标选择
    if (m_isCapturing) {
        return;
    }
    
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_endPoint = event->pos();
        m_selectedRect = QRect(m_startPoint, m_endPoint).normalized();
        m_isSelecting = false;
        
        if (m_selectedRect.width() > 10 && m_selectedRect.height() > 10) {
            m_isSelected = true;
            showButtons();
            updateButtonPosition();
            
            // 更新信息标签
            if (m_infoLabel) {
                m_infoLabel->setText(QString("区域: %1×%2")
                                   .arg(m_selectedRect.width())
                                   .arg(m_selectedRect.height()));
            }
        } else {
            m_selectedRect = QRect();
            m_isSelected = false;
        }
        
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void SelectionOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelSelection();
    }
    QWidget::keyPressEvent(event);
}

void SelectionOverlay::onConfirmClicked()
{
    if (m_isSelected && !m_selectedRect.isEmpty()) {
        // 获取屏幕信息
        QScreen* screen = QApplication::primaryScreen();
        if (!screen) {
            qDebug() << "无法获取主屏幕信息";
            return;
        }
        
        // 获取屏幕几何信息
        QRect screenGeometry = screen->geometry();
        qDebug() << "屏幕几何信息：" << screenGeometry;
        qDebug() << "选择区域（窗口坐标）：" << m_selectedRect;
        
        // 将窗口坐标转换为屏幕坐标
        QRect screenRect = m_selectedRect;
        
        // 如果选择覆盖层是全屏的，坐标应该已经是屏幕坐标
        // 但为了确保，我们验证一下
        if (geometry() != screenGeometry) {
            qDebug() << "选择覆盖层几何：" << geometry() << "与屏幕几何不匹配：" << screenGeometry;
            // 调整坐标
            QPoint offset = geometry().topLeft() - screenGeometry.topLeft();
            screenRect.translate(-offset.x(), -offset.y());
        }
        
        // 确保区域在屏幕范围内
        screenRect = screenRect.intersected(screenGeometry);
        
        qDebug() << "最终选择区域（屏幕坐标）：" << screenRect;
        
        // 切换到截图模式而不是隐藏
        switchToCaptureMode();
        
        emit selectionConfirmed(screenRect);
    }
}

void SelectionOverlay::onCancelClicked()
{
    cancelSelection();
}

void SelectionOverlay::onSaveClicked()
{
    emit saveRequested();
}

void SelectionOverlay::onFinishClicked()
{
    emit captureFinished();
    hide();
}

void SelectionOverlay::switchToCaptureMode()
{
    m_isCapturing = true;
    
    // 隐藏选择模式的按钮
    hideButtons();
    
    // 显示截图模式的UI
    showCaptureUI();
    
    // 更新鼠标样式
    setCursor(Qt::ArrowCursor);
    
    // 禁用鼠标选择
    m_isSelecting = false;
    
    qDebug() << "切换到截图模式";
}

void SelectionOverlay::showCaptureUI()
{
    if (m_captureContainer && !m_selectedRect.isEmpty()) {
        // 计算截图UI位置（选择区域下方）
        int buttonWidth = m_captureContainer->sizeHint().width();
        int buttonHeight = BUTTON_HEIGHT;
        
        int x = m_selectedRect.center().x() - buttonWidth / 2;
        int y = m_selectedRect.bottom() + BUTTON_SPACING;
        
        // 确保按钮不超出屏幕
        x = qMax(0, qMin(x, width() - buttonWidth));
        y = qMax(0, qMin(y, height() - buttonHeight));
        
        m_captureContainer->setGeometry(x, y, buttonWidth, buttonHeight);
        m_captureContainer->show();
        
        qDebug() << "显示截图UI，位置：" << x << "," << y;
    }
}

void SelectionOverlay::hideCaptureUI()
{
    if (m_captureContainer) {
        m_captureContainer->hide();
    }
}

void SelectionOverlay::updateButtonPosition()
{
    if (!m_buttonContainer || m_selectedRect.isEmpty()) {
        return;
    }
    
    int buttonWidth = m_buttonContainer->sizeHint().width();
    int buttonHeight = BUTTON_HEIGHT;
    
    // 计算按钮位置（选择区域下方）
    int x = m_selectedRect.center().x() - buttonWidth / 2;
    int y = m_selectedRect.bottom() + BUTTON_SPACING;
    
    // 确保按钮不超出屏幕
    x = qMax(0, qMin(x, width() - buttonWidth));
    y = qMax(0, qMin(y, height() - buttonHeight));
    
    m_buttonContainer->setGeometry(x, y, buttonWidth, buttonHeight);
}

void SelectionOverlay::drawDashedRect(QPainter& painter, const QRect& rect)
{
    // 只绘制虚线边框，不填充任何颜色
    painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);  // 确保不填充任何颜色
    painter.drawRect(rect);
    
    // 绘制角落的小方块（空心）
    const int cornerSize = 6;
    painter.setPen(QPen(Qt::red, 2, Qt::SolidLine));
    painter.setBrush(Qt::NoBrush);  // 角落方块也不填充
    
    // 四个角落 - 绘制空心方块
    QRect topLeftCorner(rect.topLeft().x() - cornerSize/2, rect.topLeft().y() - cornerSize/2, cornerSize, cornerSize);
    QRect topRightCorner(rect.topRight().x() - cornerSize/2, rect.topRight().y() - cornerSize/2, cornerSize, cornerSize);
    QRect bottomLeftCorner(rect.bottomLeft().x() - cornerSize/2, rect.bottomLeft().y() - cornerSize/2, cornerSize, cornerSize);
    QRect bottomRightCorner(rect.bottomRight().x() - cornerSize/2, rect.bottomRight().y() - cornerSize/2, cornerSize, cornerSize);
    
    painter.drawRect(topLeftCorner);
    painter.drawRect(topRightCorner);
    painter.drawRect(bottomLeftCorner);
    painter.drawRect(bottomRightCorner);
}

void SelectionOverlay::showButtons()
{
    if (m_buttonContainer) {
        m_buttonContainer->show();
    }
}

void SelectionOverlay::hideButtons()
{
    if (m_buttonContainer) {
        m_buttonContainer->hide();
    }
} 
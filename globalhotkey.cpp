#include "globalhotkey.h"
#include <QDebug>

#ifdef Q_OS_MACOS
GlobalHotkey *GlobalHotkey::s_instance = nullptr;

OSStatus GlobalHotkey::hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData)
{
    Q_UNUSED(nextHandler)
    Q_UNUSED(userData)
    
    EventHotKeyID hotKeyID;
    GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hotKeyID), NULL, &hotKeyID);
    
    if (s_instance && hotKeyID.id == s_instance->m_hotKeyID) {
        qDebug() << "全局快捷键被触发!";
        emit s_instance->activated();
    }
    
    return noErr;
}
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_registered(false)
#ifdef Q_OS_MACOS
    , m_hotKeyRef(nullptr)
    , m_hotKeyID(1)
#endif
{
#ifdef Q_OS_MACOS
    s_instance = this;
    
    // 安装事件处理器
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyPressed;
    
    InstallApplicationEventHandler(hotKeyHandler, 1, &eventType, this, NULL);
#endif
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
#ifdef Q_OS_MACOS
    s_instance = nullptr;
#endif
}

bool GlobalHotkey::registerHotkey(const QKeySequence &keySequence)
{
    if (m_registered) {
        unregisterHotkey();
    }
    
    m_keySequence = keySequence;
    
#ifdef Q_OS_MACOS
    // 解析快捷键
    QString keyString = keySequence.toString();
    qDebug() << "注册全局快捷键:" << keyString;
    
    // 简化实现：只处理 Ctrl+Shift+A
    if (keyString == "Ctrl+Shift+A") {
        UInt32 keyCode = kVK_ANSI_A; // A key
        UInt32 modifiers = controlKey | shiftKey;
        
        EventHotKeyID hotKeyID;
        hotKeyID.signature = 'RBSH'; // RabbitShot signature
        hotKeyID.id = m_hotKeyID;
        
        OSStatus status = RegisterEventHotKey(keyCode, modifiers, hotKeyID, GetApplicationEventTarget(), 0, &m_hotKeyRef);
        
        if (status == noErr) {
            m_registered = true;
            qDebug() << "全局快捷键注册成功!";
            return true;
        } else {
            qDebug() << "全局快捷键注册失败，错误码:" << status;
            return false;
        }
    } else {
        qDebug() << "暂不支持此快捷键组合:" << keyString;
        return false;
    }
#else
    qDebug() << "此平台不支持全局快捷键";
    return false;
#endif
}

void GlobalHotkey::unregisterHotkey()
{
    if (!m_registered) {
        return;
    }
    
#ifdef Q_OS_MACOS
    if (m_hotKeyRef) {
        UnregisterEventHotKey(m_hotKeyRef);
        m_hotKeyRef = nullptr;
    }
#endif
    
    m_registered = false;
    qDebug() << "全局快捷键已取消注册";
} 
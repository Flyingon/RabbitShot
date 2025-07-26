#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QKeySequence>

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#endif

class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    bool registerHotkey(const QKeySequence &keySequence);
    void unregisterHotkey();
    bool isRegistered() const { return m_registered; }

signals:
    void activated();

private:
    bool m_registered;
    QKeySequence m_keySequence;

#ifdef Q_OS_MACOS
    EventHotKeyRef m_hotKeyRef;
    UInt32 m_hotKeyID;
    static OSStatus hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData);
    static GlobalHotkey *s_instance;
#endif
};

#endif // GLOBALHOTKEY_H 
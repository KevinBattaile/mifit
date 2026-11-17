#ifndef CursorArea_h
#define CursorArea_h
#include <QGraphicsItem>
#include <QtQuick/QQuickItem>

class CursorArea : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QString shape READ cursor WRITE setCursor)

public:
    explicit CursorArea(QQuickItem *parent = nullptr);

    QString cursor() const;
    void setCursor(const QString &shape);
};

#endif // CursorArea_h

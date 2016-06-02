#ifndef VIEW3D_H
#define VIEW3D_H

#include <QGLWidget>
#include <QTimer>
#include <QPoint>
#include <QRect>

#include <QKeyEvent>
#include <QMouseEvent>

#include "glarray.h"

class View3D : public QGLWidget
{
    Q_OBJECT

public:
    View3D(QWidget* parent = 0);

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);

    void initMap();

    virtual void mouseMoveEvent(QMouseEvent* e);
    virtual void mousePressEvent(QMouseEvent* e);
    virtual void mouseReleaseEvent(QMouseEvent* e);
    virtual void wheelEvent(QWheelEvent* e);

    virtual void keyPressEvent(QKeyEvent* e);
    virtual void keyReleaseEvent(QKeyEvent* e);

    void becameVisible();

public slots:
    void repaintTimerHandler();

private:
    void setPerspective(float fov);

    QTimer* repaintTimer;
    bool wasVisible;

    float posX;
    float posY;
    float posZ;

    float angX;
    float angY;
    float angZ;

    int mouseXLast;
    int mouseYLast;
    bool mouseIgnore;

    bool moveForward;
    bool moveBackward;
    bool moveLeft;
    bool moveRight;
};

#endif // VIEW3D_H

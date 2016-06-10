#ifndef VIEW2D_H
#define VIEW2D_H

#include <QGLWidget>
#include <QTimer>
#include <QPoint>
#include <QRect>

#include <QKeyEvent>
#include <QMouseEvent>

#include "glarray.h"

class View2D : public QGLWidget
{
    Q_OBJECT

public:
    View2D(QWidget* parent = 0);

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);

    void initMap();

    void centerOnPoint(float x, float y);
    void scrollTo(float x, float y) { scrollX = x; scrollY = y; }
    float getScrollX() { return scrollX; }
    float getScrollY() { return scrollY; }

    float getMouseX() { return mouseXScaled; }
    float getMouseY() { return mouseYScaled; }

    virtual void mouseMoveEvent(QMouseEvent* e);
    virtual void mousePressEvent(QMouseEvent* e);
    virtual void mouseReleaseEvent(QMouseEvent* e);
    virtual void wheelEvent(QWheelEvent* e);

    virtual void keyPressEvent(QKeyEvent* e);
    virtual void keyReleaseEvent(QKeyEvent* e);

public slots:
    void repaintTimerHandler();

private:
    void setClipRect(int x, int y, int w, int h);

    QTimer* repaintTimer;

    float scrollX;
    float scrollY;
    float scale;
    int gridSize;

    int mouseX;
    int mouseY;

    float mouseXScaled;
    float mouseYScaled;

    //
    bool scrolling;
    int scrollMouseX;
    int scrollMouseY;

    GLArray linesArray;
    bool linesUpdate;
};

#endif // VIEW2D_H

#ifndef VIEWAREA_H
#define VIEWAREA_H

#include <QGLWidget>

class ViewArea : public QGLWidget
{
public:
    ViewArea(QWidget* parent = 0);

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);

private:
    void setClipRect2D(int x, int y, int w, int h);
};

#endif // VIEWAREA_H

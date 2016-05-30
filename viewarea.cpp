#include "viewarea.h"
#include <QDebug>

static const QGLFormat& GetViewAreaFormat()
{
    static QGLFormat fmt;
    fmt = QGLFormat();
    fmt.setVersion(2, 0);
    fmt.setProfile(QGLFormat::CompatibilityProfile);
    fmt.setSampleBuffers(true);
    return fmt;
}

ViewArea::ViewArea(QWidget* parent) : QGLWidget(GetViewAreaFormat(), parent)
{

}

void ViewArea::initializeGL()
{
    /*
    QString versionString(QLatin1String(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    qDebug() << "Driver Version String:" << versionString;
    qDebug() << "Current Context:" << format();*/
    setClipRect2D(0, 0, width(), height());
}

void ViewArea::paintGL()
{
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
        glVertex2i(32, 32);
        glVertex2i(64, 32);
        glVertex2i(64, 64);
        glVertex2i(32, 64);
    glEnd();
}


void ViewArea::resizeGL(int width, int height)
{
    // 2d = ortho
    setClipRect2D(0, 0, width, height);
}

void ViewArea::setClipRect2D(int x, int y, int w, int h)
{
    glViewport(x, height()-(y+h), w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(x, x+w, y+h, y, -65536, 65536); // I doubt it will ever draw 64k sprites at once
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

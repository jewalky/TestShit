#include "View2D.h"
#include <QDebug>
#include "mainwindow.h"
#include "glarray.h"

static const QGLFormat& GetView2DFormat()
{
    static QGLFormat fmt;
    fmt = QGLFormat();
    fmt.setVersion(2, 0);
    fmt.setProfile(QGLFormat::CompatibilityProfile);
    //fmt.setSampleBuffers(true);
    return fmt;
}

View2D::View2D(QWidget* parent) : QGLWidget(GetView2DFormat(), parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    repaintTimer = new QTimer(this);
    connect(repaintTimer, SIGNAL(timeout()), this, SLOT(repaintTimerHandler()));
    repaintTimer->setInterval(20);
    repaintTimer->start();

    scrollX = 0;
    scrollY = 0;
    scale = 0.5;
    gridSize = 32;

    linesUpdate = false;
}

void View2D::initializeGL()
{
    setClipRect(0, 0, width(), height());
    glLineWidth(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void View2D::paintGL()
{
    setClipRect(0, 0, width(), height());

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glTranslatef(-scrollX, -scrollY, 0); // scroll is in physical pixels.
    glScalef(scale, scale, 1);

    DoomMap* cmap = MainWindow::get()->getMap();
    if (!cmap)
        return;

    //
    if (linesUpdate)
    {
        linesUpdate = false;
        linesArray.update();
    }

    linesArray.draw(GL_LINES);

    // draw sectors HUEHUEHUEHUEHUE
    for (int i = 0; i < cmap->sectors.size(); i++)
    {
        DoomMapSector& sec = cmap->sectors[i];
        sec.triangles.draw(GL_TRIANGLES);
    }
}


void View2D::resizeGL(int width, int height)
{
    //setClipRect(0, 0, width, height);
    // this is done each frame anyway
    update();
}

void View2D::repaintTimerHandler()
{
    // do something else? otherwise just remove this and connect to repaint()
    //update();
}

void View2D::setClipRect(int x, int y, int w, int h)
{
    glViewport(x, height()-(y+h), w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(x, x+w, y+h, y, -65536, 65536); // I doubt it will ever draw 64k sprites at once
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void View2D::centerOnPoint(float x, float y)
{
    // get scaled point
    x /= scale;
    y /= scale;
    scrollX = x - width() / 2;
    scrollY = y - height() / 2;
}

void View2D::initMap()
{
    centerOnPoint(0, 0);
    MainWindow::get()->setMouseXY(mouseXScaled, mouseYScaled);
    MainWindow::get()->setScale(scale);

    DoomMap* cmap = MainWindow::get()->getMap();
    linesArray.vertices.clear();
    if (cmap)
    {
        //qDebug("MAP FOUND");
        for (int i = 0; i < cmap->linedefs.size(); i++)
        {
            // 1-2sided
            DoomMapLinedef& linedef = cmap->linedefs[i];
            //if (linedef.getBack())
            //    glColor4ub(128, 128, 128, 255);
            //else glColor4ub(255, 255, 255, 255);

            quint8 c = !!linedef.getBack() ? 128 : 255;

            GLVertex v1;
            v1.x = linedef.getV1()->x;
            v1.y = -linedef.getV1()->y;
            v1.r = v1.g = v1.b = c;
            v1.a = 255;
            GLVertex v2;
            v2.x = linedef.getV2()->x;
            v2.y = -linedef.getV2()->y;
            v2.r = v2.g = v2.b = c;
            v2.a = 255;
            linesArray.vertices.append(v1);
            linesArray.vertices.append(v2);
        }
    }
    linesUpdate = true;
}

void View2D::mouseMoveEvent(QMouseEvent* e)
{
    mouseX = e->pos().x();
    mouseY = e->pos().y();

    if (scrolling)
    {
        int oScrollX = scrollX;
        int oScrollY = scrollY;
        scrollX = scrollMouseX-mouseX;
        scrollY = scrollMouseY-mouseY;
        if (oScrollX != scrollX || oScrollY != scrollY)
            update();
    }

    mouseXScaled = (float)(mouseX + scrollX) / scale;
    mouseYScaled = -((float)(mouseY + scrollY) / scale);

    MainWindow::get()->setMouseXY(mouseXScaled, mouseYScaled);
}

void View2D::mousePressEvent(QMouseEvent* e)
{

}

void View2D::mouseReleaseEvent(QMouseEvent* e)
{

}

void View2D::wheelEvent(QWheelEvent* e)
{
    //qDebug("delta = %d", e->delta()); // positive = up, negative = down
    // 120, -120

    // remember current scale
    float oldScale = scale;

    float deltaF = (float)e->delta() / 120;
    scale += deltaF * (scale / 10);
    if (scale < 0.01)
        scale = 0.01;
    if (scale > 90)
        scale = 90;

    // change scroll according to scale change.
    scrollX = (scrollX+mouseX) / oldScale * scale - mouseX;
    scrollY = (scrollY+mouseY) / oldScale * scale - mouseY;

    //qDebug("scale = %f; scrollX = %f; scrollY = %f", scale, scrollX, scrollY);
    MainWindow::get()->setScale(scale);
    update();
}

void View2D::keyPressEvent(QKeyEvent *e)
{
    // get absolute mousex/y
    if (e->key() == Qt::Key_Space)
    {
        scrollMouseX = scrollX+mouseX;
        scrollMouseY = scrollY+mouseY;
        scrolling = true;
    }
    else if (e->key() == Qt::Key_Return ||
             e->key() == Qt::Key_Enter)
    {
        MainWindow::get()->set3DMode(true);
    }
}

void View2D::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space)
    {
        scrolling = false;
    }
}


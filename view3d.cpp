#include "view3d.h"
#include <QDebug>
#include "mainwindow.h"
#include "glarray.h"
#include <cmath> // M_PI, tan()
#include <QApplication>

static const QGLFormat& GetView3DFormat()
{
    static QGLFormat fmt;
    fmt = QGLFormat();
    fmt.setVersion(2, 0);
    fmt.setProfile(QGLFormat::CompatibilityProfile);
    //fmt.setSampleBuffers(true);
    return fmt;
}

View3D::View3D(QWidget* parent) : QGLWidget(GetView3DFormat(), parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    repaintTimer = new QTimer(this);
    connect(repaintTimer, SIGNAL(timeout()), this, SLOT(repaintTimerHandler()));
    repaintTimer->setInterval(20);
    repaintTimer->start();

    wasVisible = false;
    mouseXLast = mouseYLast = -1;

    mouseIgnore = false;
}

void View3D::initializeGL()
{
    glLineWidth(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void View3D::paintGL()
{
    setPerspective(140);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // draw a quad.
    /*glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
        glVertex3f(-32, -32, 64);
        glVertex3f(32, -32, 64);
        glVertex3f(32, 32, 64);
        glVertex3f(-32, 32, 64);
        //glVertex3f(-32, -32, 0);
        //glVertex3f(32, -32, 0);
        //glVertex3f(32, 32, 0);
        //glVertex3f(-32, 32, 0);
    glEnd();*/

    DoomMap* cmap = MainWindow::get()->getMap();
    if (!cmap)
        return;

    float rdist = 1024;

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, rdist-64);
    glFogf(GL_FOG_END, rdist);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    // hueeeeeeeeeeeeeeeeeeeeeeeeee
    for (int i = 0; i < cmap->sectors.size(); i++)
    {
        DoomMapSector* sector = &cmap->sectors[i];
        // dont render if too far
        if (!sector->isAnyWithin(posX, -posY, rdist+64))
            continue;

        // draw sector's floor and ceiling first
        GLArray tri_floor = sector->triangles, tri_ceiling = sector->triangles;
        for (int j = 0; j < sector->triangles.vertices.size(); j++)
        {
            GLVertex& fv = tri_floor.vertices[j];
            GLVertex& cv = tri_ceiling.vertices[j];

            fv.z = sector->zatFloor(fv.x, fv.y);
            cv.z = sector->zatCeiling(cv.x, cv.y);

            fv.r = fv.g = fv.b = (sector->texturefloor=="F_SKY1")?0:64;
            fv.a = 255;

            cv.r = cv.g = cv.b = (sector->textureceiling=="F_SKY1")?0:64;
            cv.a = 255;
        }

        // draw lines around the sector.
        GLArray quads;
        for (int j = 0; j < sector->linedefs.size(); j++)
        {
            DoomMapLinedef* linedef = sector->linedefs[j];

            DoomMapVertex* v1 = linedef->getV1(sector);
            DoomMapVertex* v2 = linedef->getV2(sector);

            // dont draw 2sided yet
            GLVertex vv1, vv2, vv3, vv4;
            vv1.x = v1->x;
            vv1.y = -v1->y;
            vv1.z = sector->zatCeiling(vv1.x, -vv1.y);

            vv2.x = v2->x;
            vv2.y = -v2->y;
            vv2.z = sector->zatCeiling(vv2.x, -vv2.y);

            vv3.x = v2->x;
            vv3.y = -v2->y;
            vv3.z = sector->zatFloor(vv3.x, -vv3.y);

            vv4.x = v1->x;
            vv4.y = -v1->y;
            vv4.z = sector->zatFloor(vv4.x, -vv4.y);

            QLineF line = QLineF(v1->x, v1->y, v2->x, v2->y);
            float wvl = -16;
            float whl = 16;
            // scary formula taken from zdoom
            float lite = whl
                + fabs(atan(double(line.dx()) / line.dy()) / 1.57079)
                * (wvl - whl);

            quint8 c = 128 + lite;

            vv1.r=vv1.g=vv1.b=c;
            vv2.r=vv2.g=vv2.b=c;
            vv3.r=vv3.g=vv3.b=c;
            vv4.r=vv4.g=vv4.b=c;

            // now do different processing based on line type.
            // single-sided line:
            if (!linedef->getBack())
            {
                quads.vertices.append(vv1);
                quads.vertices.append(vv2);
                quads.vertices.append(vv3);
                quads.vertices.append(vv4);
            }
            else
            {
                // get the other sector
                DoomMapSector* other = (linedef->getFront()->getSector() == sector) ? linedef->getBack()->getSector() : linedef->getFront()->getSector();
                // make ourselves some more vertices for top and bottom linedef ending. also don't draw certain wall parts if other sector height is larger than current.
                // or draw. who the fuck cares.
                GLVertex ov1 = vv1, ov2 = vv2, ov3 = vv3, ov4 = vv4;
                ov1.z = std::min(sector->zatCeiling(ov1.x, -ov1.y), other->zatCeiling(ov1.x, -ov1.y));
                ov2.z = std::min(sector->zatCeiling(ov2.x, -ov2.y), other->zatCeiling(ov2.x, -ov2.y));
                ov3.z = std::max(sector->zatFloor(ov3.x, -ov3.y), other->zatFloor(ov3.x, -ov3.y));
                ov4.z = std::max(sector->zatFloor(ov4.x, -ov4.y), other->zatFloor(ov4.x, -ov4.y));
                // top texture
                quads.vertices.append(vv1);
                quads.vertices.append(vv2);
                quads.vertices.append(ov2);
                quads.vertices.append(ov1);
                // bottom texture
                quads.vertices.append(ov4);
                quads.vertices.append(ov3);
                quads.vertices.append(vv3);
                quads.vertices.append(vv4);
            }
        }

        glColor4ub(255, 255, 255, 255);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1, 0.5);

        glFrontFace(GL_CCW);
        tri_floor.draw(GL_TRIANGLES);
        glFrontFace(GL_CW);
        tri_ceiling.draw(GL_TRIANGLES);

        quads.draw(GL_QUADS);

        glDisable(GL_POLYGON_OFFSET_FILL);
        //lines.draw(GL_LINES);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
}


void View3D::resizeGL(int width, int height)
{
    //setClipRect(0, 0, width, height);
    // this is done each frame anyway
    update();
}

void View3D::repaintTimerHandler()
{
    // do something else? otherwise just remove this and connect to repaint()
    //update();

    if (isVisible() && !wasVisible)
    {
        //grabMouse();
        //grabKeyboard();
        becameVisible();
    }
    else if (!isVisible() && wasVisible)
    {
        //releaseMouse();
        //releaseKeyboard();
    }

    if (!isVisible())
        return;

    // move the player
    float dx = 0;
    float dy = 0;
    if (moveForward) dy += 1;
    if (moveBackward) dy -= 1;
    if (moveLeft) dx -= 1;
    if (moveRight) dx += 1;

    float movemul = 6;
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
        movemul *= 3;

    QLineF vertvec(0, 0, 1, 0);
    vertvec.setAngle(angX);
    posZ -= dy*vertvec.dy()*movemul;
    QLineF movevec(0, 0, dx, dy);
    movevec.setLength(vertvec.dx());
    movevec.setAngle(movevec.angle()-angZ);
    posX += movevec.dx()*movemul;
    posY -= movevec.dy()*movemul;

    //
    update();
}

void View3D::initMap()
{
    DoomMap* cmap = MainWindow::get()->getMap();

    //
    posX = posY = posZ = 0;
    posZ = 48;
    angX = angY = angZ = 0;

    mouseXLast = mouseYLast = -1;
}

void View3D::mouseMoveEvent(QMouseEvent* e)
{
    int deltaX = 0, deltaY = 0;

    // check if center. ignore events that point to widget's center, as these are our own events.
    QPoint center(width()/2, height()/2);

    if (mouseXLast >= 0 && mouseYLast >= 0)
    {
        deltaX = center.x()-e->pos().x();
        deltaY = center.y()-e->pos().y();

        float mper1 = 4;
        angX += (float)deltaY / mper1;
        if (angX < -90) angX = -90;
        else if (angX > 90) angX = 90;

        angZ += (float)deltaX / mper1;
        while (angZ < 0)
            angZ += 360;
        while (angZ > 360)
            angZ -= 360;

        //qDebug("angX = %f", angX);
    }

    mouseXLast = e->x();
    mouseYLast = e->y();

    QCursor c = cursor();
    c.setPos(mapToGlobal(center));
    c.setShape(Qt::BlankCursor);
    setCursor(c);
}

void View3D::mousePressEvent(QMouseEvent* e)
{

}

void View3D::mouseReleaseEvent(QMouseEvent* e)
{

}

void View3D::wheelEvent(QWheelEvent* e)
{

}

void View3D::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return ||
        e->key() == Qt::Key_Enter)
    {
        MainWindow::get()->set3DMode(false);
    }
    else if (e->key() == Qt::Key_W)
    {
        moveForward = true;
    }
    else if (e->key() == Qt::Key_A)
    {
        moveLeft = true;
    }
    else if (e->key() == Qt::Key_S)
    {
        moveBackward = true;
    }
    else if (e->key() == Qt::Key_D)
    {
        moveRight = true;
    }
}

void View3D::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_W)
    {
        moveForward = false;
    }
    else if (e->key() == Qt::Key_A)
    {
        moveLeft = false;
    }
    else if (e->key() == Qt::Key_S)
    {
        moveBackward = false;
    }
    else if (e->key() == Qt::Key_D)
    {
        moveRight = false;
    }
}

void View3D::becameVisible()
{
    // todo reset mouse
    mouseXLast = mouseYLast = -1;
}

void View3D::setPerspective(float fov)
{
    glViewport(0, 0, width(), height());

    float aspect = (float)width() / height();
    float fovy = fov/aspect;
    float zNear = 1;
    float zFar = 65536;

    GLdouble xmin, xmax, ymin, ymax;

    ymax = zNear * tan(fovy * M_PI / 360.0);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //
    glRotatef(90-angX, 1, 0, 0);
    glRotatef(angZ, 0, 0, 1);
    glScalef(1, 1, -1);
    glTranslatef(-posX, -posY, -posZ);
}

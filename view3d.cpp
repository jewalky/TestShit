#include "view3d.h"
#include <QDebug>
#include "mainwindow.h"
#include "glarray.h"
#include <cmath> // M_PI, tan()
#include <QApplication>
#include "data/texman.h"

static const QGLFormat& GetView3DFormat()
{
    static QGLFormat fmt;
    fmt = QGLFormat();
    fmt.setVersion(2, 0);
    fmt.setProfile(QGLFormat::CompatibilityProfile);
    //fmt.setSampleBuffers(true);
    return fmt;
}

View3D::View3D(QWidget* parent) : QGLWidget(GetView3DFormat(), parent, MainWindow::get()->getSharedGLWidget())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    repaintTimer = new QTimer(this);
    connect(repaintTimer, SIGNAL(timeout()), this, SLOT(repaintTimerHandler()));
    repaintTimer->setInterval(20);
    repaintTimer->start();

    wasVisible = false;
    mouseXLast = mouseYLast = -1;

    running = false;
}

void View3D::initializeGL()
{
    glLineWidth(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ewwwwwwwwwwwwwwwwwwwwwwwww this function is so ugly
static void View3D_Helper_SetTextureOffsets(GLVertex& vv1, GLVertex& vv2, GLVertex& vv3, GLVertex& vv4, DoomMapSector* sector, DoomMapSector* other, DoomMapLinedef* linedef, DoomMapSidedef* sidedef, QLineF& line, TexTexture* tex, bool top)
{
    float xbase = sidedef->offsetx;
    float ybase = sidedef->offsety;
    // lower unpegged means the texture should have origin at it's bottom, not top.
    if (linedef->dontpegbottom && !top)
    {
        int highestfloor = sector->heightfloor;
        if (other) highestfloor = other->heightfloor;
        int ptd = (sector->heightceiling-highestfloor) % tex->getHeight(); // 272 % 128
        if (!other) ptd = -ptd;
        ybase += ptd;
    }
    else if (!linedef->dontpegtop && top)
    {
        int ptd = tex->getHeight() - (sector->heightceiling - other->heightceiling);
        ybase += ptd;
    }
    xbase /= tex->getWidth();
    ybase /= tex->getHeight();
    float xlen = line.length() / tex->getWidth();
    float ylen1 = (vv1.z-vv4.z) / tex->getHeight();
    float ylen2 = (vv2.z-vv3.z) / tex->getHeight();

    vv1.u = xbase;
    vv1.v = ybase;
    vv2.u = xbase+xlen;
    vv2.v = ybase;
    vv3.u = xbase+xlen;
    vv3.v = ybase+ylen2;
    vv4.u = xbase;
    vv4.v = ybase+ylen1;
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
    glDisable(GL_BLEND);
    for (int i = 0; i < cmap->sectors.size(); i++)
    {
        DoomMapSector* sector = &cmap->sectors[i];
        // dont render if too far
        if (!sector->isAnyWithin(posX, -posY, rdist+64))
            continue;

        // find ceiling texture
        // this NEVER returns null. at most - embedded resource that says "BROKEN TEXTURE"
        TexTexture* flatceiling = Tex_GetTexture(sector->textureceiling, TexTexture::Flat, true);
        TexTexture* flatfloor = Tex_GetTexture(sector->texturefloor, TexTexture::Flat, true);

        // draw sector's floor and ceiling first
        GLArray tri_floor = sector->triangles, tri_ceiling = sector->triangles;
        for (int j = 0; j < sector->triangles.vertices.size(); j++)
        {
            GLVertex& fv = tri_floor.vertices[j];
            GLVertex& cv = tri_ceiling.vertices[j];

            fv.z = sector->zatFloor(fv.x, fv.y);
            cv.z = sector->zatCeiling(cv.x, cv.y);

            fv.r = fv.g = fv.b = sector->lightlevel;
            fv.a = 255;

            cv.r = cv.g = cv.b = sector->lightlevel;
            cv.a = 255;

            // set floor u/v
            fv.u = fv.x / flatfloor->getWidth();
            fv.v = fv.y / flatfloor->getHeight();

            // set ceiling u/v
            cv.u = cv.x / flatceiling->getWidth();
            cv.v = cv.y / flatceiling->getHeight();
        }

        // draw lines around the sector.
        for (int j = 0; j < sector->linedefs.size(); j++)
        {
            DoomMapLinedef* linedef = sector->linedefs[j];

            DoomMapVertex* v1 = linedef->getV1(sector);
            DoomMapVertex* v2 = linedef->getV2(sector);

            DoomMapSidedef* sidedef = linedef->getSidedef(sector);
            if (!sidedef)
                continue; // wtf?

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
            lite += sector->lightlevel;
            if (lite > 255) lite = 255;
            if (lite < 0) lite = 0;

            quint8 c = lite;

            vv1.r=vv1.g=vv1.b=c;
            vv2.r=vv2.g=vv2.b=c;
            vv3.r=vv3.g=vv3.b=c;
            vv4.r=vv4.g=vv4.b=c;

            // now do different processing based on line type.
            // single-sided line:
            if (!linedef->getBack())
            {
                TexTexture* tex = Tex_GetTexture(sidedef->texturemiddle, TexTexture::Texture, true);

                View3D_Helper_SetTextureOffsets(vv1, vv2, vv3, vv4, sector, 0, linedef, sidedef, line, tex, false);

                GLArray quads;
                quads.vertices.append(vv1);
                quads.vertices.append(vv2);
                quads.vertices.append(vv3);
                quads.vertices.append(vv4);

                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex->getTexture());
                quads.draw(GL_QUADS);
                glDisable(GL_TEXTURE_2D);
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

                // evaluate texture offsets
                TexTexture* textop = Tex_GetTexture(sidedef->texturetop, TexTexture::Texture, true);
                TexTexture* texbottom = Tex_GetTexture(sidedef->texturebottom, TexTexture::Texture, true);

                View3D_Helper_SetTextureOffsets(vv1, vv2, ov2, ov1, sector, other, linedef, sidedef, line, textop, true);
                View3D_Helper_SetTextureOffsets(ov4, ov3, vv3, vv4, sector, other, linedef, sidedef, line, texbottom, false);

                GLArray quads;

                // top texture
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, textop->getTexture());
                quads.vertices.append(vv1);
                quads.vertices.append(vv2);
                quads.vertices.append(ov2);
                quads.vertices.append(ov1);
                quads.draw(GL_QUADS);
                quads.vertices.clear();

                // bottom texture
                glBindTexture(GL_TEXTURE_2D, texbottom->getTexture());
                quads.vertices.append(ov4);
                quads.vertices.append(ov3);
                quads.vertices.append(vv3);
                quads.vertices.append(vv4);
                quads.draw(GL_QUADS);
                glDisable(GL_TEXTURE_2D);
            }
        }

        glColor4ub(255, 255, 255, 255);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1, 0.5);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, flatfloor->getTexture());
        glFrontFace(GL_CCW);
        tri_floor.draw(GL_TRIANGLES);
        glFrontFace(GL_CW);
        glBindTexture(GL_TEXTURE_2D, flatceiling->getTexture());
        tri_ceiling.draw(GL_TRIANGLES);
        glDisable(GL_TEXTURE_2D);

        glDisable(GL_POLYGON_OFFSET_FILL);
        //lines.draw(GL_LINES);
    }
    glEnable(GL_BLEND);
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
        wasVisible = true;
    }
    else if (!isVisible() && wasVisible)
    {
        //releaseMouse();
        //releaseKeyboard();
        wasVisible = false;
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
    if (running)
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
    if (e->isAutoRepeat())
        return;

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
    else if (e->key() == Qt::Key_Shift)
    {
        running = true;
        //qDebug("running on");
    }
}

void View3D::keyReleaseEvent(QKeyEvent *e)
{
    if (e->isAutoRepeat())
        return;

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
    else if ((e->key() == Qt::Key_Shift) || (e->modifiers() & Qt::ShiftModifier))
    {
        running = false;
        //qDebug("running off");
    }
}

void View3D::becameVisible()
{
    // todo reset mouse
    mouseXLast = mouseYLast = -1;
    running = false;

    // set x/y position to 2d mode position, and z position to 0
    posX = MainWindow::get()->getMouseX();
    posY = -MainWindow::get()->getMouseY();
    posZ = 0;
    //qDebug("posX = %f, posY = %f; posZ = %f", posX, posY, posZ);
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

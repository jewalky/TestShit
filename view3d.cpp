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

View3D::View3D(QWidget* parent) : QGLWidget(GetView3DFormat(), parent, MainWindow::get()->getSharedGLWidget()),
                                  highlightShader(context(), this)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    repaintTimer = new QTimer(this);
    connect(repaintTimer, SIGNAL(timeout()), this, SLOT(repaintTimerHandler()));
    repaintTimer->setInterval(26); // ~30fps
    repaintTimer->start();

    wasVisible = false;
    mouseXLast = mouseYLast = -1;

    running = false;

    hoverFBO = 0;

    rdist = 1024;
}

void View3D::initShader(QString name, QGLShaderProgram& out, QString filenamevx, QString filenamefr)
{
    if (!out.addShaderFromSourceFile(QGLShader::Vertex, filenamevx))
    {
        qDebug("View3D: failed to compile %s shader/vertex: %s", name.toUtf8().data(), highlightShader.log().toUtf8().data());
        return;
    }

    if (!out.addShaderFromSourceFile(QGLShader::Fragment, filenamefr))
    {
        qDebug("View3D: failed to compile %s shader/fragment: %s", name.toUtf8().data(), highlightShader.log().toUtf8().data());
        return;
    }

    if (!out.link())
    {
        qDebug("View3D: failed to link %s shader: %s", name.toUtf8().data(), highlightShader.log().toUtf8().data());
        return;
    }
}

void View3D::initializeGL()
{
    qDebug("initializing GL...");

    // init entity highlight shader (basically mixes some highlight color with the texture)
    initShader("highlight", highlightShader, ":/resources/highlightShader.vx", ":/resources/highlightShader.fr");

    // init midtex select shader (stencil-like rendering only with alpha >0.5 of the actual midtex texture, used in offscreen rendering of masks)
    initShader("midtex select", midtexSelectShader, ":/resources/midtexSelectShader.vx", ":/resources/midtexSelectShader.fr");

    highlightShader.setUniformValue("uHighlightColor", QVector4D(0, 0, 0, 0));
}

void View3D::paintGL()
{
    setPerspective(140);

    if (!hoverFBO || hoverFBO->width() != width() || hoverFBO->height() != height())
    {
        if (hoverFBO) delete hoverFBO;
        hoverFBO = new QGLFramebufferObject(width(), height(), QGLFramebufferObject::Depth, GL_TEXTURE_2D);
    }

    hoverFBO->bind();
    render(0);
    hoverFBO->release();
    // slow operation. maybe do something about it later?
    QImage fboImage(hoverFBO->toImage());
    QImage image(fboImage.constBits(), fboImage.width(), fboImage.height(), QImage::Format_ARGB32);
    QRgb colorCenter = image.pixel(image.width()/2, image.height()/2);
    // unpack hovered surface.
    hoverType = (HoverType)((colorCenter & 0x00FF0000) >> 16);
    hoverId = ((colorCenter & 0xFFFF) << 8) | ((colorCenter & 0xFF000000) >> 24);
    //qDebug("hoverType = %d; hoverId = %d", hoverType, hoverId);

    render(1);
}


void View3D::resizeGL(int width, int height)
{
    //setClipRect(0, 0, width, height);
    // this is done each frame anyway
    update();
}

#include <QElapsedTimer>
static QElapsedTimer qetfps;
static int intfps = 0;
void View3D::repaintTimerHandler()
{
    if (!qetfps.isValid())
        qetfps.start();

    intfps++;
    if (qetfps.elapsed() > 1000)
    {
        qDebug("FPS: %d", intfps);
        intfps = 0;
        qetfps.restart();
    }

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

    float movemul = 14;
    if (running)
        movemul *= 3;

    QLineF vertvec(0, 0, 1, 0);
    vertvec.setAngle(angX);
    posZ -= dy*vertvec.dy()*movemul;
    QLineF movevec(0, 0, dx, dy*vertvec.dx());
    movevec.setAngle(movevec.angle()-angZ);
    posX += movevec.dx()*movemul;
    posY -= movevec.dy()*movemul;

    //
    repaint();
}

void View3D::initMap()
{
    DoomMap* cmap = MainWindow::get()->getMap();

    //
    posX = posY = posZ = 0;
    posZ = 48;
    angX = angY = angZ = 0;

    mouseXLast = mouseYLast = -1;

    moveForward = moveBackward = moveLeft = moveRight = 0;
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
    float zFar = rdist;

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

#define VIEW3D_HELPER_PACKHOVERID(a,b,c,d,hovertype,id) \
    a = (int)(hovertype); \
    b = ((id) & 0xFF0000) >> 16; \
    c = ((id) & 0xFF00) >> 8; \
    d = ((id) & 0xFF);

// ewwwwwwwwwwwwwwwwwwwwwwwww this function is so ugly
// it's used to compute similar, but different texture offsets for top/bottom/middle linedef parts.
static void View3D_Helper_SetTextureOffsets(GLVertex& vv1, GLVertex& vv2, GLVertex& vv3, GLVertex& vv4, DoomMapSector* sector, DoomMapSector* other, DoomMapLinedef* linedef, DoomMapSidedef* sidedef, QLineF& line, TexTexture* tex, int part)
{
    bool top = (part == 0);
    bool bottom = (part == 2);
    bool middle = (part == 1);

    float xbase = sidedef->offsetx;
    float ybase = sidedef->offsety;
    // lower unpegged means the texture should have origin at it's bottom, not top.
    if (middle)
    {
        // if texture is lower unpegged, move it to bottom of own sidedef
        if (linedef->dontpegbottom)
        {
            int ptd = (sector->heightceiling-sector->heightfloor) - tex->getHeight();
            ybase -= ptd;
        }
    }
    else if (linedef->dontpegbottom && !top)
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

struct ScheduledObject
{
    View3D* view3d;
    QVector4D color_hl;
    QVector4D color_sel;
    float z;

    virtual void render(int pass) = 0;
    virtual QVector4D getHighlight(int pass) = 0;

    static int SortHelper(const void* a, const void* b)
    {
        const ScheduledObject* soa = *(ScheduledObject**)a;
        const ScheduledObject* sob = *(ScheduledObject**)b;

        if (soa->z < sob->z) return -1;
        if (soa->z > sob->z) return 1;
        return 0;
    }
};

struct ScheduledSidedef : public ScheduledObject
{
    GLArray array;
    TexTexture* texture;
    int id;
    int start;
    int len;

    virtual void render(int pass)
    {
        if (!view3d->cullArray(array))
            return;

        if (pass == 1)
        {
            //glDepthMask(GL_FALSE);
            //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else if (pass == 0) glDisable(GL_BLEND);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture->getTexture());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // draw midtex
        array.draw(GL_QUADS, start, len);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glDisable(GL_TEXTURE_2D);

        if (pass == 1)
        {
            // restore texture wrapping parameters
            //glDepthMask(GL_TRUE);
        }
        else if (pass == 0) glEnable(GL_BLEND);
    }

    virtual QVector4D getHighlight(int pass)
    {
        if (pass != 1) return QVector4D(0, 0, 0, 0);

        if (view3d->hoverType == View3D::Hover_SidedefMiddle &&
                view3d->hoverId == id) return color_hl;

        return QVector4D(0, 0, 0, 0);
    }
};

void View3D::render(int pass)
{
    // pass = 0 = render without fog and without textures; used for highlighting purposes.
    // pass = 1 = render for display

    // get matrix.
    GLfloat rawmodelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, rawmodelview);
    modelview = QMatrix4x4(rawmodelview);

    GLfloat rawprojection[16];
    glGetFloatv(GL_PROJECTION_MATRIX, rawprojection);
    projection = QMatrix4x4(rawprojection);

    QVector4D color_hl(0.5, 0.25, 0.0, 0.5);
    QVector4D color_sel(0.5, 0.0, 0.0, 0.5);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    DoomMap* cmap = MainWindow::get()->getMap();
    if (!cmap)
        return;

    int rpass = 1;

    if (pass == 1)
    {
        rpass = 0;
        highlightShader.bind();
        highlightShader.setUniformValue("uFogSize", QVector2D(rdist-64, rdist));
    }
    else if (pass == 0)
    {
        midtexSelectShader.bind();
    }

    QVector<ScheduledObject*> scheduled;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    // hueeeeeeeeeeeeeeeeeeeeeeeeee
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    int ds = 0;
    for (int i = 0; i < cmap->sectors.size(); i++)
    {
        DoomMapSector* sector = &cmap->sectors[i];
        // dont render if too far
        if (!sector->isAnyWithin(posX, -posY, rdist+64))
            continue;
        ds++;

        // find ceiling texture
        // this NEVER returns null. at most - embedded resource that says "BROKEN TEXTURE"
        TexTexture* flatceiling = Tex_GetTexture(sector->textureceiling, TexTexture::Flat, true);
        TexTexture* flatfloor = Tex_GetTexture(sector->texturefloor, TexTexture::Flat, true);

        // draw sector's floor and ceiling first
        if (sector->glupdate)
        {
            sector->glupdate = false;
            sector->glfloor.vertices.clear();
            sector->glceiling.vertices.clear();


            for (int k = 0; k < 2; k++)
            {
                GLArray tri_floor = sector->triangles, tri_ceiling = sector->triangles;
                for (int j = 0; j < sector->triangles.vertices.size(); j++)
                {
                    GLVertex& fv = tri_floor.vertices[j];
                    GLVertex& cv = tri_ceiling.vertices[j];

                    fv.z = sector->zatFloor(fv.x, fv.y);
                    cv.z = sector->zatCeiling(cv.x, cv.y);

                    if (k == 0)
                    {
                        fv.r = fv.g = fv.b = sector->lightlevel;
                        fv.a = 255;

                        cv.r = cv.g = cv.b = sector->lightlevel;
                        cv.a = 255;
                    }
                    else if (k == 1)
                    {
                        VIEW3D_HELPER_PACKHOVERID(fv.r, fv.g, fv.b, fv.a, Hover_Floor, sector-cmap->sectors.data());
                        VIEW3D_HELPER_PACKHOVERID(cv.r, cv.g, cv.b, cv.a, Hover_Ceiling, sector-cmap->sectors.data());
                    }

                    // set floor u/v
                    fv.u = fv.x / flatfloor->getWidth();
                    fv.v = fv.y / flatfloor->getHeight();

                    // set ceiling u/v
                    cv.u = cv.x / flatceiling->getWidth();
                    cv.v = cv.y / flatceiling->getHeight();

                    sector->glfloor.vertices.append(fv);
                    sector->glceiling.vertices.append(cv);
                }
            }

            sector->glfloor.update();
            sector->glceiling.update();
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

            // for hovering
            int sd_id = sidedef-cmap->sidedefs.data();

            // now do different processing based on line type.
            // single-sided line:
            if (sidedef->glupdate)
            {
                sidedef->glupdate = false;

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

                quint8 c = 255;

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

                // set color
                c = lite;

                vv1.r=vv1.g=vv1.b=c;
                vv2.r=vv2.g=vv2.b=c;
                vv3.r=vv3.g=vv3.b=c;
                vv4.r=vv4.g=vv4.b=c;

                sidedef->gltop.vertices.clear();
                sidedef->glbottom.vertices.clear();
                sidedef->glmiddle.vertices.clear();
                if (!linedef->getBack())
                {
                    TexTexture* tex = Tex_GetTexture(sidedef->texturemiddle, TexTexture::Texture, true);

                    View3D_Helper_SetTextureOffsets(vv1, vv2, vv3, vv4, sector, 0, linedef, sidedef, line, tex, 2);

                    sidedef->glmiddle.vertices.append(vv1);
                    sidedef->glmiddle.vertices.append(vv2);
                    sidedef->glmiddle.vertices.append(vv3);
                    sidedef->glmiddle.vertices.append(vv4);

                    VIEW3D_HELPER_PACKHOVERID(vv1.r, vv1.g, vv1.b, vv1.a, Hover_SidedefMiddle, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv2.r, vv2.g, vv2.b, vv2.a, Hover_SidedefMiddle, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv3.r, vv3.g, vv3.b, vv3.a, Hover_SidedefMiddle, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv4.r, vv4.g, vv4.b, vv4.a, Hover_SidedefMiddle, sd_id);

                    sidedef->glmiddle.vertices.append(vv1);
                    sidedef->glmiddle.vertices.append(vv2);
                    sidedef->glmiddle.vertices.append(vv3);
                    sidedef->glmiddle.vertices.append(vv4);
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
                    TexTexture* texmiddle = (sidedef->texturemiddle != "-") ? Tex_GetTexture(sidedef->texturemiddle, TexTexture::Texture, true) : 0;

                    View3D_Helper_SetTextureOffsets(vv1, vv2, ov2, ov1, sector, other, linedef, sidedef, line, textop, 0);
                    View3D_Helper_SetTextureOffsets(ov4, ov3, vv3, vv4, sector, other, linedef, sidedef, line, texbottom, 2);

                    // top texture pass 1
                    sidedef->gltop.vertices.append(vv1);
                    sidedef->gltop.vertices.append(vv2);
                    sidedef->gltop.vertices.append(ov2);
                    sidedef->gltop.vertices.append(ov1);

                    // bottom texture pass 1
                    sidedef->glbottom.vertices.append(ov4);
                    sidedef->glbottom.vertices.append(ov3);
                    sidedef->glbottom.vertices.append(vv3);
                    sidedef->glbottom.vertices.append(vv4);

                    // middle texture pass 1
                    if (texmiddle != 0)
                    {
                        View3D_Helper_SetTextureOffsets(ov1, ov2, ov3, ov4, sector, other, linedef, sidedef, line, texmiddle, 1);
                        sidedef->glmiddle.vertices.append(ov1);
                        sidedef->glmiddle.vertices.append(ov2);
                        sidedef->glmiddle.vertices.append(ov3);
                        sidedef->glmiddle.vertices.append(ov4);
                    }

                    // top texture pass 0
                    VIEW3D_HELPER_PACKHOVERID(vv1.r, vv1.g, vv1.b, vv1.a, Hover_SidedefTop, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv2.r, vv2.g, vv2.b, vv2.a, Hover_SidedefTop, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(ov2.r, ov2.g, ov2.b, ov2.a, Hover_SidedefTop, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(ov1.r, ov1.g, ov1.b, ov1.a, Hover_SidedefTop, sd_id);

                    sidedef->gltop.vertices.append(vv1);
                    sidedef->gltop.vertices.append(vv2);
                    sidedef->gltop.vertices.append(ov2);
                    sidedef->gltop.vertices.append(ov1);

                    // bottom texture pass 0
                    VIEW3D_HELPER_PACKHOVERID(ov4.r, ov4.g, ov4.b, ov4.a, Hover_SidedefBottom, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(ov3.r, ov3.g, ov3.b, ov3.a, Hover_SidedefBottom, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv3.r, vv3.g, vv3.b, vv3.a, Hover_SidedefBottom, sd_id);
                    VIEW3D_HELPER_PACKHOVERID(vv4.r, vv4.g, vv4.b, vv4.a, Hover_SidedefBottom, sd_id);

                    sidedef->glbottom.vertices.append(ov4);
                    sidedef->glbottom.vertices.append(ov3);
                    sidedef->glbottom.vertices.append(vv3);
                    sidedef->glbottom.vertices.append(vv4);

                    // middle texture pass 0
                    if (texmiddle != 0)
                    {
                        VIEW3D_HELPER_PACKHOVERID(ov1.r, ov1.g, ov1.b, ov1.a, Hover_SidedefMiddle, sd_id);
                        VIEW3D_HELPER_PACKHOVERID(ov2.r, ov2.g, ov2.b, ov2.a, Hover_SidedefMiddle, sd_id);
                        VIEW3D_HELPER_PACKHOVERID(ov3.r, ov3.g, ov3.b, ov3.a, Hover_SidedefMiddle, sd_id);
                        VIEW3D_HELPER_PACKHOVERID(ov4.r, ov4.g, ov4.b, ov4.a, Hover_SidedefMiddle, sd_id);

                        sidedef->glmiddle.vertices.append(ov1);
                        sidedef->glmiddle.vertices.append(ov2);
                        sidedef->glmiddle.vertices.append(ov3);
                        sidedef->glmiddle.vertices.append(ov4);
                    }
                }

                sidedef->gltop.update();
                sidedef->glbottom.update();
                sidedef->glmiddle.update();
            }

            if (!linedef->getBack())
            {
                TexTexture* tex = Tex_GetTexture(sidedef->texturemiddle, TexTexture::Texture, true);

                if (cullArray(sidedef->glmiddle))
                {
                    glBindTexture(GL_TEXTURE_2D, tex->getTexture());
                    if (pass == 1) highlightShader.setUniformValue("uHighlightColor", (hoverType == Hover_SidedefMiddle && hoverId == sd_id) ? color_hl : QVector4D(0, 0, 0, 0));
                    sidedef->glmiddle.draw(GL_QUADS, rpass*4, 4);
                }
            }
            else
            {
                TexTexture* textop = Tex_GetTexture(sidedef->texturetop, TexTexture::Texture, true);
                TexTexture* texbottom = Tex_GetTexture(sidedef->texturebottom, TexTexture::Texture, true);
                TexTexture* texmiddle = (sidedef->texturemiddle != "-") ? Tex_GetTexture(sidedef->texturemiddle, TexTexture::Texture, true) : 0;

                // top texture
                if (cullArray(sidedef->gltop))
                {
                    glBindTexture(GL_TEXTURE_2D, textop->getTexture());
                    if (pass == 1) highlightShader.setUniformValue("uHighlightColor", (hoverType == Hover_SidedefTop && hoverId == sd_id) ? color_hl : QVector4D(0, 0, 0, 0));
                    sidedef->gltop.draw(GL_QUADS, rpass*4, 4);
                }

                // bottom texture
                if (cullArray(sidedef->glbottom))
                {
                    glBindTexture(GL_TEXTURE_2D, texbottom->getTexture());
                    if (pass == 1) highlightShader.setUniformValue("uHighlightColor", (hoverType == Hover_SidedefBottom && hoverId == sd_id) ? color_hl : QVector4D(0, 0, 0, 0));
                    sidedef->glbottom.draw(GL_QUADS, rpass*4, 4);
                }

                // schedule middle texture
                if (texmiddle != 0)
                {
                    ScheduledSidedef* ssd = new ScheduledSidedef();
                    ssd->view3d = this;
                    ssd->color_hl = color_hl;
                    ssd->color_sel = color_sel;
                    ssd->id = sd_id;
                    ssd->start = rpass*4;
                    ssd->len = 4;

                    ssd->array = sidedef->glmiddle;
                    ssd->texture = texmiddle;

                    // put z coord
                    GLVertex center = ssd->array.getCenter();
                    QVector3D centermult = QVector3D(center.x, center.y, center.z) * modelview;
                    ssd->z = centermult.z();

                    scheduled.append(ssd);
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, flatfloor->getTexture());

        int sec_id = sector-cmap->sectors.data();
        int tricnt = sector->triangles.vertices.size();

        if (cullArray(sector->glfloor))
        {
            glFrontFace(GL_CCW);
            if (pass == 1) highlightShader.setUniformValue("uHighlightColor", (hoverType == Hover_Floor && hoverId == sec_id) ? color_hl : QVector4D(0, 0, 0, 0));
            sector->glfloor.draw(GL_TRIANGLES, rpass*tricnt, tricnt);
            glFrontFace(GL_CW);
        }

        if (cullArray(sector->glceiling))
        {
            glBindTexture(GL_TEXTURE_2D, flatceiling->getTexture());
            if (pass == 1) highlightShader.setUniformValue("uHighlightColor", (hoverType == Hover_Ceiling && hoverId == sec_id) ? color_hl : QVector4D(0, 0, 0, 0));
            sector->glceiling.draw(GL_TRIANGLES, rpass*tricnt, tricnt);
        }
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    // draw scheduled middle textures
    qsort(scheduled.data(), scheduled.size(), sizeof(ScheduledObject*), &ScheduledObject::SortHelper);
    for (int i = 0; i < scheduled.size(); i++)
    {
        ScheduledObject* o = scheduled[i];
        if (pass == 1) highlightShader.setUniformValue("uHighlightColor", o->getHighlight(pass));
        o->render(pass);
        delete o;
    }

    scheduled.clear();

    glDisable(GL_DEPTH_TEST);

    if (pass == 1)
    {
        highlightShader.release();

        glDisable(GL_FOG);
    }
    else if (pass == 0)
    {
        midtexSelectShader.release();
    }

    if (pass == 1) // draw overlay
    {
        setClipRect(0, 0, width(), height());
        renderOverlay();
    }
}

void View3D::setClipRect(int x, int y, int w, int h)
{
    glViewport(x, height()-(y+h), w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(x, x+w, y+h, y, -65536, 65536); // I doubt it will ever draw 64k sprites at once
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void View3D::renderOverlay()
{
    // draw crosshair in the middle.
    static TexTexture* tex_Xhair = 0;
    if (!tex_Xhair) tex_Xhair = TexTexture::fromImage(QImage(":///resources/xhair.png"));

    float cx = width()/2;
    float cy = height()/2;
    float sz = (float)tex_Xhair->getWidth() / 2;

    GLArray xhaira;
    xhaira.vertices.append(GLVertex(cx-sz, cy-sz, 0, 0, 0));
    xhaira.vertices.append(GLVertex(cx+sz, cy-sz, 0, 1, 0));
    xhaira.vertices.append(GLVertex(cx+sz, cy+sz, 0, 1, 1));
    xhaira.vertices.append(GLVertex(cx-sz, cy+sz, 0, 0, 1));

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_Xhair->getTexture());
    xhaira.draw(GL_QUADS);
    glDisable(GL_TEXTURE_2D);
}

bool View3D::cullArray(GLArray& a)
{
    for (int i = 0; i < a.vertices.size(); i++)
    {
        GLVertex& v = a.vertices[i];
        QVector3D v3d(v.x, v.y, v.z);
        v3d = v3d * modelview;
        if (v3d.z() < 0)
            return true;
    }

    return false;
}

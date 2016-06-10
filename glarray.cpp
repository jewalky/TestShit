#include "glarray.h"
#include <QGLWidget>

GLArray::GLArray()
{
    useVBO = false;
}

GLArray::~GLArray()
{
    //
}

void GLArray::draw(int mode)
{
    draw(mode, 0, vertices.size());
}

void GLArray::draw(int mode, int first, int count)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    if (useVBO)
    {
        if (!vboVertices.isCreated() ||
                !vboUV.isCreated() ||
                !vboColors.isCreated()) return;

        vboVertices.bind();
        glVertexPointer(3, GL_FLOAT, 0, 0);
        vboColors.bind();
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
        vboUV.bind();
        glTexCoordPointer(2, GL_FLOAT, 0, 0);
        glDrawArrays(mode, first, count);
    }
    else
    {
        glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, x));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, r));
        glTexCoordPointer(2, GL_FLOAT, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, u));
        glDrawArrays(mode, first, count);
    }

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void GLArray::update()
{
    if (!useVBO)
        return;

    if (!vboVertices.isCreated())
    {
        vboVertices.create();
        vboVertices.setUsagePattern(QGLBuffer::DynamicDraw);
    }

    if (!vboUV.isCreated())
    {
        vboUV.create();
        vboUV.setUsagePattern(QGLBuffer::DynamicDraw);
    }

    if (!vboColors.isCreated())
    {
        vboColors.create();
        vboColors.setUsagePattern(QGLBuffer::DynamicDraw);
    }

    GLfloat avertices[vertices.size()*3];
    GLubyte acolors[vertices.size()*4];
    GLfloat auv[vertices.size()*2];

    for (int i = 0; i < vertices.size(); i++)
    {
        avertices[i*3] = vertices[i].x;
        avertices[i*3+1] = vertices[i].y;
        avertices[i*3+2] = vertices[i].z;
        acolors[i*4] = vertices[i].r;
        acolors[i*4+1] = vertices[i].g;
        acolors[i*4+2] = vertices[i].b;
        acolors[i*4+3] = vertices[i].a;
        auv[i*2] = vertices[i].u;
        auv[i*2+1] = vertices[i].v;
    }

    vboVertices.bind();
    vboVertices.allocate(avertices, sizeof(GLfloat)*vertices.size()*3);
    vboVertices.release();

    vboColors.bind();
    vboColors.allocate(acolors, sizeof(GLubyte)*vertices.size()*4);
    vboColors.release();

    vboUV.bind();
    vboUV.allocate(auv, sizeof(GLfloat)*vertices.size()*2);
    vboUV.release();
}


#include "glarray.h"
#include <QGLWidget>

GLArray::GLArray()
{

}

GLArray::~GLArray()
{

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

    glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, x));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, r));
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLVertex), ((quint8*)vertices.data())+offsetof(GLVertex, u));
    glDrawArrays(mode, first, count);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

#include "texman.h"
#include "../mainwindow.h"
#include "wadfile.h"
#include <QDataStream>

static QVector<TexResource> Resources;

static TexTexture* Embedded_BrokenTexture = 0;

static QMap<QString, TexTexture*> Textures;
static QMap<QString, TexTexture*> Flats;
static QMap<QString, TexTexture*> Graphics;

static quint32 Playpal[256];

static void PutTexture(QMap<QString, TexTexture*>& m, QString name, TexTexture* tex)
{
    if (m.contains(name) && m[name] != tex)
        delete m[name];
    if (tex) m[name] = tex;
    else m.remove(name);
}

static bool FindLastLump(WADFile*& resource, WADEntry*& entry, QString filename, WADNamespace ns)
{
    entry = 0;
    resource = 0;

    for (int i = Resources.size()-1; i >= 0; i--)
    {
        WADFile* wad = Resources[i].resource;
        if (!wad) continue;

        int num = wad->getLastNumForName(filename, -1, ns);
        if (num < 0) continue;

        entry = wad->getEntry(num);
        resource = wad;
        return true;
    }

    return false;
}

int TexTexture::getTexture()
{
    if (texture != 0)
        return texture;

    // make texture :D
    if (!QGLContext::currentContext())
    {
        QGLWidget* glw = MainWindow::get()->getSharedGLWidget();
        glw->context()->makeCurrent();
    }

    glGenTextures(1, (GLuint*)&texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (!keeppixels)
    {
        delete pixels;
        pixels = 0;
    }

    return texture;
}

TexTexture* TexTexture::fromImage(QImage img)
{
    if (img.isNull()) return 0;
    QImage cimg = img.convertToFormat(QImage::Format_ARGB32);

    int w = cimg.width();
    int h = cimg.height();
    quint32* pixels = new quint32[w*h];
    memcpy(pixels, cimg.constBits(), 4*w*h);

    return new TexTexture(w, h, pixels);
}

void Tex_Reset()
{
    for (QMap<QString, TexTexture*>::iterator it = Textures.begin();
         it != Textures.end(); ++it) delete it.value();
    for (QMap<QString, TexTexture*>::iterator it = Flats.begin();
         it != Flats.end(); ++it) delete it.value();
    for (QMap<QString, TexTexture*>::iterator it = Graphics.begin();
         it != Graphics.end(); ++it) delete it.value();

    Textures.clear();
    Flats.clear();
    Graphics.clear();
}

void Tex_SetWADList(QVector<TexResource> wads)
{
    Resources = wads;
    Tex_Reload();
}

// todo: write a separate patch loading routine for use with sprites
static TexTexture* RenderTexture(DoomTexture1Texture& tex)
{
    //qDebug("Tex_Reload: found texture \"%s\"", tex.name.toUpper().toUtf8().data());
    // I'm lazy to write fast blitting procedures here. maybe later.
    if (tex.width <= 0 || tex.height <= 0)
        return 0;

    quint32* pixels = new quint32[tex.width*tex.height];
    memset(pixels, 0, tex.width*tex.height*4);

    // todo: check if tall patches work
    for (int i = 0; i < tex.patches.size(); i++)
    {
        DoomTexture1Patch& patch = tex.patches[i];

        // find patch
        WADFile* rPatch; WADEntry* ePatch;
        // first search under Patches namespace. this is used to resolve conflicts instead of "strict patches" flag like GZDB does it.
        if (!FindLastLump(rPatch, ePatch, patch.name, NS_Patches) &&
                !FindLastLump(rPatch, ePatch, patch.name, NS_Global)) continue;

        QDataStream patch_stream(ePatch->getData());
        patch_stream.setByteOrder(QDataStream::LittleEndian);

        // read patch headers
        quint16 width;
        quint16 height;
        qint16 xoffset;
        qint16 yoffset;
        patch_stream >> width >> height >> xoffset >> yoffset;

        //qDebug("patch name = \"%s\"; data = %d, %d, %d, %d", patch.name.toUtf8().data(), width, height, xoffset, yoffset);

        quint32 columns[width];
        for (int x = 0; x < width; x++)
            patch_stream >> columns[x];

        for (int x = 0; x < width; x++)
        {
            //qDebug("column %d at %08X", x, columns[x]);
            if (columns[x] > patch_stream.device()->size()-1) // invalid patch
            {
                qDebug("RenderTexture: warning: invalid patch %s, seek to %08X", patch.name.toUtf8().data(), columns[x]);
                break;
            }

            patch_stream.device()->seek(columns[x]);
            while (true)
            {
                quint8 ystart;
                quint8 num;
                quint8 garbage;

                patch_stream >> ystart;
                if (ystart == 0xFF || patch_stream.device()->atEnd())
                    break; // end of span

                patch_stream >> num >> garbage;
                //qDebug("ystart = %d; num = %d; garbage = %d", ystart, num, garbage);
                for (int j = 0; j <= num; j++)
                {
                    // pixels from ystart to ystart+num are here
                    quint8 color;
                    patch_stream >> color;
                    int y = ystart+j;
                    int rx = x + patch.originx;
                    int ry = y + patch.originy;
                    quint32 px = Playpal[color];
                    if (rx >= 0 && ry >= 0 && rx < tex.width && ry < tex.height)
                        pixels[ry*tex.width+rx] = px;
                }
            }
        }
    }

    return new TexTexture(tex.width, tex.height, pixels);
}

void Tex_Reload()
{
    Tex_Reset();

    // failsafe.
    if (!Embedded_BrokenTexture)
        Embedded_BrokenTexture = TexTexture::fromImage(QImage(":/resources/_BROKEN.png"));

    // first, find playpal.
    // note: currently only working with WAD files.
    for (int i = 0; i < Resources.size(); i++)
        Resources[i].reload();

    qDebug("Tex_Reload: looking for PLAYPAL...");
    WADFile* rPlaypal; WADEntry* ePlaypal;
    if (!FindLastLump(rPlaypal, ePlaypal, "PLAYPAL", NS_Global))
    {
        qDebug("Tex_Reload: warning: no PLAYPAL loaded.");
        for (int i = 0; i < 256; i++)
            Playpal[i] = (0xFF000000) | (i << 16) | (i << 8) | (i);
    }
    else
    {
        QByteArray& playdata = ePlaypal->getData();
        for (int j = 0; j < 256; j++)
        {
            int r = (quint8)playdata[j*3];
            int g = (quint8)playdata[j*3+1];
            int b = (quint8)playdata[j*3+2];
            Playpal[j] = (0xFF000000) | (r << 16) | (g << 8) | (b);
        }

        qDebug("Tex_Reload: PLAYPAL loaded.");
    }

    // get flats
    for (int i = 0; i < Resources.size(); i++)
    {
        WADFile* wad = Resources[i].resource;
        if (!wad) continue;

        qDebug("Tex_Reload: loading flats from \"%s\"...", Resources[i].name.toUtf8().data());

        for (int j = 0; j < wad->getSize(); j++)
        {
            if (wad->getEntry(j)->getNamespace() != NS_Flats)
                continue;

            WADEntry* flat = wad->getEntry(j);
            if (flat->getData().size() != 4096)
                continue;

            //qDebug("Tex_Reload: found flat \"%s\"", flat->getName().toUpper().toUtf8().data());
            quint32* flatpixels = new quint32[4096];
            QByteArray& flatindices = flat->getData();
            for (int k = 0; k < 4096; k++)
                flatpixels[k] = Playpal[(quint8)flatindices[k]];

            PutTexture(Flats, flat->getName().toUpper(), new TexTexture(64, 64, flatpixels));
        }
    }

    // get TEXTUREx

    WADFile* rTexture1; WADEntry* eTexture1;
    WADFile* rTexture2; WADEntry* eTexture2;

    qDebug("Tex_Reload: looking for TEXTUREx...");

    if (FindLastLump(rTexture1, eTexture1, "TEXTURE1", NS_Global))
    {
        QVector<DoomTexture1Texture> texture1 = Tex_ReadTexture1(eTexture1);
        for (int j = 0; j < texture1.size(); j++)
            PutTexture(Textures, texture1[j].name.toUpper(), RenderTexture(texture1[j]));
        qDebug("Tex_Reload: TEXTURE1 loaded.");
    }

    if (FindLastLump(rTexture2, eTexture2, "TEXTURE2", NS_Global))
    {
        QVector<DoomTexture1Texture> texture2 = Tex_ReadTexture1(eTexture2);
        for (int j = 0; j < texture2.size(); j++)
            PutTexture(Textures, texture2[j].name.toUpper(), RenderTexture(texture2[j]));
        qDebug("Tex_Reload: TEXTURE2 loaded.");
    }

    qDebug("Tex_Reload: finished.");
}

TexTexture* Tex_GetTexture(QString name, TexTexture::Type preferredtype, bool stricttype)
{
    name = name.toUpper();
    switch (preferredtype)
    {
    case TexTexture::Texture:
        if (Textures.contains(name))
            return Textures[name];
        break;
    case TexTexture::Flat:
        if (Flats.contains(name))
            return Flats[name];
        break;
    case TexTexture::Graphic:
        if (Graphics.contains(name))
            return Graphics[name];
        break;
    }

    if (stricttype) return Embedded_BrokenTexture;

    if (Textures.contains(name))
        return Textures[name];
    if (Flats.contains(name))
        return Flats[name];
    if (Graphics.contains(name))
        return Graphics[name];

    return Embedded_BrokenTexture;
}

QVector<DoomTexture1Texture> Tex_ReadTexture1(WADEntry* ent)
{
    QVector<DoomTexture1Texture> out;
    // first read PNAMES, if we can't read PNAMES, return null
    /*int pnames = wad->getLastNumForName("PNAMES");
    if (pnames < 0)
        return out;*/
    WADFile* rPnames; WADEntry* ePnames;
    if (!FindLastLump(rPnames, ePnames, "PNAMES", NS_Global))
        return out;

    QStringList pnames_unp;
    QDataStream pnames_stream(ePnames->getData());
    pnames_stream.setByteOrder(QDataStream::LittleEndian);

    quint32 pnames_len;
    pnames_stream >> pnames_len;
    for (quint32 i = 0; i < pnames_len; i++)
    {
        char pname[9];
        pname[8] = 0;
        pnames_stream.readRawData(pname, 8);
        pnames_unp.append(QString::fromUtf8(pname));
    }

    // now, read the actual texture directory
    bool strife = false;
    QDataStream texturex_stream(ent->getData());
    texturex_stream.setByteOrder(QDataStream::LittleEndian);

    quint32 numtextures;
    texturex_stream >> numtextures;
    quint32 offsets[numtextures];
    for (quint32 i = 0; i < numtextures; i++)
        texturex_stream >> offsets[i];

    for (quint32 i = 0; i < numtextures; i++)
    {
        texturex_stream.device()->seek(offsets[i]);
        char name[9];
        name[8] = 0;
        texturex_stream.readRawData(name, 8);

        quint16 flags;
        quint8 scalex;
        quint8 scaley;
        qint16 width;
        qint16 height;
        quint32 columndirectory;
        qint16 patchcount;

        texturex_stream >> flags >> scalex >> scaley >> width >> height;
        if (!strife)
        {
            texturex_stream >> columndirectory;
            // check columndirectory. if last two bytes are nonzero, assume strife format and reread patchcount.
            if ((columndirectory & 0xFFFF) != 0)
            {
                texturex_stream.device()->seek(texturex_stream.device()->pos()-4);
                strife = true;
            }
        }

        texturex_stream >> patchcount;

        DoomTexture1Texture tex;
        tex.name = QString::fromUtf8(name);
        tex.scalex = (float)scalex / 8;
        tex.scaley = (float)scaley / 8;
        tex.width = width;
        tex.height = height;

        for (qint16 i = 0; i < patchcount; i++)
        {
            qint16 originx;
            qint16 originy;
            qint16 patchid;
            qint16 stepdir;
            qint16 colormap;

            texturex_stream >> originx >> originy >> patchid;
            if (!strife) texturex_stream >> stepdir >> colormap;

            DoomTexture1Patch patch;
            patch.originx = originx;
            patch.originy = originy;
            patch.name = (patchid >= 0 && patchid < pnames_unp.size()) ? pnames_unp[patchid] : "";
            tex.patches.append(patch);
        }

        out.append(tex);
    }

    return out;
}

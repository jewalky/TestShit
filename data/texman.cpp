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

    bool wasenabled = glIsEnabled(GL_TEXTURE_2D);

    if (!wasenabled) glEnable(GL_TEXTURE_2D);
    glGenTextures(1, (GLuint*)&texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (!wasenabled) glDisable(GL_TEXTURE_2D);

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
static TexTexture* RenderTexture(WADFile* wad, DoomTexture1Texture& tex)
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
        int pnum = wad->getLastNumForName(patch.name);
        if (pnum < 0) // invalid patch, ignore. todo: log error
            continue;

        QDataStream patch_stream(wad->getEntry(pnum)->getData());
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
            patch_stream.device()->seek(columns[x]);
            while (true)
            {
                quint8 ystart;
                quint8 num;
                quint8 garbage;

                patch_stream >> ystart;
                if (ystart == 0xFF)
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
    QVector<WADFile*> wads;
    for (int i = 0; i < Resources.size(); i++)
    {
        if (Resources[i].type != TexResource::WAD)
        {
            qDebug("Tex_Reload: error: non-WAD types aren't supported yet");
            wads.append(0);
            continue;
        }

        WADFile* wad = WADFile::fromFile(Resources[i].name);
        if (!wad)
        {
            qDebug("Tex_Reload: error: couldn't open \"%s\"", Resources[i].name.toUtf8().data());
            wads.append(0);
            continue;
        }

        qDebug("Tex_Reload: added \"%s\"...", Resources[i].name.toUtf8().data());
        wads.append(wad);
    }

    bool playpalfound = false;
    for (int i = 0; i < wads.size(); i++)
    {
        if (!wads[i])
            continue;

        qDebug("Tex_Reload: searching for PLAYPAL in \"%s\"...", Resources[i].name.toUtf8().data());
        int playpal = wads[i]->getLastNumForName("PLAYPAL");
        if (playpal < 0)
            continue;

        QByteArray& playdata = wads[i]->getEntry(playpal)->getData();
        for (int j = 0; j < 256; j++)
        {
            int r = (quint8)playdata[j*3];
            int g = (quint8)playdata[j*3+1];
            int b = (quint8)playdata[j*3+2];
            Playpal[j] = (0xFF000000) | (r << 16) | (g << 8) | (b);
        }

        qDebug("Tex_Reload: loaded PLAYPAL.");
        playpalfound = true;
    }

    if (!playpalfound)
    {
        qDebug("Tex_Reload: warning: no PLAYPAL loaded.");
        for (int i = 0; i < 256; i++)
            Playpal[i] = (0xFF000000) | (i << 16) | (i << 8) | (i);
    }

    for (int i = 0; i < wads.size(); i++)
    {
        if (!wads[i])
            continue;

        qDebug("Tex_Reload: processing \"%s\"...", Resources[i].name.toUtf8().data());
        // fetch all flats.
        int fnum = 0;
        while (true)
        {
            int f_start = wads[i]->getNumForName("F_START", fnum);
            int ff_start = wads[i]->getNumForName("FF_START", fnum);
            int cs = (f_start>=0&&ff_start>=0) ? std::min(f_start, ff_start) : std::max(f_start, ff_start);
            if (cs < 0)
                break;

            WADEntry* ent = wads[i]->getEntry(cs);
            QString lastent = (ent->getName()=="F_START")?"F_END":"FF_END";
            int ce = wads[i]->getNumForName(lastent, cs+1);
            fnum = ce+1;

            // everything between cs+1 and ce-1 (inclusive) is a flat.
            for (int j = cs+1; j < ce; j++)
            {
                WADEntry* flat = wads[i]->getEntry(j);
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

        // fetch all textures.
        QVector<DoomTexture1Texture> texture1 = Tex_ReadTexture1(wads[i], "TEXTURE1");
        QVector<DoomTexture1Texture> texture2 = Tex_ReadTexture1(wads[i], "TEXTURE2");
        qDebug("%d texture1 textures, %d texture2 textures", texture1.size(), texture2.size());
        for (int j = 0; j < texture1.size(); j++)
            PutTexture(Textures, texture1[j].name.toUpper(), RenderTexture(wads[i], texture1[j]));
        for (int j = 0; j < texture2.size(); j++)
            PutTexture(Textures, texture2[j].name.toUpper(), RenderTexture(wads[i], texture2[j]));

        delete wads[i];
        wads[i] = 0;
    }
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

QVector<DoomTexture1Texture> Tex_ReadTexture1(WADFile* wad, QString lumpname)
{
    QVector<DoomTexture1Texture> out;
    int texturex = wad->getLastNumForName(lumpname);
    if (texturex < 0)
        return out;
    // first read PNAMES, if we can't read PNAMES, return null
    int pnames = wad->getLastNumForName("PNAMES");
    if (pnames < 0)
        return out;

    QStringList pnames_unp;
    QDataStream pnames_stream(wad->getEntry(pnames)->getData());
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
    QDataStream texturex_stream(wad->getEntry(texturex)->getData());
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

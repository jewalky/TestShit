#include "doommap.h"
#include <QBuffer>
#include <QDataStream>
#include <QVector>
#include <QPolygonF>
#include <QPointF>
#include <QLineF>
#include <QxPoly2Tri>

DoomMap::DoomMap()
{

}

DoomMap::DoomMap(WADFile *wad, QString name)
{
    // find the last name.
    int snum = wad->getSize();
    while (true)
    {
        int num = wad->getLastNumForName(name, snum);
        snum = num-1;

        WADEntry* nextent = wad->getEntry(num+1);
        if (!nextent)
            continue;

        if (nextent->getName().toUpper() == "THINGS") // classic map
        {
            bool validmap = true;
            QStringList entriesOrder = QStringList() << "THINGS" << "LINEDEFS" << "SIDEDEFS" << "VERTEXES" << "SEGS" << "SSECTORS" << "NODES" << "SECTORS" << "REJECT" << "BLOCKMAP";
            WADEntry* entries[10] = {0};
            entries[0] = nextent;
            for (int i = 1; i < entriesOrder.size(); i++)
            {
                WADEntry* ent = wad->getEntry(num+1+i);
                if (!ent)
                {
                    validmap = false;
                    break;
                }

                entries[i] = ent;
            }

            if (!validmap)
                continue;

            //
            // check the presence of BEHAVIOR
            //hasbehavior = wad->getEntry(num+entriesOrder.size()) && (wad->getEntry(num+entriesOrder.size())->getName().toUpper() == "BEHAVIOR");
            WADEntry* behent = wad->getEntry(num+entriesOrder.size()+1);
            if (behent && behent->getName().toUpper() == "BEHAVIOR") // hexen map
            {
                behavior = behent->getData();
                WADEntry* scriptsent = wad->getEntry(num+entriesOrder.size()+2);
                if (scriptsent && scriptsent->getName().toUpper() == "SCRIPTS")
                    scripts = QString::fromUtf8(scriptsent->getData());
                type = Hexen;
            }
            else type = Doom;

            // load the map.
            QBuffer _things(&entries[0]->getData());
            QBuffer _linedefs(&entries[1]->getData());
            QBuffer _sidedefs(&entries[2]->getData());
            QBuffer _vertexes(&entries[3]->getData());
            QBuffer _sectors(&entries[7]->getData());
            _things.open(QIODevice::ReadOnly);
            _linedefs.open(QIODevice::ReadOnly);
            _sidedefs.open(QIODevice::ReadOnly);
            _vertexes.open(QIODevice::ReadOnly);
            _sectors.open(QIODevice::ReadOnly);

            initClassic(&_things, &_linedefs, &_sidedefs, &_vertexes, &_sectors);
            break;
        }
        else if (nextent->getName().toUpper() == "TEXTMAP") // textmap
        {
            int endnum = wad->getNumForName("ENDMAP", num+1);
            if (endnum < 0)
                continue; // invalid map
            int behnum = wad->getNumForName("BEHAVIOR", num+1);
            int scriptsnum = wad->getNumForName("SCRIPTS", num+1);
            if (behnum >= 0 && behnum < endnum)
                behavior = wad->getEntry(behnum)->getData();
            if (scriptsnum >= 0 && scriptsnum < endnum)
                scripts = QString::fromUtf8(wad->getEntry(scriptsnum)->getData());
            type = UDMF;

            // load the map
            initUDMF(QString::fromUtf8(nextent->getData()));
            break;
        }
        else continue;
    }

    // triangulate sectors
    for (int i = 0; i < sectors.size(); i++)
        sectors[i].triangulate();
}

static int DetectMapsSorter(const void* a, const void* b)
{
    DetectedDoomMap* ma = (DetectedDoomMap*)a;
    DetectedDoomMap* mb = (DetectedDoomMap*)b;
    return ma->name.compare(mb->name);
}

QVector<DetectedDoomMap> DoomMap::detectMaps(WADFile* wad)
{
    QVector<DetectedDoomMap> maps;

    int snum = 0;
    while (true)
    {
        // find next THINGS lump or TEXTMAP lump.
        int tnum = wad->getNumForName("THINGS", snum);
        int unum = wad->getNumForName("TEXTMAP", snum);
        int num = -1;
        if (tnum < 0)
            num = unum;
        else if (unum < 0)
            num = tnum;
        else num = std::min(tnum, unum);

        if (num < 0) // end of maps
            break;

        snum = num+1;

        // check correct order of lumps if this is a text map
        bool hasbehavior = false;
        MapType type;
        if (num == unum)
        {
            type = UDMF;
            int endmap = wad->getNumForName("ENDMAP", snum);
            if (endmap < 0)
                continue; // invalid textmap
            int behnum = wad->getNumForName("BEHAVIOR");
            hasbehavior = behnum >= 0 && behnum < endmap;
        }
        else
        {
            bool validmap = true;
            QStringList entriesOrder = QStringList() << "THINGS" << "LINEDEFS" << "SIDEDEFS" << "VERTEXES" << "SEGS" << "SSECTORS" << "NODES" << "SECTORS" << "REJECT" << "BLOCKMAP";
            for (int i = 1; i < entriesOrder.size(); i++)
            {
                WADEntry* nextent = wad->getEntry(num+i);
                if (nextent == 0 || (nextent->getName().toUpper() != entriesOrder[i]))
                {
                    validmap = false;
                    break;
                }
            }

            if (!validmap)
                continue;

            hasbehavior = wad->getEntry(num+entriesOrder.size()) && (wad->getEntry(num+entriesOrder.size())->getName().toUpper() == "BEHAVIOR");
            if (hasbehavior)
                type = Hexen;
            else
            {
                // detect strife. or not.
                type = Doom;
            }
        }

        WADEntry* prevent = wad->getEntry(num-1);
        if (prevent == 0)
            continue; // :(

        DetectedDoomMap ddm;
        ddm.name = prevent->getName().toUpper();
        ddm.type = type;
        maps.append(ddm);
    }

    // sort maps by name
    qsort(maps.data(), maps.size(), sizeof(DetectedDoomMap), &DetectMapsSorter);

    return maps;
}

void DoomMap::initUDMF(QString text)
{

}

void DoomMap::initClassic(QIODevice* things, QIODevice* linedefs, QIODevice* sidedefs, QIODevice* vertexes, QIODevice* sectors)
{
    // linedefs, sidedefs, sectors
    // one vertex = 4 bytes
    int numvertexes = vertexes->size() / 4;
    QDataStream vertexes_stream(vertexes);
    vertexes_stream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < numvertexes; i++)
    {
        qint16 x;
        qint16 y;
        vertexes_stream >> x >> y;
        DoomMapVertex vx(this);
        vx["x"] = (float)x;
        vx["y"] = (float)y;
        vertices.append(vx);
    }

    // one linedef = 14 bytes for Doom, and 16 bytes for Hexen
    int numlinedefs = (type == Hexen) ? linedefs->size() / 16 : linedefs->size() / 14;
    QDataStream linedefs_stream(linedefs);
    linedefs_stream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < numlinedefs; i++)
    {
        if (type == Hexen)
        {
            quint16 v1;
            quint16 v2;
            quint16 flags;
            quint8 special;
            quint8 arg0;
            quint8 arg1;
            quint8 arg2;
            quint8 arg3;
            quint8 arg4;
            quint16 sidefront;
            quint16 sideback;
            linedefs_stream >> v1 >> v2 >> flags >> special >> arg0 >> arg1 >> arg2 >> arg3 >> arg4 >> sidefront >> sideback;
            DoomMapLinedef ln(this);
            ln["v1"] = (int)v1;
            ln["v2"] = (int)v2;
            ln["special"] = (int)special;
            ln["arg0"] = (int)arg0;
            ln["arg1"] = (int)arg1;
            ln["arg2"] = (int)arg2;
            ln["arg3"] = (int)arg3;
            ln["arg4"] = (int)arg4;
            ln["sidefront"] = (sidefront < 0xFFFF) ? (int)sidefront : -1;
            ln["sideback"] = (sideback < 0xFFFF) ? (int)sideback : -1;
            // todo parse flags
            this->linedefs.append(ln);
        }
        else
        {
            quint16 v1;
            quint16 v2;
            quint16 flags;
            quint16 special;
            quint16 tag;
            quint16 sidefront;
            quint16 sideback;
            linedefs_stream >> v1 >> v2 >> flags >> special >> tag >> sidefront >> sideback;
            DoomMapLinedef ln(this);
            ln["v1"] = (int)v1;
            ln["v2"] = (int)v2;
            ln["special"] = (int)special;
            ln["id"] = (int)tag;
            ln["sidefront"] = (sidefront < 0xFFFF) ? (int)sidefront : -1;
            ln["sideback"] = (sideback < 0xFFFF) ? (int)sideback : -1;
            // todo parse flags
            this->linedefs.append(ln);
        }
    }

    // one sidedef = 30 bytes
    int numsidedefs = sidedefs->size() / 30;
    QDataStream sidedefs_stream(sidedefs);
    sidedefs_stream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < numsidedefs; i++)
    {
        qint16 offsetx;
        qint16 offsety;
        char rtexturetop[9] = {0};
        char rtexturebottom[9] = {0};
        char rtexturemiddle[9] = {0};
        quint16 sector;
        sidedefs_stream >> offsetx >> offsety;
        sidedefs_stream.readRawData(rtexturetop, 8);
        sidedefs_stream.readRawData(rtexturebottom, 8);
        sidedefs_stream.readRawData(rtexturemiddle, 8);
        sidedefs_stream >> sector;
        DoomMapSidedef sd(this);
        sd["offsetx"] = (float)offsetx;
        sd["offsety"] = (float)offsety;
        sd["texturetop"] = QString::fromUtf8(rtexturetop);
        sd["texturebottom"] = QString::fromUtf8(rtexturebottom);
        sd["texturemiddle"] = QString::fromUtf8(rtexturemiddle);
        sd["sector"] = (int)sector;
        this->sidedefs.append(sd);
    }

    // one sector = 26 bytes
    int numsectors = sectors->size() / 26;
    QDataStream sectors_stream(sectors);
    sectors_stream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < numsectors; i++)
    {
        qint16 heightfloor;
        qint16 heightceiling;
        char texturefloor[9] = {0};
        char textureceiling[9] = {0};
        qint16 lightlevel;
        quint16 special;
        quint16 tag;
        sectors_stream >> heightfloor >> heightceiling;
        sectors_stream.readRawData(texturefloor, 8);
        sectors_stream.readRawData(textureceiling, 8);
        sectors_stream >> lightlevel >> special >> tag;
        DoomMapSector sec(this);
        sec["heightfloor"] = (float)heightfloor;
        sec["heightceiling"] = (float)heightceiling;
        sec["texturefloor"] = QString::fromUtf8(texturefloor);
        sec["textureceiling"] = QString::fromUtf8(textureceiling);
        sec["lightlevel"] = (int)lightlevel;
        sec["special"] = (int)special;
        sec["tag"] = (int)tag;
        this->sectors.append(sec);
    }
}

static float DoomMapSectorArea(QPolygonF& p)
{
    float at = 0;
    for (int i = 0; i < p.size(); i++)
    {
        QPointF& p1 = p[i];
        QPointF& p2 = p[(i+1)%p.size()];
        float ac = (p1.x()*p2.y()) - (p2.x()*p1.y());
        at += ac;
    }

    return at / 2;
}

static int DoomMapSectorAreaSort(const void* a, const void* b)
{
    QPolygonF* pa = (QPolygonF*)a;
    QPolygonF* pb = (QPolygonF*)b;
    float aa = DoomMapSectorArea(*pa);
    float ab = DoomMapSectorArea(*pb);

    if (aa < ab) return -1;
    if (aa > ab) return 1;
    return 0;
}

// linedefs = total linedefs for current sector
// checkedlinedefs = linedefs that are already assigned to polygons
// prevld = previous linedef (v2 of which is used)
// sector = current sector
static DoomMapLinedef* DoomMapSectorResolveIntersection(QVector<DoomMapLinedef*>& linedefs, QVector<DoomMapLinedef*>& checkedlinedefs, DoomMapLinedef* prevld, DoomMapSector* sector)
{
    // an intersection is when a single vertex points to multiple lines.
    // we resolve this by picking the line with the smallest angle.
    DoomMapVertex* ov2 = prevld->getV2(sector); // this is the vertex that has multiple endpoints (or not)
    DoomMapVertex* ov1 = prevld->getV1(sector);

    qreal minAngle = 360;
    DoomMapLinedef* minLinedef = 0;

    // check if current vertex is v1 for more than one linedef.
    for (int i = 0; i < linedefs.size(); i++)
    {
        DoomMapLinedef* linedef = linedefs[i];
        if (checkedlinedefs.contains(linedef))
            continue;

        DoomMapVertex* v1 = linedef->getV1(sector);
        DoomMapVertex* v2 = linedef->getV2(sector);
        if (v1 == ov2 ||
            (v1->getX() == ov2->getX() &&
             v1->getY() == ov2->getY()))
        {
            // this line has v1 set at v2 of the reference line.
            // calculate angle.
            qreal cAngle = QLineF(ov1->getX(), ov1->getY(), ov2->getX(), ov2->getY()).angleTo(QLineF(v1->getX(), v1->getY(), v2->getX(), v2->getY()));
            if (cAngle < minAngle)
            {
                DoomMap* map = sector->getParent();
                qDebug("line %d has angle %f to line %d for sector %d",
                       linedef-map->linedefs.data(), cAngle, prevld-map->linedefs.data(), sector-map->sectors.data());
                minLinedef = linedef;
                minAngle = cAngle;
            }
        }
    }

    return minLinedef;
}

void DoomMapSector::triangulate()
{
    triangles.vertices.clear();
    // first, split the sector into line loops (polygons)
    // find all lines that have this sector as reference.
    DoomMap* p = getParent();
    if (!p) return;

    QVector<DoomMapLinedef*> linedefs;
    for (int i = 0; i < p->linedefs.size(); i++)
    {
        DoomMapLinedef* linedef = &p->linedefs[i];
        DoomMapSidedef* front = linedef->getFront();
        DoomMapSidedef* back = linedef->getBack();

        bool s1 = (front && front->getSector() == this);
        bool s2 = (back && back->getSector() == this);

        if (s1 && s2) // ignore self-referencing lines
            continue;

        if (s1 || s2)
            linedefs.append(linedef);
    }

    // go through linedefs in clockwise order and split the sector into separate shapes.
    // v1->v2 for lines that face this sector.
    // v2->v1 for lines that don't face this sector.
    QVector<QPolygonF> polygons;
    QVector<DoomMapLinedef*> checkedlinedefs;
    while (checkedlinedefs.size() < linedefs.size())
    {
        DoomMapVertex* vs = 0;
        DoomMapVertex* vp = 0;
        DoomMapLinedef* lastlinedef = 0;

        QPolygonF poly;

        QVector<DoomMapLinedef*> lastcheckedlinedefs;
        QVector<DoomMapVertex*> checkedvertices;
        bool dbrk = false;
        while (!dbrk)
        {
            dbrk = true;
            for (int i = 0; i < linedefs.size(); i++)
            {
                DoomMapLinedef* linedef = linedefs[i];

                if (checkedlinedefs.contains(linedef))
                    continue; //

                DoomMapVertex* v1 = linedef->getV1(this);
                DoomMapVertex* v2 = linedef->getV2(this);

                //
                if (!vs)
                {
                    poly.append(QPointF(v1->getX(), v1->getY()));
                    checkedvertices.append(v1);
                    vs = v1;
                    vp = v2;
                    lastcheckedlinedefs.append(linedef);
                    lastlinedef = linedef;
                    dbrk = false;
                    continue;
                }

                if (vp) // check if this line is the next one.
                {
                    if (vp == v1 ||
                        (vp->getX() == v1->getX() &&
                         vp->getY() == v1->getY()))
                    {
                        // check if we need this particular line. sometimes, more than one line matches the same vertex.
                        if (lastlinedef && DoomMapSectorResolveIntersection(linedefs, checkedlinedefs, lastlinedef, this) != linedef)
                            continue;

                        lastlinedef = linedef;

                        if (checkedvertices.contains(v1))
                        {
                            dbrk = true;
                            break; // unclosed sector
                        }

                        // add this vertex
                        poly.append(QPointF(v1->getX(), v1->getY()));
                        checkedvertices.append(v1);
                        vp = v2;
                        lastcheckedlinedefs.append(linedef);
                        dbrk = (vp == vs || (vp->getX() == vs->getX() && vp->getY() == vs->getY()));
                        continue;
                    }
                }
            }
        }

        for (int i = 0; i < lastcheckedlinedefs.size(); i++)
            checkedlinedefs.append(lastcheckedlinedefs[i]);

        // check poly size. <3 points is bad
        if (poly.size() < 3)
            continue;

        polygons.append(poly);
    }

    if (polygons.size() < 1)
        return;

    qsort(polygons.data(), polygons.size(), sizeof(QPolygonF), &DoomMapSectorAreaSort);
    qDebug("sector %d; first polygon = %d", this-p->sectors.data(), polygons[0].size());

    // for each polygon, check intersections and add holes.
    for (int i = 0; i < polygons.size(); i++)
    {
        QPolygonF& poly = polygons[i];

        QList<QPolygonF> holes;
        for (int j = 0; j < polygons.size(); j++) // note: self-intersecting polygons will be handled very very bad here
        {
            if (i == j) continue;
            // check intersection
            if (poly.intersected(polygons[j]).size())
            {
                qDebug("sector %d; polygon %d; has hole %d!", this-p->sectors.data(), i, j);
                // add hole
                holes.append(polygons[j]);
                // remove hole from polygons list
                polygons.removeAt(j);
                j--;
            }
        }

        QxPoly2Tri p2tri;
        QList<QPointF> steinerPoints;

        p2tri.triangulate(poly, holes, steinerPoints);

        // make GL array now
        QVector<int> tri_indices = p2tri.indices();
        QVector<QPointF> tri_points = p2tri.points();
        for (int i = 0; i < tri_indices.size(); i++)
        {
            GLVertex v;
            v.x = tri_points[tri_indices[i]].x();
            v.y = -tri_points[tri_indices[i]].y();
            v.z = 0;
            v.r = v.g = v.b = 255;
            v.a = 64;
            v.u = v.v = 0; // todo: set texture coordinates based on 64 grid
            triangles.vertices.append(v);
        }
    }
}

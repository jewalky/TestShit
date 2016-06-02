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
        vx.x = (float)x;
        vx.y = (float)y;
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
            ln.v1 = (int)v1;
            ln.v2 = (int)v2;
            ln.special = (int)special;
            ln.arg0 = (int)arg0;
            ln.arg1 = (int)arg1;
            ln.arg2 = (int)arg2;
            ln.arg3 = (int)arg3;
            ln.arg4 = (int)arg4;
            ln.sidefront = (sidefront < 0xFFFF) ? (int)sidefront : -1;
            ln.sideback = (sideback < 0xFFFF) ? (int)sideback : -1;
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
            ln.v1 = (int)v1;
            ln.v2 = (int)v2;
            ln.special = (int)special;
            ln.id = (int)tag;
            ln.sidefront = (sidefront < 0xFFFF) ? (int)sidefront : -1;
            ln.sideback = (sideback < 0xFFFF) ? (int)sideback : -1;
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
        sd.offsetx = (float)offsetx;
        sd.offsety = (float)offsety;
        sd.texturetop = QString::fromUtf8(rtexturetop);
        sd.texturebottom = QString::fromUtf8(rtexturebottom);
        sd.texturemiddle = QString::fromUtf8(rtexturemiddle);
        sd.sector = (int)sector;
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
        sec.heightfloor = (float)heightfloor;
        sec.heightceiling = (float)heightceiling;
        sec.texturefloor = QString::fromUtf8(texturefloor);
        sec.textureceiling = QString::fromUtf8(textureceiling);
        sec.lightlevel = (int)lightlevel;
        sec.special = (int)special;
        sec.id = (int)tag;
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

    at /= 2;
    if (at < 0) at = -at;
    return at;
}

static int DoomMapSectorAreaSort(const void* a, const void* b)
{
    QPolygonF* pa = (QPolygonF*)a;
    QPolygonF* pb = (QPolygonF*)b;
    float aa = DoomMapSectorArea(*pa);
    float ab = DoomMapSectorArea(*pb);

    if (aa < ab) return 1;
    if (aa > ab) return -1;
    return 0;
}

QPair< QPolygonF, QVector<DoomMapLinedef*> > SectorPolygonTracer::nextPolygon(DoomMapLinedef* line)
{
    //
    DoomMap* p = line->getParent();

    int numsector = sector-p->sectors.data();

    DoomMapVertex* vs = line->getV1(sector); // this is the vertex that we start from
    DoomMapVertex* vp = 0; // this is the next vertex to find

    QVector< QPair< QPolygonF, QVector<DoomMapLinedef*> > > results;

    bool log = false;//(numsector == 0);

    // scan twice, first with choosing larger angle, second with choosing smaller angle.
    for (int k = 0; k < 2; k++)
    {
        if (log) qDebug("sector %d, pass %d", numsector, k);
        if (log) qDebug("sector %d, line = %d", numsector, line-p->linedefs.data());

        vp = line->getV2(sector);

        QPolygonF poly;
        QVector<DoomMapLinedef*> polylines;

        poly.append(QPointF(vs->x, vs->y));
        polylines.append(line);

        DoomMapLinedef* prevld = line;

        bool dbrk = false;
        while (!dbrk)
        {
            DoomMapVertex* oldvp = vp;
            for (int i = 0; i < linedefs.size(); i++)
            {
                DoomMapLinedef* linedef = linedefs[i];
                DoomMapVertex* v1 = linedef->getV1(sector);
                DoomMapVertex* v2 = linedef->getV2(sector);

                // if this vertex is equal to vp, this is a next line in the current shape
                if (vp)
                {
                    if (vp == v1 ||
                        (vp->x == v1->x &&
                         vp->y == v1->y))
                    {
                        // check if this is the first line.
                        if ((vs == v1 ||
                            (vs->x == v1->x &&
                             vs->y == v1->y)))
                        {
                            //return QPair< QPolygonF, QVector<DoomMapLinedef*> > (QPolygonF(), QVector<DoomMapLinedef*>());
                            // successful trace, add polygon
                            dbrk = true;
                            if (log) qDebug("sector %d, pass closed", numsector);
                            break;
                        }

                        // otherwise make sure this linedef wasn't checked already.
                        if (checkedlinedefs.contains(linedefs[i]) || polylines.contains(linedefs[i]))
                            continue;

                        // here we have multiple possible outcomes. to reduce recursive calls, check if there are multiple lines that start with this vertex and only recurse if there are.
                        QVector<DoomMapLinedef*> outcomes;
                        outcomes.append(linedef);

                        for (int j = 0; j < linedefs.size(); j++)
                        {
                            if (checkedlinedefs.contains(linedefs[j]) || polylines.contains(linedefs[j]))
                                continue;

                            if (j == i)
                                continue;

                            DoomMapVertex* ov1 = linedefs[j]->getV1(sector);
                            if (vp == ov1 ||
                                (vp->x == ov1->x &&
                                 vp->y == ov1->y))
                            {
                                outcomes.append(linedefs[j]);
                            }
                        }

                        if (outcomes.size() == 1)
                        {
                            vp = v2;
                            poly.append(QPointF(v1->x, v1->y));
                            polylines.append(linedef);
                            prevld = linedef;
                            if (log) qDebug("sector %d, line = %d", numsector, linedef-p->linedefs.data());
                        }
                        else
                        {
                            DoomMapLinedef* refLinedef;
                            float refAngle;
                            if (k == 0) refAngle = 360;
                            else if (k == 1) refAngle = -1;

                            DoomMapVertex* pv1 = prevld->getV1(sector);
                            DoomMapVertex* pv2 = prevld->getV2(sector);

                            for (int j = 0; j < outcomes.size(); j++)
                            {
                                DoomMapLinedef* cLinedef = outcomes[j];
                                DoomMapVertex* cV2 = cLinedef->getV2(sector);
                                //float cAngle = QLineF(QPointF(pv1->x, pv1->y), QPointF(pv2->x, pv2->y)).angleTo(QLineF(QPointF(v1->x, v1->y), QPointF(cV2->x, cV2->y)));
                                //float cAngle = QLineF(QPointF(v1->x, v1->y), QPointF(pv1->x, pv1->y)).angleTo(QLineF(QPointF(v1->x, v1->y), QPointF(cV2->x, cV2->y)));
                                float cAngle = QLineF(v1->x, v1->y, cV2->x, cV2->y).angleTo(QLineF(v1->x, v1->y, pv1->x, pv1->y));
                                if (log) qDebug("sector %d, possible line = %d, angle %f", numsector, cLinedef-p->linedefs.data(), cAngle);
                                if ((k == 0 && cAngle < refAngle) ||
                                    (k == 1 && cAngle > refAngle))
                                {
                                    vp = cV2;
                                    refLinedef = cLinedef;
                                    refAngle = cAngle;
                                }
                            }

                            poly.append(QPointF(v1->x, v1->y));
                            polylines.append(refLinedef);
                            prevld = refLinedef;
                            if (log) qDebug("sector %d, line = %d", numsector, refLinedef-p->linedefs.data());
                        }
                    }
                }
            }

            if (oldvp == vp && !dbrk)
            {
                // no next vertex found. invalid polygon.
                poly.clear();
                polylines.clear();
                dbrk = true;
                if (log) qDebug("sector %d, pass NOT closed", numsector);
                break;
            }
        }

        if (poly.size())
            results.append(QPair< QPolygonF, QVector<DoomMapLinedef*> > (poly, polylines));
    }

    // if there are no results, this empty pair is returned. it's okay and expected.
    double refArea = 4294967296;
    QPair< QPolygonF, QVector<DoomMapLinedef*> > refResult;
    for (int i = 0; i < results.size(); i++)
    {
        double cArea = DoomMapSectorArea(results[i].first);
        if (cArea < refArea)
        {
            refResult = results[i];
            refArea = cArea;
        }
    }

    if (log) qDebug("sector %d, result has %d vertices", numsector, refResult.first.size());

    return refResult;
}

void DoomMapSector::triangulate()
{
    triangles.vertices.clear();
    // first, split the sector into line loops (polygons)
    SectorPolygonTracer spt(this);

    DoomMap* p = getParent();

    // go through linedefs in clockwise order and split the sector into separate shapes.
    QVector<QPolygonF> polygons = spt.getPolygons();

    if (polygons.size() < 1)
        return;

    qsort(polygons.data(), polygons.size(), sizeof(QPolygonF), &DoomMapSectorAreaSort);

    int sectornum = this-p->sectors.data();
    //qDebug("triangulating sector %d, got %d polygons", sectornum, polygons.size());

    for (int i = 0; i < polygons.size(); i++)
    {
        QPolygonF& poly = polygons[i];
        for (int k = 0; k < poly.size(); k++)
        {
            QPointF prevpt = (k == 0) ? (poly[poly.size()-1]) : poly[k-1];
            QPointF nextpt = poly[(k+1)%poly.size()];

            if (!QLineF(prevpt, poly[k]).angleTo(QLineF(poly[k], nextpt)))
            {
                // delete this vertex
                poly.removeAt(k);
                k--;
            }
        }
    }

    for (int i = 0; i < polygons.size(); i++)
    {
        QPolygonF& poly = polygons[i];
        // remove too small poly
        if (poly.size() < 3)
        {
            polygons.removeAt(i);
            i--;
        }

        ///////// GROSS HACK
        /// this is made to circumvent the "no vertices should occupy the same position" rule. if this is not present, poly2tri will randomly crash.
        float offs = (float)0.000001;
        for (int k = 0; k < poly.size(); k++)
        {
            // nested gross hack
            QPointF prevpt = (k == 0) ? (poly[poly.size()-1]) : poly[k-1];
            QPointF nextpt = poly[(k+1)%poly.size()];

            //QLineF midline(0, 0, 1, 0);
            QLineF vec1 = QLineF(poly[k], prevpt).unitVector();
            QLineF vec2 = QLineF(poly[k], nextpt).unitVector();
            QLineF midline(0, 0, (vec1.dx()+vec2.dx())/2, (vec1.dy()+vec2.dy())/2);
            //midline.setAngle((QLineF(poly[k], prevpt).angle()+QLineF(poly[k], nextpt).angle())/2);

            poly[k].setX(poly[k].x()+midline.dx()*offs);
            poly[k].setY(poly[k].y()+midline.dy()*offs);
        }
        ///////// END GROSS HACK
    }

    //qDebug("%d polygons", polygons.size());
    //polygons.resize(2);

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
                // make sure this poly doesn't intersect any holes. if it does, it's probably a nested sector and that will be handled separately.
                bool ishole = true;
                for (int k = 0; k < holes.size(); k++)
                {
                    if (holes[k].intersected(polygons[j]).size())
                    {
                        ishole = false;
                        break;
                    }
                }

                if (!ishole)
                {
                    //qDebug("sector %d, polygon %d, hole %d is not a hole!", sectornum, i, j);
                    continue;
                }

                //qDebug("sector %d, polygon %d has hole %d!", sectornum, i, j);

                QPolygonF& hole = polygons[j];
                // add hole
                holes.append(hole);
                // remove hole from polygons list
                polygons.removeAt(j);
                j--;
            }
            //else qDebug("sector %d, polygon %d has no hole %d!", sectornum, i, j);
        }

        QxPoly2Tri p2tri;
        QList<QPointF> steinerPoints;

        p2tri.triangulate(poly, holes, steinerPoints);

        // make GL array now
        QVector<int> tri_indices = p2tri.indices();
        QVector<QPointF> tri_points = p2tri.points();
        for (int j = 0; j < tri_indices.size(); j++)
        {
            GLVertex v;
            v.x = tri_points[tri_indices[j]].x();
            v.y = -tri_points[tri_indices[j]].y();
            v.z = 0;
            v.r = v.g = v.b = 255;
            v.a = 64;
            v.u = v.v = 0; // todo: set texture coordinates based on 64 grid
            triangles.vertices.append(v);
        }

        polygons.removeAt(i);
        i--;
    }
}

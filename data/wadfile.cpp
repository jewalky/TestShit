#include "wadfile.h"

#include <QFile>
#include <QDataStream>

WADFile::WADFile(QIODevice* device)
{
    // remember beginning of stream
    qint64 zoffs = device->pos();

    QDataStream qds(device);
    qds.setByteOrder(QDataStream::LittleEndian);

    char rsig[5];
    rsig[4] = 0;
    qds.readRawData(rsig, 4);
    QString sig(rsig);

    if (sig != "IWAD" && sig != "PWAD")
    {
        setError("Not a WAD file (unknown signature: "+sig+")");
        return;
    }

    qint32 numentries;
    quint32 fatoffs;

    qds >> numentries >> fatoffs;

    WADNamespace current_ns = NS_Global;
    QStringList namespaces; namespaces << "S" << "SS" << "F" << "FF" << "C" << "A" << "TX" << "V" << "HI" << "VX";

    //
    for (int i = 0; i < numentries; i++)
    {
        device->seek(zoffs+fatoffs+i*16);

        quint32 lmp_offset;
        quint32 lmp_len;
        char lmp_rname[9];
        lmp_rname[8] = 0;

        qds >> lmp_offset >> lmp_len;
        qds.readRawData(lmp_rname, 8);

        QString lmp_name(lmp_rname);
        lmp_name = lmp_name.toUpper();

        bool this_ns = false;
        for (int j = 0; j < namespaces.size(); j++)
        {
            if (lmp_name == namespaces[j]+"_START")
            {
                if (namespaces[j] == "S" || namespaces[j] == "SS")
                    current_ns = NS_Sprites;
                else if (namespaces[j] == "F" || namespaces[j] == "FF")
                    current_ns = NS_Flats;
                else if (namespaces[j] == "C")
                    current_ns = NS_Colormaps;
                else if (namespaces[j] == "A")
                    current_ns = NS_ACS;
                else if (namespaces[j] == "TX")
                    current_ns = NS_Textures;
                else if (namespaces[j] == "V")
                    current_ns = NS_Voices;
                else if (namespaces[j] == "HI")
                    current_ns = NS_Hires;
                else if (namespaces[j] == "VX")
                    current_ns = NS_Voxels;
                else current_ns = NS_Global;
                this_ns = true;
            }
            else if (lmp_name == namespaces[j]+"_END")
            {
                current_ns = NS_Global;
                this_ns = true;
            }
        }

        device->seek(zoffs+lmp_offset);
        QByteArray lmp_data = device->read(lmp_len);

        WADEntry* ent = new WADEntry(lmp_name.toUpper(), lmp_offset, this_ns?NS_Global:current_ns, lmp_data);
        entries.append(ent);
    }

    valid = true;
}

WADFile* WADFile::fromFile(QString filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return new WADFile(&f);
}

WADFile* WADFile::fromStream(QIODevice* device)
{
    return new WADFile(device);
}

WADEntry* WADFile::getEntry(int num)
{
    if (num >= 0 && num < entries.size())
        return entries[num];
    return 0;
}

WADEntry* WADFile::removeEntry(int num)
{
    if (num >= entries.size() || num < 0)
        return 0;
    WADEntry* ent = entries[num];
    entries.removeAt(num);
    return ent;
}

void WADFile::putEntry(int num, WADEntry* ent)
{
    if (num < 0)
        num = 0;
    if (num >= entries.size())
    {
        int onum = entries.size();
        entries.resize(num);
        for (int i = onum; i < entries.size(); i++)
            entries[i] = 0;
    }

    entries.insert(num, ent);
}

int WADFile::getNumForName(QString name, int num, WADNamespace ns)
{
    name = name.toUpper();
    for (int i = num; i < entries.size(); i++)
    {
        if (!entries[i])
            continue;
        if (ns != NS_Any && entries[i]->getNamespace() != ns)
            continue;
        if (entries[i]->getName().toUpper() == name)
            return i;
    }

    return -1;
}

int WADFile::getLastNumForName(QString name, int num, WADNamespace ns)
{
    if (num >= getSize() || num < 0)
        num = getSize()-1;

    name = name.toUpper();
    for (int i = num; i >= 0; i--)
    {
        if (!entries[i])
            continue;
        if (ns != NS_Any && entries[i]->getNamespace() != ns)
            continue;
        if (entries[i]->getName().toUpper() == name)
            return i;
    }

    return -1;
}

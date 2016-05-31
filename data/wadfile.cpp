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

        device->seek(zoffs+lmp_offset);
        QByteArray lmp_data = device->read(lmp_len);

        WADEntry* ent = new WADEntry(lmp_name.toUpper(), lmp_offset, lmp_data);
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

int WADFile::getNumForName(QString name, int num)
{
    name = name.toUpper();
    for (int i = num; i < entries.size(); i++)
    {
        if (!entries[i])
            continue;
        if (entries[i]->getName().toUpper() == name)
            return i;
    }

    return -1;
}

int WADFile::getLastNumForName(QString name, int num)
{
    if (num >= getSize())
        num = getSize()-1;

    name = name.toUpper();
    for (int i = num; i >= 0; i--)
    {
        if (!entries[i])
            continue;
        if (entries[i]->getName().toUpper() == name)
            return i;
    }

    return -1;
}

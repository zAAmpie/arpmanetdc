#include "util.h"

QByteArray base32Encode(byte *input, int inputLength)
{
    int encodedLength = Base32::GetEncode32Length(inputLength);
    byte *b32text = new byte[encodedLength];
    if (!Base32::Encode32(input, inputLength, b32text))
    {
        return QByteArray();
    }

    char *alpha = new char[32];
    memcpy(alpha, base32Alpha, 32);
    Base32::Map32(b32text, encodedLength, (byte *)alpha);

    QByteArray output((const char*)b32text, encodedLength);

    delete [] b32text;
    delete [] alpha;
    return output;
}

bool base32Decode(QByteArray input, byte *output, int &outputLength)
{
    char *alpha = new char[32];
    memcpy(alpha, base32Alpha, 32);
    
    byte *b32text = new byte[input.size()];
    memcpy(b32text, (byte *)input.data(), input.size());
    
    Base32::Unmap32(b32text, input.size(), (byte *)alpha);
    
    //outputLength = Base32::GetDecode32Length(input.size());
    byte *outputBuff = new byte[outputLength];

    bool res = Base32::Decode32(b32text, input.size(), outputBuff);
    
    delete [] b32text;
    delete [] alpha;

    memmove(output, outputBuff, outputLength);
    delete [] outputBuff;
    return res;
}

//More intuitive base32 methods
bool base32Encode(QByteArray &data)
{
    if (data.isEmpty())
        return false;

    QByteArray res = base32Encode((byte *)data.data(), data.size());
    if (res.isEmpty())
        return false;

    data = res;
    return true;
}

bool base32Decode(QByteArray &data)
{
    if (data.isEmpty())
        return false;

    int outputLength = Base32::GetDecode32Length(data.size());
    byte *output = new byte[outputLength];
    
    bool res = base32Decode(data.data(), output, outputLength);
    if (!res)
    {
        delete [] output;
        return false;
    }

    data = QByteArray((char *)output, outputLength);
    delete [] output;
    return true;
}

//Function to convert quint64 containing bytes into a human readable format
QString bytesToSize(quint64 bytes)
{
    QString unit = "bytes";
    double d = bytes;
    if (d >= 1024.0)
	{
		d /= 1024.0;
		unit = "KiB";
		if (d >= 1024.0)
		{
			d /= 1024.0;
			unit = "MiB";
			if (d >= 1024.0)
			{
				d /= 1024.0;
				unit = "GiB";
				if (d >= 1024.0)
				{
					d /= 1024.0;
					unit = "TiB";
				}
			}
		}
	}

    return QObject::tr("%1 %2").arg(d, 0, 'f', 2).arg(unit);
}

//Function to convert bytes/second quint64 to human readable format
QString bytesToRate(quint64 rate)
{
    return QObject::tr("%1/s").arg(bytesToSize(rate).replace("bytes", "b"));
}

//Function to convert human readable size into bytes i.e. from 24.0428 MB to 25 210 706 bytes
quint64 sizeToBytes(QString size)
{
    //String can be 24MB / 24.56MB / 24.56 MB / 24.56 MiB etc
    QString regex = "(\\d+\\.?\\d+)\\s*([a-z]*)";
    QRegExp rx(regex, Qt::CaseInsensitive);

    double val;
    QString unit;
  
    if (rx.indexIn(size) != -1)
    {
        val = rx.cap(1).toDouble();
        unit = rx.cap(2);
    }

    if (unit.compare("TiB") == 0 || unit.compare("TB") == 0)
        val *= 1LL<<40;
    if (unit.compare("GiB") == 0 || unit.compare("GB") == 0)
        val *= 1<<30;
    if (unit.compare("MiB") == 0 || unit.compare("MB") == 0)
        val *= 1<<20;
    if (unit.compare("KiB") == 0 || unit.compare("KB") == 0)
        val *= 1<<10;

    return (quint64)val;
}

//Function to convert human readable rate into bytes
quint64 rateToBytes(QString rate)
{
    return sizeToBytes(rate.replace("/s", ""));
}

//Function to convert time duration in msecs to string
QString timeFromInt(qint64 timeMsecs)
{
    int hours = timeMsecs / 3600000;
    int minutes = (timeMsecs - hours * 3600000) / 60000;
    int seconds = (timeMsecs - hours * 3600000 - minutes * 60000) / 1000;

    QString hoursStr = QObject::tr("%1").arg(hours);
    if (hours <= 9)
        hoursStr.prepend("0");

    QString minutesStr = QObject::tr("%1").arg(minutes);
    if (minutes <= 9)
        minutesStr.prepend("0");

    QString secondsStr = QObject::tr("%1").arg(seconds);
    if (seconds <= 9)
        secondsStr.prepend("0");

    return QObject::tr("%1:%2:%3").arg(hoursStr).arg(minutesStr).arg(secondsStr);
}

//Function to convert uptime in msecs to string
QString uptimeFromInt(qint64 uptimeMsecs)
{
    int weeks = (uptimeMsecs / 604800000);
    uptimeMsecs -= weeks * 604800000;
    int days = (uptimeMsecs / 86400000);
    uptimeMsecs -= days * 86400000;
    int hours = uptimeMsecs / 3600000;
    uptimeMsecs -= hours * 3600000;
    int minutes = uptimeMsecs / 60000;
    uptimeMsecs -= minutes * 60000;
    int seconds = uptimeMsecs / 1000;
    uptimeMsecs -= seconds * 1000;

    QString weeksStr, daysStr, hoursStr, minutesStr, secondsStr;

    if (weeks > 0)
        weeksStr = QObject::tr("%1 week%2").arg(weeks).arg(weeks != 1 ? "s" : "");

    if (days > 0)
        daysStr = QObject::tr("%1 day%2").arg(days).arg(days != 1 ? "s" : "");
    if (weeks > 0)
        daysStr.prepend(", ");

    if (hours > 0)
        hoursStr = QObject::tr("%1 hour%2").arg(hours).arg(hours != 1 ? "s" : "");
    if (days > 0 || weeks > 0)
        hoursStr.prepend(", ");

    if (minutes > 0)
        minutesStr = QObject::tr("%1 minute%2").arg(minutes).arg(minutes != 1 ? "s" : "");
    if (hours > 0 || days > 0 || weeks > 0)
        minutesStr.prepend(", ");

    secondsStr = QObject::tr("%1 second%2").arg(seconds).arg(seconds != 1 ? "s" : "");
    
    if (minutes > 0 | hours > 0 || days > 0 || weeks > 0)
        secondsStr.prepend(", ");
    
    return QObject::tr("%1%2%3%4%5").arg(weeksStr).arg(daysStr).arg(hoursStr).arg(minutesStr).arg(secondsStr);
}

bool firstVersionLarger(QString first, QString second)
{
    QStringList firstList = first.split(".");
    QStringList secondList = second.split(".");

    //Ensure version lengths are the same
    while (firstList.size() > secondList.size())
        secondList.append("0");
    while (secondList.size() > firstList.size())
        firstList.append("0");

    for (int i = 0; i < firstList.size(); i++)
    {
        //Compare version numbers
        int f = firstList.at(i).toInt();
        int s = secondList.at(i).toInt();

        if (f > s)
            return true;
        else if (f < s)
            return false;
        //Else continue with next set of numbers
    }
    //They are the same! Check this
    return true;
}

void resizeRowsToContents(QTableView *tableView)
{
    if (!tableView)
        return;

	tableView->setRowHeight(0, 10);
    tableView->resizeRowToContents(0);
    for (int i = 1; i < tableView->model()->rowCount(); i++)
        tableView->setRowHeight(i, tableView->rowHeight(0));
}


// ------------------=====================   Sort STUKKENDE rubbish uit   =====================----------------------

// qbytearray append nie integers se rou data nie, hy maak droog.
// haastige implementering, kan later kyk watse potensiaal QVariant het om die drie funksies te herenig.
QByteArray toQByteArray(quint16 n)
{
    QByteArray a;
    quint16 mask = 0xff << 8;
    for (int i = 0; i < 16; i += 8)
    {
        QChar c = (char)((n & mask) >> (8 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}

QByteArray toQByteArray(quint32 n)
{
    QByteArray a;
    quint32 mask = 0xff << 24;
    for (int i = 0; i < 32; i += 8)
    {
        QChar c = (char)((n & mask) >> (24 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}

QByteArray toQByteArray(quint64 n)
{
    QByteArray a;
    //n = 0xff00ff00ff00ff00;
    quint64 mask = 0xff;
    mask <<= 56;
    for (int i = 0; i < 64; i += 8)
    {
        QChar c = (char)((n & mask) >> (56 - i));
        mask >>= 8;
        a.append(c);
    }
    return a;
}


QByteArray quint16ToByteArray(quint16 num)
{
	//Convert quint16 to QByteArray
    quint16 numBS = num;
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(num);
#endif
    QByteArray numBA((const char *)&numBS, 2);

	//QByteArray numBA;
	//QDataStream ds(&numBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (quint16)num;
	return numBA;
}

QByteArray quint8ToByteArray(quint8 num)
{
	//Convert quint8 to QByteArray
    quint8 numBS = num;
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(num);
#endif
    QByteArray numBA((const char *)&numBS, 1);

	//QByteArray numBA;
	//QDataStream ds(&numBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (quint8)num;
	return numBA;
}

QByteArray qint16ToByteArray(qint16 num)
{
	//Convert qint16 to QByteArray
    qint16 numBS = num;
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(num);
#endif
    QByteArray numBA((const char *)&numBS, 2);

	//QByteArray numBA;
	//QDataStream ds(&numBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (qint16)num;
	return numBA;
}

QByteArray quint32ToByteArray(quint32 num)
{
	//Convert quint32 to QByteArray
    quint32 numBS = num;
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(num);
#endif
    QByteArray numBA((const char *)&numBS, 4);
    
    //QByteArray numBA;
    //QDataStream ds(&numBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (quint32)num;

	return numBA;
}

QByteArray quint64ToByteArray(quint64 num)
{
	//Convert quint64 to QByteArray
    quint64 numBS = num;
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(num);
#endif
    QByteArray numBA((const char *)&numBS, 8);
	
    //QByteArray numBA;
	//QDataStream ds(&numBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (quint64)num;
	return numBA;
}

QByteArray stringToByteArray(QString str)
{
	//Return a n byte QByteArray with str and its size
	
    quint16 size = str.size();
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    size = qbswap(size);
#endif
    QByteArray numBA((const char *)&size, 2);
    numBA.append(str);

	//QDataStream ds(&strBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds << (quint16)str8bitBA.size();
    //strBA.append(str8bitBA);

	return numBA;
}

QByteArray sizeOfByteArray(QByteArray *data)
{
	//Return a 2 byte QByteArray with data's size
	qint16 numBS = data->size();
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    numBS = qbswap(numBS);
#endif
    QByteArray numBA((const char *)&numBS, 2);
    
    //QByteArray sizeBA;
	//QDataStream ds(&sizeBA,QIODevice::WriteOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//quint16 size = data->size();
	//ds << (quint16)size;
	//QMessageBox::information(this,tr("sizeOfByteArray"), tr("Size of ByteArray: %1\nsize: %2").arg(sizeBA.size()).arg(size));
	//return sizeBA;
    return numBA;
}

QString getStringFromByteArray(QByteArray *data)
{
	//Remove a string from a QByteArray

    quint16 num;
	memcpy(&num, data->constData(), 2);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
	data->remove(0,2);

    QString str = data->left(num).constData();

    data->remove(0, num);
    return str;

	//quint16 size;
	//QDataStream ds(data,QIODevice::ReadOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds >> size;
	//data->remove(0,2);
	//QString str = data->left(size).data();
	//data->remove(0,size);
	//return str;
}

quint16 getQuint16FromByteArray(QByteArray *data)
{
	//Remove a quint16 from a QByteArray
	quint16 num;
	//QDataStream ds(data,QIODevice::ReadOnly);
    //ds.setVersion(QDataStream::Qt_4_6);
    //ds >> num;
    memcpy(&num, data->constData(), 2);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
	data->remove(0,2);
	return num;
}

quint8 getQuint8FromByteArray(QByteArray *data)
{
	//Remove a quint8 from a QByteArray
	quint8 num;
	//QDataStream ds(data,QIODevice::ReadOnly);
    //ds.setVersion(QDataStream::Qt_4_6);
    //ds >> num;
    memcpy(&num, data->constData(), 1);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
	data->remove(0,1);
	return num;
}

quint32 getQuint32FromByteArray(QByteArray *data)
{
    //Remove a quint32 from a QByteArray
    quint32 num;
    //QDataStream ds(data,QIODevice::ReadOnly);
    //ds.setVersion(QDataStream::Qt_4_6);
    //ds >> num;
    memcpy(&num, data->constData(), 4);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
    
    data->remove(0,4);
    return num;
}

quint64 getQuint64FromByteArray(QByteArray *data)
{
	//Remove a quint64 from a QByteArray
	quint64 num;
	//QDataStream ds(data,QIODevice::ReadOnly);
	//ds.setVersion(QDataStream::Qt_4_6);
	//ds >> num;
    memcpy(&num, data->constData(), 8);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
	data->remove(0,8);
	return num;
}

qint16 getQint16FromByteArray(QByteArray *data)
{
	//Remove a qint16 from a QByteArray
	qint16 num;
	//QDataStream ds(data,QIODevice::ReadOnly);
    //ds.setVersion(QDataStream::Qt_4_6);
    //ds >> num;
    memcpy(&num, data->constData(), 2);
#ifdef Q_LITTLE_ENDIAN
    //Convert to BigEndian
    num = qbswap(num);
#endif
	data->remove(0,2);
	return num;
}
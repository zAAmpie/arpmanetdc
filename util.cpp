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


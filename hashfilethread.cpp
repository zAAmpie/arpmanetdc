#include "hashfilethread.h"
#include <QtGui>
#include "arpmanetdc.h"

//Constructor
HashFileThread::HashFileThread(ReturnEncoding encoding, QObject *parent) : QObject(parent)
{
    pEncoding = encoding;
    pStopHashing = false;

    hashTimer = new QTimer(this);
    hashTimer->setInterval(1000); //Every second
    connect(hashTimer, SIGNAL(timeout()), this, SLOT(hashTimerTimeout()));
    hashTimer->start();

    pBytesHashedThisSecond = 0;
}

//Destructor
HashFileThread::~HashFileThread()
{
    
}

//Main exec function
void HashFileThread::processFile(QString filePath, QString rootDir)
{
    pFileProgressBytes = 0;

    //Get file info
    QFileInfo fi(filePath);
    qint64 fileSize = fi.size();
    QString fileName = fi.fileName();
    QString modifiedDate = fi.lastModified().toString("dd-MM-yyyy HH:mm:ss:zzz");

    pCurrentFileSize = fileSize;

    //Start hashing
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly))
    {
        Tiger totalTTH;
    
        QString tthRoot;
        QList<QString> *oneMBTTHList = new QList<QString>();

        while (!file.atEnd())
        {
            //Stop hashing if variable is set
            if (pStopHashing)
            {
                pStopHashing = false;
                file.close();
                return;
             }
            else
                //Process events to allow variable to be set
                QApplication::processEvents();
            
            while (pBytesHashedThisSecond >= ArpmanetDC::settingsManager()->getSetting(SettingsManager::MAX_HASH_SPEED_MB)*1048576)
            {
                ((ExecThread *)thread())->msleep(10);

                //Process events to allow variable to be set
                QApplication::processEvents();
            }

            //Read file in 1MB chunks
            QByteArray iochunk = file.read(1*1048576);

            pBytesHashedThisSecond += iochunk.size();
            pFileProgressBytes += iochunk.size();

            //Add to total TTH
            totalTTH.Update((byte *)iochunk.data(), iochunk.size());

            while (!iochunk.isEmpty())
            {
                Tiger oneMBTTH;

                //Read 1MB chunk
                QByteArray chunk = iochunk.left(1048576);
                iochunk.remove(0, 1048576);

                //Add to 1MB TTH
                oneMBTTH.Update((byte *)chunk.data(), chunk.size());

                //Calculate 1MB TTH root
                byte *oneMBDigestTTH = new byte[oneMBTTH.DigestSize()];
                oneMBTTH.Final(oneMBDigestTTH);
                
                //Append 1MB TTH to list
                if (pEncoding == Base64Encoded)
                    oneMBTTHList->append(QString(QByteArray((char *)oneMBDigestTTH, oneMBTTH.DigestSize()).toBase64()).toUtf8()); //Base64
                else if (pEncoding == BinaryEncoded)
                    oneMBTTHList->append(QString(QByteArray((char *)oneMBDigestTTH, oneMBTTH.DigestSize())).toUtf8()); //8-bit
                else if (pEncoding == Base32Encoded)
                    oneMBTTHList->append(base32Encode(oneMBDigestTTH, oneMBTTH.DigestSize())); //Base32
                    
                delete oneMBDigestTTH;
            }
        }

        byte *digestTTH = new byte[totalTTH.DigestSize()];
        totalTTH.Final(digestTTH);

        if (pEncoding == Base64Encoded)
            tthRoot = QString(QByteArray((char *)digestTTH, totalTTH.DigestSize()).toBase64()).toUtf8(); //Base64
        else if (pEncoding == BinaryEncoded)
            tthRoot = QString(QByteArray((char *)digestTTH, totalTTH.DigestSize())).toUtf8(); //8-bit
        else if (pEncoding == Base32Encoded)
            tthRoot = base32Encode(digestTTH, totalTTH.DigestSize()); //Base32
        
        delete digestTTH;

        file.close();

        //Done hashing the file
        emit done(filePath, fileName, fileSize, tthRoot, rootDir, modifiedDate, oneMBTTHList, this);
    }
    else
        //Could not open file
        emit failed(filePath, this);    
}

//Hashes a bucket
void HashFileThread::processBucket(QByteArray rootTTH, int bucketNumber, QByteArray bucket, ReturnEncoding encoding, QHostAddress peer)
{
    Tiger totalTTH;
    QByteArray tth;
    
    //Calculate TTH of bucket - can be any size
    totalTTH.Update((byte *)bucket.data(), bucket.size());

    byte *digestTTH = new byte[totalTTH.DigestSize()];
    totalTTH.Final(digestTTH);

    if (encoding == Base64Encoded)
        tth = QByteArray((char *)digestTTH, totalTTH.DigestSize()).toBase64(); //Base64
    else if (encoding == BinaryEncoded)
        tth = QByteArray((char *)digestTTH, totalTTH.DigestSize()); //8-bit
    else if (encoding == Base32Encoded)
        tth = base32Encode(digestTTH, totalTTH.DigestSize()); //Base32

    //Done hashing the bucket
    emit doneBucket(rootTTH, bucketNumber, tth, peer);
}

void HashFileThread::hashFile(quint8 type, QString filePath)
{
    QFileInfo fi(filePath);
    quint64 fileSize = fi.size();

    //Start hashing
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly))
    {
        Tiger totalTTH;
    
        QByteArray tthRoot;
        
        while (!file.atEnd())
        {
            //Read file in 1MB chunks
            QByteArray iochunk = file.read(1*1048576);

            //Add to total TTH
            totalTTH.Update((byte *)iochunk.data(), iochunk.size());
        }

        byte *digestTTH = new byte[totalTTH.DigestSize()];
        totalTTH.Final(digestTTH);

        tthRoot = QByteArray((char *)digestTTH, totalTTH.DigestSize());
        
        delete digestTTH;

        file.close();

        //Done hashing the file
        emit doneFile(type, filePath, tthRoot, fileSize);
    }
    else
        //Could not open file - return null
        emit doneFile(type, filePath, QByteArray(), fileSize);
}

void HashFileThread::stopHashing(bool value)
{
    pStopHashing = value;
}

void HashFileThread::hashTimerTimeout()
{
    //Emit hashing progress
    emit hashingProgress(pBytesHashedThisSecond, pFileProgressBytes, pCurrentFileSize);

    //Reset amount
    pBytesHashedThisSecond = 0;
}

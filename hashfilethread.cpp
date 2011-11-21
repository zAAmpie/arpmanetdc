#include "hashfilethread.h"


//Constructor
HashFileThread::HashFileThread(ReturnEncoding encoding, QObject *parent) : QObject(parent)
{
	pEncoding = encoding;
}

//Destructor
HashFileThread::~HashFileThread()
{
	
}

//Main exec function
void HashFileThread::processFile(QString filePath, QString rootDir)
{
	//Get file info
	QFileInfo fi(filePath);
	qint64 fileSize = fi.size();
	QString fileName = fi.fileName();
	QString modifiedDate = fi.lastModified().toString("dd-MM-yyyy HH:mm:ss:zzz");

	//Start hashing
	QFile file(filePath);
	
	if (file.open(QIODevice::ReadOnly))
	{
		Tiger totalTTH;
	
		QString tthRoot;
		QList<QString> *oneMBTTHList = new QList<QString>();

		while (!file.atEnd())
		{
			//Read file in 1MB chunks
			QByteArray iochunk = file.read(1*1048576);

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

/*
QString HashFileThread::base32Encode(byte *input, int inputLength)
{
	int encodedLength = Base32::GetEncode32Length(inputLength);
	byte *b32text = new byte[encodedLength];
	if (!Base32::Encode32(input, inputLength, b32text))
	{
		return "";
	}

	char *alpha = new char[32];
	memcpy(alpha, "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", 32);
	Base32::Map32(b32text, encodedLength, (byte *)alpha);

	QString output = QByteArray((const char*)b32text, encodedLength);

	delete [] b32text;
	delete [] alpha;
	return output;
}*/

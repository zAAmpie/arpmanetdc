/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UPLOADTRANSFER_H
#define UPLOADTRANSFER_H
#include "transfer.h"
#include "fstptransfersegment.h"
#include "protocoldef.h"

#define TIMER_INACTIVITY_MSECS 60000

class UploadTransfer : public Transfer
{
    Q_OBJECT
public:
    UploadTransfer(QObject *parent = 0);
    ~UploadTransfer();
    void setFileName(QString filename);
    void setTTH(QByteArray tth);
    void createUploadObject(quint8 protocol);

public slots:
    int getTransferProgress();

private slots:
    void dataTransmitted(QHostAddress host, QByteArray *data);

private:
    QTimer* transferInactivityTimer;
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    quint64 bytesWrittenSinceUpdate;

    TransferSegment *upload;
};

#endif // UPLOADTRANSFER_H

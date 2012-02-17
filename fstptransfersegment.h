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

#ifndef FSTPTRANSFERSEGMENT_H
#define FSTPTRANSFERSEGMENT_H
#include "transfersegment.h"

#define FSTP_TRANSFER_MINIMUM_SEGMENT 65536
#define FSTP_TRANSFER_MAXIMUM_SEGMENT 1048576//524288

class Transfer;

class FSTPTransferSegment : public TransferSegment
{
public:
    FSTPTransferSegment(Transfer *parent = 0);
    ~FSTPTransferSegment();

public slots:
    void incomingDataPacket(quint64 offset, QByteArray data);
    void transferTimerEvent();
    void setFileName(QString filename);
    void setFileSize(quint64 size);
    void startUploading();
    void startDownloading();
    void pauseDownload();
    void unpauseDownload();
    qint64 getBytesReceivedNotFlushed();

private:
    quint64 requestingOffset;
    quint64 requestingLength;
    quint64 requestingTargetOffset;
    int packetsSinceUpdate;
    int retransmitTimeoutCounter;
    int retransmitRetryCounter;
};

#endif // FSTPTRANSFERSEGMENT_H

#include "utptransfersegment.h"

uTPTransferSegment::uTPTransferSegment(Transfer *parent)
{
    pParent = parent;
#ifdef Q_WS_WIN
    BYTE byMajorVersion = 2, byMinorVersion = 2;
    int result = WSAStartup(MAKEWORD(byMajorVersion, byMinorVersion), &wsa);
    if (result != 0 || LOBYTE(wsa.wVersion) != byMajorVersion || HIBYTE(wsa.wVersion) != byMinorVersion )
    {
        if (result == 0)
            WSACleanup();
    }
#endif
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(0);
    sock = make_socket((const struct sockaddr*)&sin, sizeof(sin));
    //s.s = UTP_Create(a,c,(const struct sockaddr*)&sin, sizeof(sin));

    UTP_SetSockopt(s.s, SO_SNDBUF, 100*300);
    s.state = 0;

    /*UTPFunctionTable utp_callbacks = {
        (void (*))&uTPTransferSegment::utp_read,
        &uTPTransferSegment::utp_write,
        &uTPTransferSegment::utp_get_rb_size,
        &uTPTransferSegment::utp_state,
        &uTPTransferSegment::utp_error,
        &uTPTransferSegment::utp_overhead
    };
    UTP_SetCallbacks(s.s, &utp_callbacks, &s);*/
}

uTPTransferSegment::~uTPTransferSegment()
{
    if (inputFile.isOpen())
        inputFile.close();
}

void uTPTransferSegment::transferTimerEvent(){}

void uTPTransferSegment::incomingDataPacket(quint64 offset, QByteArray data)
{
    // suggestion: since this thing uses its own socket, we can trigger an upload by sending the leeching side's
    // host address and listening port as a data packet, parse it here and reverse connect.
}

void uTPTransferSegment::setFileName(QString filename)
{
    filePathName = filename;
    inputFile.setFileName(filePathName);
    inputFile.open(QIODevice::ReadOnly);
    fileSize = inputFile.size();
}

void uTPTransferSegment::setFileSize(quint64 size)
{
    fileSize = size;
}

void uTPTransferSegment::startUploading()
{

}

void uTPTransferSegment::startDownloading()
{

}

qint64 uTPTransferSegment::getBytesReceivedNotFlushed()
{
    return 0;
}

// Convenience wrapper to create a socket for use with uTP, from utp_test
SOCKET uTPTransferSegment::make_socket(const struct sockaddr *addr, socklen_t addrlen)
{
    SOCKET s = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return s;

    if (bind(s, addr, addrlen) < 0) {
        char str[20];
        printf("UDP port bind failed %s: (%d) %s\n",
               inet_ntop(addr->sa_family, (sockaddr*)addr, str, sizeof(str)), errno, strerror(errno));
        closesocket(s);
        return INVALID_SOCKET;
    }

    // Mark to hold a couple of megabytes
    int size = 2 * 1024 * 1024;

    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (CSOCKOPTP)&size, sizeof(size)) < 0) {
        printf("UDP setsockopt(SO_RCVBUF, %d) failed: %d %s\n", size, errno, strerror(errno));
    }
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (CSOCKOPTP)&size, sizeof(size)) < 0) {
        printf("UDP setsockopt(SO_SNDBUF, %d) failed: %d %s\n", size, errno, strerror(errno));
    }

    // make socket non blocking
#ifdef _WIN32
    u_long b = 1;
    ioctlsocket(s, FIONBIO, &b);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    return s;
}

// --------------============= uTP callbacks =============--------------
// count bytes arrived over uTP connection
void uTPTransferSegment::utp_read(void* socket, const byte* bytes, size_t count)
{
    //TODO: read count bytes from bytes and tip them into the buckets
    // while manually keeping track of the offset
}

// phone in to get next count bytes to write
void uTPTransferSegment::utp_write(void* socket, byte* bytes, size_t count)
{
    // TODO: put count bytes into bytes, keep track of offset
    socket_state* s = (socket_state*)socket;
    s->total_sent += count;
}

// get receive buffer length?
size_t uTPTransferSegment::utp_get_rb_size(void* socket)
{
    return 0;
}

// phone state changes in
void uTPTransferSegment::utp_state(void* socket, int state)
{
    socket_state* s = (socket_state*)socket;
    s->state = state;
}

// phone errors in
void uTPTransferSegment::utp_error(void* socket, int)
{
    socket_state* s = (socket_state*)socket;
    if (s->s)
    {
        UTP_Close(s->s);
        s->s = NULL;
    }
}

void uTPTransferSegment::utp_overhead(void *socket, bool send, size_t count, int type)
{
}

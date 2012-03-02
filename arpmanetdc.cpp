#include "arpmanetdc.h"
#ifdef Q_OS_WIN
#include "Windows.h"
#endif

SettingsManager* ArpmanetDC::settingsManager()
{
    return pSettingsManager;
}

SettingsManager* ArpmanetDC::pSettingsManager = new SettingsManager();

ArpmanetDC::ArpmanetDC(QStringList arguments, QWidget *parent, Qt::WFlags flags)
    : QMainWindow(parent, flags)
{
    //Set database pointer to zero at start
    db = 0;

    //Get database path
    //TODO: Check command line arguments for specified database path
    shareDatabasePath = DEFAULT_SHARE_DATABASE_PATH;
#ifdef Q_OS_LINUX
    if (!QFileInfo(shareDatabasePath).exists())
        shareDatabasePath = QDir::homePath() + "/.arpmanetdc.sqlite";
    qDebug() << shareDatabasePath;
#endif

    setupDatabase();
    //pSettings = new QHash<QString, QString>();

    //Load settings from database or initialize settings from defaults
    QString ipString = getIPGuess().toString();
    delete pSettingsManager;
    pSettingsManager = new SettingsManager(db, this);
    pSettingsManager->loadSettings();

    createdGUI = false;
    pArguments = arguments;
    pSharedMemory = new QSharedMemory(pSettingsManager->getSetting(SettingsManager::SHARED_MEMORY_KEY));
    QByteArray magnetArg;
    if (pArguments.size() >= 2)
        magnetArg.append(pArguments.at(1));
    else
        magnetArg.append(tr("DUPLICATE_INSTANCE"));

    //Try to attach to shared memory space
    if (pSharedMemory->attach())
    {
        //This means another instance is running
        pSharedMemory->lock();
        memcpy((char *)pSharedMemory->data(), magnetArg.constData(), qMin(magnetArg.size(), pSharedMemory->size()));
        pSharedMemory->unlock();

#ifdef Q_OS_LINUX
        pSharedMemory->detach();
#endif

        sqlite3_close(db);
        
        //Close this instance
        return;
    }
    else
    {
        //No other instance is running, create shared memory sector (1KiB max)
        if (!pSharedMemory->create(1024))
        {
            //Couldn't create shared memory space
            qDebug() << "ArpmanetDC::Constructor: Could not create shared memory space";
        }
        else
        {
            //Initialize shared memory
            pSharedMemory->lock();
            memset((char *)pSharedMemory->data(), '\0', pSharedMemory->size());
            pSharedMemory->unlock();
        }
    }

    //QApplication::setStyle(new QCleanlooksStyle());

    //Register QHostAddress for queueing over threads
    qRegisterMetaType<QHostAddress>("QHostAddress");
    qRegisterMetaType<QueueStruct>("QueueStruct");
    qRegisterMetaType<QueuePriority>("QueuePriority");
    qRegisterMetaType<FinishedDownloadStruct>("FinishedDownloadStruct");
    qRegisterMetaType<QDir>("QDir");
    qRegisterMetaType<QList<QHostAddress> >("QList<QHostAddress>");
    qRegisterMetaType<QHash<QString, ContainerContentsType> >("QHash<QString, ContainerContentsType>");
    qRegisterMetaType<QHash<QString, QStringList> >("QHash<QString, QStringList>");
    qRegisterMetaType<QHash<QString, QList<ContainerLookupReturnStruct> > >("QHash<QString, QList<ContainerLookupReturnStruct> >");
    qRegisterMetaType<ContainerContentsType>("ContainerContentsType");
    qRegisterMetaType<ContainerLookupReturnStruct>("ContainerLookupReturnStruct");
    qRegisterMetaType<QList<ContainerLookupReturnStruct> >("QList<ContainerLookupReturnStruct>");
    qRegisterMetaType<QList<TransferItemStatus> >("QList<TransferItemStatus>");
    qRegisterMetaType<QList<QDir> >("QList<QDir>");
    qRegisterMetaType<QByteArray>("QByteArray");
    qRegisterMetaType<QHash<QString, UserCommandStruct> >("QHash<QString, UserCommandStruct>");

    /*if (!pSettings->contains("nick"))
        pSettings->insert("nick", DEFAULT_NICK);
    if (!pSettings->contains("password"))
        pSettings->insert("password", DEFAULT_PASSWORD);
    if (!pSettings->contains("hubAddress"))
        pSettings->insert("hubAddress", DEFAULT_HUB_ADDRESS); 
    if (!pSettings->contains("hubPort"))
        pSettings->insert("hubPort", DEFAULT_HUB_PORT);
    if (!pSettings->contains("externalIP"))
        pSettings->insert("externalIP", ipString);
    if (!pSettings->contains("externalPort"))
        pSettings->insert("externalPort", DEFAULT_EXTERNAL_PORT);
    if (!pSettings->contains("downloadPath"))
        pSettings->insert("downloadPath", getDefaultDownloadPath());
    if (!pSettings->contains("showAdvanced"))
        pSettings->insert("showAdvanced", DEFAULT_SHOW_ADVANCED);
    if (!pSettings->contains("lastSeenIP"))
        pSettings->insert("lastSeenIP", ipString);
    if (!pSettings->contains("autoUpdateShareInterval"))
        pSettings->insert("autoUpdateShareInterval", DEFAULT_SHARE_UPDATE_INTERVAL);
    if (!pSettings->contains("lastDownloadToFolder"))
        pSettings->insert("lastDownloadToFolder", pSettings->value("downloadPath"));
    if (!pSettings->contains("protocolHint"))
    {
        QByteArray protocolHint;
        foreach (char val, PROTOCOL_MAP)
            protocolHint.append(val);
        pSettings->insert("protocolHint", protocolHint.data());
    }*/

    //Check current IP setting with previous setting
    if (pSettingsManager->getSetting(SettingsManager::LAST_SEEN_IP) != ipString)
    {
        if (QMessageBox::question(this, tr("ArpmanetDC v%1").arg(VERSION_STRING), tr("<p>IP has changed from last value seen.</p>"
            "<p><table border=1 cellpadding=3 cellspacing=0><tr><td><b>Name</b></td><td><b>IP</b></td></tr><tr><td width=\"200\">Current</td><td>%1</td></tr>"
            "<tr><td width=\"200\">Previously seen</td><td>%2</td></tr><tr><td width=\"200\">External as set in Settings</td><td>%3</td></tr></table></p>"
            "<p>Do you want to use the current IP?</p>").arg(ipString).arg(pSettingsManager->getSetting(SettingsManager::LAST_SEEN_IP)).arg(pSettingsManager->getSetting(SettingsManager::EXTERNAL_IP)),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
        {
            pSettingsManager->setSetting(SettingsManager::EXTERNAL_IP, ipString);
            //pSettings->insert("externalIP", ipString);
        }
    }

    //Save new IP
    pSettingsManager->setSetting(SettingsManager::LAST_SEEN_IP, ipString);
    //pSettings->insert("lastSeenIP", ipString);
    
    mainChatBlocks = 0;
    pFilesHashedSinceUpdate = 0;
    pFileSizeHashedSinceUpdate = 0;

    //Set window title
    setWindowTitle(tr("ArpmanetDC v%1").arg(VERSION_STRING));

    //Create Hub Connection
    pHub = new HubConnection(pSettingsManager->getSetting(SettingsManager::HUB_ADDRESS), pSettingsManager->getSetting(SettingsManager::HUB_PORT), pSettingsManager->getSetting(SettingsManager::NICKNAME), pSettingsManager->getSetting(SettingsManager::PASSWORD), VERSION_STRING, this);

    //Connect HubConnection to GUI
    connect(pHub, SIGNAL(receivedChatMessage(QString)), this, SLOT(appendChatLine(QString)));
    connect(pHub, SIGNAL(receivedMyINFO(QString, QString, QString, QString, QString, QString, quint16, quint64)), 
        this, SLOT(userListInfoReceived(QString, QString, QString, QString, QString, QString, quint16, quint64)));
    connect(pHub, SIGNAL(receivedNickList(QStringList)), this, SLOT(userListNickListReceived(QStringList)));
    connect(pHub, SIGNAL(receivedOpList(QStringList)), this, SLOT(opListReceived(QStringList)));
    connect(pHub, SIGNAL(userLoggedOut(QString)), this, SLOT(userListUserLoggedOut(QString)));    
    connect(pHub, SIGNAL(receivedPrivateMessage(QString, QString)), this, SLOT(receivedPrivateMessage(QString, QString)));
    connect(pHub, SIGNAL(hubOnline()), this, SLOT(hubOnline()));
    connect(pHub, SIGNAL(hubOffline()), this, SLOT(hubOffline()));

    //pHub->connectHub();

    //Create Dispatcher connection
    dispatcherThread = new ExecThread();
    pDispatcher = new Dispatcher(QHostAddress(pSettingsManager->getSetting(SettingsManager::EXTERNAL_IP)), pSettingsManager->getSetting(SettingsManager::EXTERNAL_PORT));

    // conjure up something unique here and save it for every subsequent client invocation
    //Maybe make a SHA1 hash of the Nick + Password - unique and consistent? Except if you have two clients open with the same login details? Meh
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray().append(pSettingsManager->getSetting(SettingsManager::NICKNAME)));
    hash.addData(QByteArray().append(pSettingsManager->getSetting(SettingsManager::PASSWORD)));
    QByteArray cid = hash.result();

    //QByteArray cid = "012345678901234567890123";
    pDispatcher->setCID(cid);

    // Tell Dispatcher what protocols we support from a nice and central place
    pDispatcher->setProtocolCapabilityBitmask(FailsafeTransferProtocol);
    //pDispatcher->setProtocolCapabilityBitmask(FailsafeTransferProtocol | uTPProtocol);
    //pDispatcher->setProtocolCapabilityBitmask(uTPProtocol);

    //Connect Dispatcher to GUI - handle search replies from other clients
    connect(pDispatcher, SIGNAL(bootstrapStatusChanged(int)), this, SLOT(bootstrapStatusChanged(int)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(searchResultsReceived(QHostAddress, QByteArray, quint64, QByteArray)),
            this, SLOT(searchResultReceived(QHostAddress, QByteArray, quint64, QByteArray)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(appendChatLine(QString)), this, SLOT(appendChatLine(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(getHostCount()), pDispatcher, SLOT(getHostCount()), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(returnHostCount(int)), this, SLOT(returnHostCount(int)), Qt::QueuedConnection);
    connect(this, SIGNAL(initiateSearch(quint64, QByteArray)), pDispatcher, SLOT(initiateSearch(quint64, QByteArray)), Qt::QueuedConnection);

    // Create Transfer manager
    transferThread = new ExecThread();
    pTransferManager = new TransferManager();
    pTransferManager->setMaximumSimultaneousDownloads(pSettingsManager->getSetting(SettingsManager::MAX_SIMULTANEOUS_DOWNLOADS));
    pTransferManager->setMaximumSimultaneousUploads(pSettingsManager->getSetting(SettingsManager::MAX_SIMULTANEOUS_UPLOADS));
    pTransferManager->setProtocolOrderPreference(pSettingsManager->getSetting(SettingsManager::PROTOCOL_HINT).toAscii());

    //Connect Dispatcher to TransferManager - handles upload/download requests and transfers
    connect(pDispatcher, SIGNAL(incomingUploadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            pTransferManager, SLOT(incomingUploadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(incomingDataPacket(quint8,QHostAddress,QByteArray)),
            pTransferManager, SLOT(incomingDataPacket(quint8,QHostAddress,QByteArray)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(incomingDirectDataPacket(quint32,qint64,QByteArray)),
            pTransferManager, SLOT(incomingDirectDataPacket(quint32,qint64,QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)),
            pDispatcher, SLOT(sendUnicastRawDatagram(QHostAddress,QByteArray*)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(receivedTTHTree(QByteArray,QByteArray)),
            pTransferManager, SLOT(incomingTTHTree(QByteArray,QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)),
            pDispatcher, SLOT(sendTTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(TTHSearchResultsReceived(QByteArray,QHostAddress)),
            pTransferManager, SLOT(incomingTTHSource(QByteArray,QHostAddress)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(searchTTHAlternateSources(QByteArray)),
            pDispatcher, SLOT(initiateTTHSearch(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            pDispatcher, SLOT(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(incomingProtocolCapabilityResponse(QHostAddress,char)),
            pTransferManager, SLOT(incomingProtocolCapabilityResponse(QHostAddress,char)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(requestProtocolCapability(QHostAddress)),
            pDispatcher, SLOT(sendProtocolCapabilityQuery(QHostAddress)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(incomingTransferError(QHostAddress,QByteArray,qint64,quint8)),
            pTransferManager, SLOT(incomingTransferError(QHostAddress,QByteArray,qint64,quint8)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,qint64)),
            pDispatcher, SLOT(sendTransferError(QHostAddress,quint8,QByteArray,qint64)), Qt::QueuedConnection);
    /*connect(pDispatcher, SIGNAL(incomingUploadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            pTransferManager, SLOT(incomingUploadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)));
    connect(pDispatcher, SIGNAL(incomingDataPacket(quint8,QHostAddress,QByteArray)),
            pTransferManager, SLOT(incomingDataPacket(quint8,QHostAddress,QByteArray)));
    connect(pDispatcher, SIGNAL(incomingDirectDataPacket(quint32,qint64,QByteArray)),
            pTransferManager, SLOT(incomingDirectDataPacket(quint32,qint64,QByteArray)));
    connect(pTransferManager, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)),
            pDispatcher, SLOT(sendUnicastRawDatagram(QHostAddress,QByteArray*)));
    connect(pDispatcher, SIGNAL(receivedTTHTree(QByteArray,QByteArray)),
            pTransferManager, SLOT(incomingTTHTree(QByteArray,QByteArray)));
    connect(pTransferManager, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)),
            pDispatcher, SLOT(sendTTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)));
    connect(pDispatcher, SIGNAL(TTHSearchResultsReceived(QByteArray,QHostAddress)),
            pTransferManager, SLOT(incomingTTHSource(QByteArray,QHostAddress)));
    connect(pTransferManager, SIGNAL(searchTTHAlternateSources(QByteArray)),
            pDispatcher, SLOT(initiateTTHSearch(QByteArray)));
    connect(pTransferManager, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)),
            pDispatcher, SLOT(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32)));
    connect(pDispatcher, SIGNAL(incomingProtocolCapabilityResponse(QHostAddress,char)),
            pTransferManager, SLOT(incomingProtocolCapabilityResponse(QHostAddress,char)));
    connect(pTransferManager, SIGNAL(requestProtocolCapability(QHostAddress)),
            pDispatcher, SLOT(sendProtocolCapabilityQuery(QHostAddress)));
    connect(pDispatcher, SIGNAL(incomingTransferError(QHostAddress,QByteArray,qint64,quint8)),
            pTransferManager, SLOT(incomingTransferError(QHostAddress,QByteArray,qint64,quint8)));
    connect(pTransferManager, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,qint64)),
            pDispatcher, SLOT(sendTransferError(QHostAddress,quint8,QByteArray,qint64)));*/

    //Connect TransferManager to GUI - notify of started/completed transfers
    connect(pTransferManager, SIGNAL(downloadStarted(QByteArray)), 
            this, SLOT(downloadStarted(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(downloadCompleted(QByteArray)), 
            this, SLOT(downloadCompleted(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(requeueDownload(QByteArray)),
            this, SLOT(requeueDownload(QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(removeQueuedDownload(int, QByteArray)), 
            pTransferManager, SLOT(removeQueuedDownload(int, QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), 
            pTransferManager, SLOT(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), Qt::QueuedConnection);
    connect(this, SIGNAL(changeQueuedDownloadPriority(int, int, QByteArray)),
            pTransferManager, SLOT(changeQueuedDownloadPriority(int, int, QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(stopTransfer(QByteArray, int, QHostAddress)),
            pTransferManager, SLOT(stopTransfer(QByteArray, int, QHostAddress)), Qt::QueuedConnection);
    connect(this, SIGNAL(closeClientEvent()),
            pTransferManager, SLOT(closeClientEvent()), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(closeClientEventReturn()),
            this, SLOT(closeClientEventReturn()), Qt::QueuedConnection);

    // Set network scan ranges in Dispatcher, initial shotgun approach
    //pDispatcher->addNetworkScanRange(QHostAddress("143.160.0.1").toIPv4Address(), 65534);
    //pDispatcher->addNetworkScanRange(QHostAddress("10.22.4.1").toIPv4Address(), 254);
    //pDispatcher->addNetworkScanRange(QHostAddress("192.168.0.2").toIPv4Address(), 4);
    //pDispatcher->addNetworkScanRange(QHostAddress("172.31.0.1").toIPv4Address(), 65534);
    // Changed to accept begin and end host addresses, calculate range internally.
    // Perhaps we should later read these from config and manage them somewhere decent.
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.9.1").toIPv4Address(), QHostAddress("143.160.11.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.13.1").toIPv4Address(), QHostAddress("143.160.15.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.17.1").toIPv4Address(), QHostAddress("143.160.19.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.21.1").toIPv4Address(), QHostAddress("143.160.23.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.25.1").toIPv4Address(), QHostAddress("143.160.27.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.29.1").toIPv4Address(), QHostAddress("143.160.31.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.41.1").toIPv4Address(), QHostAddress("143.160.43.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.45.1").toIPv4Address(), QHostAddress("143.160.47.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.57.1").toIPv4Address(), QHostAddress("143.160.59.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.61.1").toIPv4Address(), QHostAddress("143.160.63.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.69.1").toIPv4Address(), QHostAddress("143.160.71.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.77.1").toIPv4Address(), QHostAddress("143.160.79.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.93.1").toIPv4Address(), QHostAddress("143.160.95.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.109.1").toIPv4Address(), QHostAddress("143.160.111.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.125.1").toIPv4Address(), QHostAddress("143.160.127.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.133.1").toIPv4Address(), QHostAddress("143.160.175.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.65.1").toIPv4Address(), QHostAddress("172.31.65.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.94.1").toIPv4Address(), QHostAddress("172.31.95.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.113.1").toIPv4Address(), QHostAddress("172.31.114.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.157.1").toIPv4Address(), QHostAddress("172.31.162.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.164.1").toIPv4Address(), QHostAddress("172.31.164.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.180.1").toIPv4Address(), QHostAddress("172.31.180.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.187.1").toIPv4Address(), QHostAddress("172.31.189.254").toIPv4Address());
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.191.1").toIPv4Address(), QHostAddress("172.31.191.254").toIPv4Address());

    //Set up thread for database / ShareSearch
    dbThread = new ExecThread();
    pShare = new ShareSearch(pSettingsManager->getSetting(SettingsManager::MAX_SEARCH_RESULTS), this);

    //Connect ShareSearch to GUI - share files on this computer and hash them
    connect(pShare, SIGNAL(fileHashed(QString, quint64, quint64)), this, SLOT(fileHashed(QString, quint64, quint64)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(directoryParsed(QString)), this, SLOT(directoryParsed(QString)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(hashingDone(int, int)), this, SLOT(hashingDone(int, int)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(parsingDone(int)), this, SLOT(parsingDone(int)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnQueueList(QHash<QByteArray, QueueStruct> *)), this, SLOT(returnQueueList(QHash<QByteArray, QueueStruct> *)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnFinishedList(QHash<QByteArray, FinishedDownloadStruct> *)), this, SLOT(returnFinishedList(QHash<QByteArray, FinishedDownloadStruct> *)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(searchWordListReceived(QStandardItemModel *)), this, SLOT(searchWordListReceived(QStandardItemModel *)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnTotalShare(quint64)), this, SLOT(returnTotalShare(quint64)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnUserCommands(QHash<QString, UserCommandStruct>*)), this, SLOT(returnUserCommands(QHash<QString, UserCommandStruct>*)), Qt::QueuedConnection);
    connect(this, SIGNAL(updateShares()), pShare, SLOT(updateShares()), Qt::QueuedConnection);
    connect(this, SIGNAL(requestQueueList()), pShare, SLOT(requestQueueList()), Qt::QueuedConnection);
    connect(this, SIGNAL(setQueuedDownloadPriority(QByteArray, QueuePriority)), pShare, SLOT(setQueuedDownloadPriority(QByteArray, QueuePriority)), Qt::QueuedConnection);
    connect(this, SIGNAL(removeQueuedDownload(QByteArray)), pShare, SLOT(removeQueuedDownload(QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveQueuedDownload(QueueStruct)), pShare, SLOT(saveQueuedDownload(QueueStruct)), Qt::QueuedConnection);
    connect(this, SIGNAL(requestFinishedList()), pShare, SLOT(requestFinishedList()), Qt::QueuedConnection);
    connect(this, SIGNAL(saveFinishedDownload(FinishedDownloadStruct)), pShare, SLOT(saveFinishedDownload(FinishedDownloadStruct)), Qt::QueuedConnection);
    connect(this, SIGNAL(removeFinishedDownload(QByteArray)), pShare, SLOT(removeFinishedDownload(QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(clearFinishedDownloads()), pShare, SLOT(clearFinishedDownloads()), Qt::QueuedConnection);
    connect(this, SIGNAL(requestAutoCompleteWordList(QStandardItemModel *)), pShare, SLOT(requestAutoCompleteWordList(QStandardItemModel *)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveAutoCompleteWordList(QString)), pShare, SLOT(saveAutoCompleteWordList(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveContainers(QHash<QString, ContainerContentsType>, QString)), pShare, SIGNAL(saveContainers(QHash<QString, ContainerContentsType>, QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(processContainer(QHostAddress, QString, QString)), pShare, SLOT(processContainer(QHostAddress, QString, QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(requestTotalShare(bool)), pShare, SLOT(requestTotalShare(bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(requestUserCommands()), pShare, SLOT(requestUserCommands()), Qt::QueuedConnection);
    connect(this, SIGNAL(deleteBucketFlushStateBitmap(QByteArray)), pShare, SLOT(deleteBucketFlushStateBitmap(QByteArray)), Qt::QueuedConnection);
   
    //Connect ShareSearch to Dispatcher - reply to search request from other clients
    connect(pShare, SIGNAL(returnSearchResult(QHostAddress, QByteArray, quint64, QByteArray)), 
            pDispatcher, SLOT(sendSearchResult(QHostAddress, QByteArray, quint64, QByteArray)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(searchQuestionReceived(QHostAddress, QByteArray, quint64, QByteArray)), 
            pShare, SLOT(querySearchString(QHostAddress, QByteArray, quint64, QByteArray)), Qt::QueuedConnection);
    
    //TTH searches and trees
    connect(pDispatcher, SIGNAL(TTHSearchQuestionReceived(QByteArray,QHostAddress)),
            pShare, SLOT(TTHSearchQuestionReceived(QByteArray, QHostAddress)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(sendTTHSearchResult(QHostAddress, QByteArray)),
            pDispatcher, SLOT(sendTTHSearchResult(QHostAddress,QByteArray)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(incomingTTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)),
            pShare, SLOT(incomingTTHTreeRequest(QHostAddress, QByteArray,quint32,quint32)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(sendTTHTreeReply(QHostAddress, QByteArray)),
            pDispatcher, SLOT(sendTTHTreeReply(QHostAddress,QByteArray)), Qt::QueuedConnection);

    //Bootstrapped peers
    connect(pShare, SIGNAL(sendLastKnownPeers(QList<QHostAddress>)), 
            pDispatcher, SIGNAL(sendLastKnownPeers(QList<QHostAddress>)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(requestLastKnownPeers()),
            pShare, SLOT(requestLastKnownPeers()), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(saveLastKnownPeers(QList<QHostAddress>)),
            pShare, SLOT(saveLastKnownPeers(QList<QHostAddress>)), Qt::QueuedConnection);

    //Connect ShareSearch to TransferManager - loads and saves a set of sources to the database
    connect(pTransferManager, SIGNAL(filePathNameRequest(QByteArray)),
            pShare, SLOT(requestFilePath(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(saveTTHSourceToDatabase(QByteArray, QHostAddress)),
            pShare, SLOT(saveTTHSource(QByteArray, QHostAddress)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)),
            pShare, SLOT(loadTTHSource(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(deleteTTHSourcesFromDatabase(QByteArray)),
            pShare, SLOT(deleteTTHSources(QByteArray)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(filePathReply(QByteArray, QString, quint64)),
            pTransferManager, SLOT(filePathNameReply(QByteArray, QString, quint64)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(tthSourceLoaded(QByteArray, QHostAddress)),
            pTransferManager, SLOT(incomingTTHSource(QByteArray, QHostAddress)), Qt::QueuedConnection);

    connect(pShare, SIGNAL(restoreBucketFlushStateBitmap(QByteArray, QByteArray)),
        pTransferManager, SLOT(restoreBucketFlushStateBitmap(QByteArray, QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(saveBucketFlushStateBitmap(QByteArray, QByteArray)),
        pShare, SLOT(saveBucketFlushStateBitmap(QByteArray, QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(loadBucketFlushStateBitmap(QByteArray)),
        pShare, SLOT(loadBucketFlushStateBitmap(QByteArray)), Qt::QueuedConnection);

    connect(pTransferManager, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray)),
            pShare, SLOT(hashBucketRequest(QByteArray, int, QByteArray)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(hashBucketReply(QByteArray, int, QByteArray)),
            pTransferManager, SLOT(hashBucketReply(QByteArray,int,QByteArray)), Qt::QueuedConnection);

    //Temporary signal to search local database

    //connect(pShare, SIGNAL(returnSearchResult(QHostAddress,QByteArray,quint64,QByteArray)),
    //        this, SLOT(searchResultReceived(QHostAddress,QByteArray,quint64,QByteArray)));

    bucketFlushThread = new ExecThread();
    pBucketFlushThread = new BucketFlushThread();

    //Connect BucketFlushThread to TransferManager
    connect(pTransferManager, SIGNAL(flushBucket(QString,QByteArray*)),
            pBucketFlushThread, SLOT(flushBucket(QString,QByteArray*)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(assembleOutputFile(QString,QString,int,int)),
            pBucketFlushThread, SLOT(assembleOutputFile(QString,QString,int,int)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(flushBucketDirect(QString,int,QByteArray*,QByteArray)),
            pBucketFlushThread, SLOT(flushBucketDirect(QString,int,QByteArray*,QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(renameIncompleteFile(QString)),
            pBucketFlushThread, SLOT(renameIncompleteFile(QString)), Qt::QueuedConnection);
    connect(pBucketFlushThread, SIGNAL(bucketFlushed(QByteArray,int)),
            pTransferManager, SLOT(bucketFlushed(QByteArray,int)), Qt::QueuedConnection);
    connect(pBucketFlushThread, SIGNAL(bucketFlushFailed(QByteArray,int)),
            pTransferManager, SLOT(bucketFlushFailed(QByteArray,int)), Qt::QueuedConnection);

    //Connect bucketFlushThread to GUI
    connect(pBucketFlushThread, SIGNAL(fileAssemblyComplete(QString)),
            this, SLOT(fileAssemblyComplete(QString)), Qt::QueuedConnection);

    //Construct the auto update object
    pFtpUpdate = new FTPUpdate(pSettingsManager->getSetting(SettingsManager::FTP_UPDATE_HOST), pSettingsManager->getSetting(SettingsManager::FTP_UPDATE_DIRECTORY), this);

    connect(pFtpUpdate, SIGNAL(returnUpdateResults(bool, QString)),
        this, SLOT(ftpReturnUpdateResults(bool, QString)));
    connect(pFtpUpdate, SIGNAL(downloadCompleted(bool)),
        this, SLOT(ftpDownloadCompleted(bool)));
    connect(pFtpUpdate, SIGNAL(dataTransferProgress(qint64, qint64)),
        this, SLOT(ftpDataTransferProgress(qint64, qint64)));
    connect(this, SIGNAL(ftpCheckForUpdate()),
        pFtpUpdate, SLOT(checkForUpdate()));
    connect(this, SIGNAL(ftpDownloadNewestVersion()),
        pFtpUpdate, SLOT(downloadNewestVersion()));
    
    //pDispatcher->moveToThread(transferThread);
    pDispatcher->moveToThread(dispatcherThread);
    dispatcherThread->start(QThread::HighPriority);

    pShare->moveToThread(dbThread);
    dbThread->start();

    pBucketFlushThread->moveToThread(bucketFlushThread);
    bucketFlushThread->start();

    pTransferManager->moveToThread(transferThread);
    transferThread->start();

    arpmanetDCLogoNormal = new QPixmap(":/ArpmanetDC/Resources/Logo128x128.png");
    arpmanetDCLogoNotify = new QPixmap(":/ArpmanetDC/Resources/Logo128x128Notify.png");

    //GUI setup
    createWidgets();
    placeWidgets();
    connectWidgets();  

    pStatusHistoryList = new QList<QString>();
    pAdditionalInfoHistoryList = new QList<QString>();
    pQueueList = new QHash<QByteArray, QueueStruct>();
    pFinishedList = new QHash<QByteArray, FinishedDownloadStruct>();
    pUserCommands = new QHash<QString, UserCommandStruct>();

    //Get queue
    setStatus(tr("Loading download queue from database..."));
    emit requestQueueList();   

    //Get finished list
    setStatus(tr("Loading finished downloads from database..."));
    emit requestFinishedList();

    //Get word list
    setStatus(tr("Loading autocomplete list from database..."));
    emit requestAutoCompleteWordList(searchWordList);

    //Get user command list
    emit requestUserCommands();

    //Update shares
    setStatus(tr("Share update procedure started. Parsing directories/paths..."));
    hashingProgressBar->setTopText("Parsing");
    emit updateShares();    

    downloadQueueWidget = 0;
    shareWidget = 0;
    queueWidget = 0;
    finishedWidget = 0;
    settingsWidget = 0;
    helpWidget = 0;
    //transferWidget = 0;

    arpmanetDCUsers = 0;
    notifyCount = 0;

    pCloseEvent = 0;

    saveSharesPressed = false;

    //Icon generation
    userIcon = new QPixmap();
    *userIcon = QPixmap(":/ArpmanetDC/Resources/UserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    arpmanetUserIcon = new QPixmap();
    *arpmanetUserIcon = QPixmap(":/ArpmanetDC/Resources/ArpmanetUserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    userFirewallIcon = new QPixmap();
    *userFirewallIcon = QPixmap(":/ArpmanetDC/Resources/FirewallIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    oldVersionUserIcon = new QPixmap();
    *oldVersionUserIcon = QPixmap(":/ArpmanetDC/Resources/SkullUserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    newerVersionUserIcon = new QPixmap();
    *newerVersionUserIcon = QPixmap(":/ArpmanetDC/Resources/CoolUserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    fullyBootstrappedIcon = new QPixmap();
    *fullyBootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerOnlineIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    bootstrappedIcon = new QPixmap();
    *bootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    unbootstrappedIcon = new QPixmap();
    *unbootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerOfflineIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    //Set window icon
    setWindowIcon(QIcon(*arpmanetDCLogoNormal));

    //Init type icon list with resource extractor class
    pTypeIconList = new ResourceExtractor();

    //Load emoticons
    loadEmoticonsFromFile();

    //Set up shared memory timer
    QTimer *sharedMemoryTimer = new QTimer(this);
    connect(sharedMemoryTimer, SIGNAL(timeout()), this, SLOT(checkSharedMemory()));
    sharedMemoryTimer->start(500);

    //Set up timer to ensure each and every update doesn't signal a complete list sort!
    sortDue = false;
    QTimer *sortTimer = new QTimer(this);
    connect(sortTimer, SIGNAL(timeout()), this, SLOT(sortUserList()));
    sortTimer->start(1000); //Meh, every second ought to do it

    //Set up rate timer
    hashRateTimer = new QTimer(this);
    connect(hashRateTimer, SIGNAL(timeout()), this, SLOT(calculateHashRate()));
    hashRateTimer->start(1000);

    //Set up timer to update the number of CID hosts currently bootstrapped to
    updateTimer = new QTimer(this);
    connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateGUIEverySecond()));
    updateTimer->start(5000);

    //Set up timer to auto update shares
    updateSharesTimer = new QTimer(this);
    connect(updateSharesTimer, SIGNAL(timeout()), this, SIGNAL(updateShares()));
    int interval = pSettingsManager->getSetting(SettingsManager::AUTO_UPDATE_SHARE_INTERVAL);
    if (interval > 0)
        updateSharesTimer->start(interval);

#ifdef Q_OS_WIN
    //Set up timer to check for client updates
    checkForFTPUpdatesTimer = new QTimer(this);
    connect(checkForFTPUpdatesTimer, SIGNAL(timeout()), this, SIGNAL(ftpCheckForUpdate()));
    checkForFTPUpdatesTimer->start(pSettingsManager->getSetting(SettingsManager::CHECK_FOR_NEW_VERSION_INTERVAL_MS));
#endif

    //Save uptime
    uptime = QDateTime::currentDateTime();

    //Search for magnet if arguments contain one
    if (!magnetArg.isEmpty() && QString(magnetArg).compare("DUPLICATE_INSTANCE") != 0)
        mainChatLinkClicked(QUrl(QString(magnetArg)));

    //Show settings window if nick is still default
    if (pSettingsManager->isDefault(SettingsManager::NICKNAME) && pSettingsManager->isDefault(SettingsManager::PASSWORD))
    {
        if (QMessageBox::information(this, tr("ArpmanetDC"), tr("Please change your nickname and password and click ""Save changes"" to start using ArpmanetDC")) == QMessageBox::Ok)
            settingsAction->trigger();
    }
    else
        //Wait 1 second for everything to initialize before starting the hub
        QTimer::singleShot(1000, pHub, SLOT(connectHub()));

    createdGUI = true;
    
#ifdef Q_OS_WIN
    //Check for program updates
    emit ftpCheckForUpdate();
#endif
}

ArpmanetDC::~ArpmanetDC()
{
    if (createdGUI)
    {
        //Destructor
        systemTrayIcon->hide(); //Icon isn't automatically hidden after program exit

        pHub->deleteLater();
        pTransferManager->deleteLater();
        pDispatcher->deleteLater();
        pBucketFlushThread->deleteLater();
        pShare->deleteLater();

        delete pAdditionalInfoHistoryList;
        delete pTypeIconList;
        
        //saveSettings();
        //delete pSettings;

        transferThread->quit();
        if (transferThread->wait(5000))
            delete transferThread;
        else
        {
            transferThread->terminate();
            delete transferThread;
        }

        bucketFlushThread->quit();
        statusLabel->setText(tr("Flushing downloaded data to disk, please wait."));
        if (bucketFlushThread->wait(ULONG_MAX))
            delete bucketFlushThread;
        else
        {
            bucketFlushThread->terminate();
            delete bucketFlushThread;
        }

        dispatcherThread->quit();
        if (dispatcherThread->wait(5000))
            delete dispatcherThread;
        else
        {
            dispatcherThread->terminate();
            delete dispatcherThread;
        }

        sqlite3_close(db);
        dbThread->quit();
        if (dbThread->wait(5000))
            delete dbThread;
        else
        {
            dbThread->terminate();
            delete dbThread;
        }
    }

    delete pSettingsManager;

    //Destroy and detach the shared memory sector
#ifdef Q_OS_LINUX
    pSharedMemory->detach();
#endif
    pSharedMemory->deleteLater();
}

bool ArpmanetDC::setupDatabase()
{
    //Create database/open existing database
    if (sqlite3_open(shareDatabasePath.toUtf8().data(), &db))
    {
        QString error("Can't open database");
        sqlite3_close(db);
        return false;
    }

    //Check if tables exist and create them otherwise
    sqlite3_stmt *statement;

    QList<QString> queries;

    //Set full synchronicity
    queries.append("PRAGMA synchronous = NORMAL;");

    //Commit any outstanding queries
    queries.append("COMMIT;");

    //Create FileShares table - list of all files hashed
    queries.append("CREATE TABLE FileShares (rowID INTEGER PRIMARY KEY, tth TEXT, fileName TEXT, fileSize INTEGER, filePath TEXT, lastModified TEXT, shareDirID INTEGER, active INTEGER, majorVersion INTEGER, minorVersion INTEGER, relativePath TEXT, FOREIGN KEY(shareDirID) REFERENCES SharePaths(rowID), UNIQUE(filePath));");
    queries.append("CREATE INDEX IDX_FILESHARES_FILENAME on FileShares(fileName);");
    queries.append("CREATE INDEX IDX_FILESHARES_FILEPATH on FileShares(filePath);");
    queries.append("CREATE INDEX IDX_FILESHARES_FILESIZE on FileShares(fileSize);");
    queries.append("CREATE INDEX IDX_FILESHARES_ACTIVE on FileShares(active);");

    //Create 1MB TTH table - list of the 1MB bucket TTHs for every fileshare
    queries.append("CREATE TABLE OneMBTTH (rowID INTEGER PRIMARY KEY, oneMBtth TEXT, tth TEXT, offset INTEGER, fileShareID INTEGER, FOREIGN KEY(fileShareID) REFERENCES FileShares(rowID));");
    queries.append("CREATE INDEX IDX_TTH on OneMBTTH(tth);");

    //Create SharePaths table - list of all the folders/files chosen in ShareWidget
    queries.append("CREATE TABLE SharePaths (rowID INTEGER PRIMARY KEY, path TEXT, UNIQUE(path));");
    queries.append("CREATE INDEX IDX_SHAREPATHS_PATH on SharePaths(path);");

    //Create TTHSources table - list of all sources for a transfer
    queries.append("CREATE TABLE TTHSources (rowID INTEGER PRIMARY KEY, tthRoot TEXT, source TEXT, UNIQUE(tthRoot, source));");
    queries.append("CREATE INDEX IDX_TTHSOURCES_TTHROOT on TTHSources(tthRoot);");

    //Create LastKnownPeers table - list of the last known peers for bootstrap
    queries.append("CREATE TABLE LastKnownPeers (rowID INTEGER PRIMARY KEY, ip TEXT, UNIQUE(ip));");
    
    //Create QueuedDownloads table - saves all queued downloads to restart transfers after restart
    queries.append("CREATE TABLE QueuedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, fileSize INTEGER, priority INTEGER, tthRoot TEXT, hostIP TEXT, UNIQUE(tthRoot));");
    queries.append("CREATE INDEX IDX_QUEUED_FILEPATH on QueuedDownloads(filePath);");

    //Create FinishedDownloads table - saves download paths that were downloaded for list
    queries.append("CREATE TABLE FinishedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, fileSize INTEGER, tthRoot TEXT, downloadedDate TEXT);");
    queries.append("CREATE INDEX IDX_FINISHED_FILEPATH on FinishedDownloads(filePath);");

    //Create Settings table - saves settings (doh)
    queries.append("CREATE TABLE Settings (rowID INTEGER PRIMARY KEY, parameter TEXT, value TEXT);");

    //Create AutoCompleteWords table - saves searches
    queries.append("CREATE TABLE AutoCompleteWords (rowID INTEGER PRIMARY KEY, word TEXT, count INTEGER);");
    queries.append("CREATE INDEX IDX_AUTO_COMPLETE_WORDS on AutoCompleteWords(word);");

    //Create UserCommands table - saves user commands
    queries.append("CREATE TABLE UserCommands (rowID INTEGER PRIMARY KEY, name TEXT, command TEXT, parameterCount INT, output TEXT, UNIQUE(command));");

    //Create FileStateBitmaps table - saves bitmap data for a specific file for resuming
    queries.append("CREATE TABLE FileStateBitmaps (rowID INTEGER PRIMARY KEY, tthRoot TEXT, bitmap TEXT, UNIQUE(tthRoot));");
    queries.append("CREATE INDEX IDX_FILE_STATE_BITMAPS on FileStateBitmaps(tthRoot);");

    QList<QString> queryErrors(queries);

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i).toUtf8());
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            queryErrors[i] = error;
    }

    return true;
}

/*bool ArpmanetDC::loadSettings()
{
    QList<QString> queries;
    pSettings->clear();
    
     //Create Settings table - saves settings (doh)
    queries.append("SELECT [parameter], [value]  FROM Settings;");

    sqlite3_stmt *statement;
    QList<QString> queryErrors(queries);

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i).toUtf8());
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                //Load settings
                if (cols == 2)
                {
                    QString parameter = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 0));
                    QString value = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 1));
                    if (!parameter.isEmpty())
                        pSettings->insert(parameter, value);
                }
            };
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            queryErrors[i] = error;
    }

    return !pSettings->isEmpty();
}

bool ArpmanetDC::saveSettings()
{
    QList<QString> queries;
    //queries.append("BEGIN;");
    queries.append("DELETE FROM Settings;");
    
    QList<QString> parameters = pSettings->keys();
    for (int i = 0; i < pSettings->size(); i++)
        queries.append("INSERT INTO Settings([parameter], [value]) VALUES (?, ?);");

    queries.append("COMMIT;");
    queries.append("BEGIN;");

    bool result = true;
    QList<QString> queryErrors(queries);
    sqlite3_stmt *statement;

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i).toUtf8());
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            if (query.contains("INSERT INTO Settings") && !parameters.isEmpty())
            {
                int res = 0;
                QString parameter = parameters.takeFirst();
                QString value = pSettings->value(parameter);
                res = res | sqlite3_bind_text16(statement, 1, parameter.utf16(), parameter.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_text16(statement, 2, value.utf16(), value.size()*2, SQLITE_STATIC);
                if (res != SQLITE_OK)
                    result = false;
            }

            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            queryErrors[i] = error;
    }

    return result;
}*/

void ArpmanetDC::createWidgets()
{
    //========== Auto completer ==========
    searchWordList = new QStandardItemModel();
    searchCompleter = new QCompleter(searchWordList, this);
    searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    searchCompleter->setModelSorting(QCompleter::CaseInsensitivelySortedModel);

    //Labels
    userHubCountLabel = new QLabel(tr("Hub Users: 0"));
    additionalInfoLabel = new QLabel(tr(""));

    statusLabel = new QLabel(tr("Status"));
    shareSizeLabel = new QLabel(tr("Share: 0 bytes"));

    bootstrapStatusLabel = new QLabel();
    //connectionIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connectionIconLabel = new QLabel();
    //connectionIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    CIDHostsLabel = new QLabel("0");
    CIDHostsLabel->setToolTip(tr("Number of hosts bootstrapped"));

    totalShareSizeLabel = new QLabel;

    //Progress bar
    hashingProgressBar = new TextProgressBar(tr("Hashing"), this);
    hashingProgressBar->setRange(0,0);
    hashingProgressBar->setStyle(new QPlastiqueStyle());
    hashingProgressBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    hashingProgressBar->setFormat(tr("H"));
    hashingProgressBar->setTextVisible(true);

    //Splitters
    splitterVertical = new QSplitter(this);
    splitterHorizontal = new QSplitter(Qt::Vertical, this);
    
    //Tab widget
    tabs = new CTabWidget(this);
    tabs->setIconSize(QSize(16,16));
    tabs->setTabsClosable(true);
    tabTextColorNotify = QColor(Qt::red);
    tabTextColorOffline = QColor(Qt::gray);
    
    //Chat
    mainChatTextEdit = new QTextBrowser(this);
    mainChatTextEdit->setOpenExternalLinks(false);
    mainChatTextEdit->setOpenLinks(false);
    mainChatTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    
    chatLineEdit = new KeyLineEdit(this);
    chatLineEdit->setPlaceholderText(tr("Type here to chat"));

    quickSearchLineEdit = new QLineEdit(this);
    quickSearchLineEdit->setPlaceholderText("Type here to search");
    quickSearchLineEdit->setCompleter(searchCompleter);
    
    //===== User list =====
    //Model
    userListModel = new QStandardItemModel(0,7);
    userListModel->setHeaderData(0, Qt::Horizontal, tr("Nickname"));
    userListModel->setHeaderData(1, Qt::Horizontal, tr("Description"));
    userListModel->setHeaderData(2, Qt::Horizontal, tr("Nickname"));
    userListModel->setHeaderData(3, Qt::Horizontal, tr("Description"));
    userListModel->setHeaderData(4, Qt::Horizontal, tr("Mode"));
    userListModel->setHeaderData(5, Qt::Horizontal, tr("Client"));
    userListModel->setHeaderData(6, Qt::Horizontal, tr("Version"));

    userSortProxy = new QSortFilterProxyModel(this);
    userSortProxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    userSortProxy->setSourceModel(userListModel);
    
    //Table
    userListTable = new CKeyTableView(this);
    userListTable->setContextMenuPolicy(Qt::CustomContextMenu);
    
    //Link table and model
    userListTable->setModel(userSortProxy);

    //Sizing
    userListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    userListTable->setColumnWidth(0,150);
    userListTable->setColumnWidth(1,250);
    userListTable->horizontalHeader()->setMinimumSectionSize(150);
    
    //Style
    userListTable->setShowGrid(true);
    userListTable->setGridStyle(Qt::DotLine);
    userListTable->verticalHeader()->hide();
    userListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    userListTable->setSelectionMode(QAbstractItemView::SingleSelection);
    //userListTable->setItemDelegate(new HTMLDelegate(userListTable));
    userListTable->setAutoScroll(false);
    userListTable->setSortingEnabled(true);
    userListTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    //userListTable->horizontalHeader()->setStretchLastSection(true);

    //userListTable->hideColumn(1);
    userListTable->hideColumn(2);
    userListTable->hideColumn(3);
    userListTable->hideColumn(4);
    userListTable->hideColumn(5);
    userListTable->hideColumn(6);

    //TransferWidget
    transferWidget = new TransferWidget(pTransferManager, this);

    //===== Bars =====
    //Toolbar
    toolBar = new QToolBar(tr("Actions"), this);
    //toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    //toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setMovable(false);
    toolBar->setContextMenuPolicy(Qt::CustomContextMenu);

    //Menu bar
    menuBar = new QMenuBar(this);

    //Status bars
    statusBar = new QStatusBar(this);
    statusBar->addPermanentWidget(statusLabel,1);
    statusBar->addPermanentWidget(connectionIconLabel);
    statusBar->addPermanentWidget(bootstrapStatusLabel);
    statusBar->addPermanentWidget(CIDHostsLabel);
    statusBar->addPermanentWidget(hashingProgressBar);
    statusBar->addPermanentWidget(shareSizeLabel);

    infoStatusBar = new QStatusBar(this);
    infoStatusBar->addPermanentWidget(additionalInfoLabel,1);
    infoStatusBar->addPermanentWidget(totalShareSizeLabel);
    infoStatusBar->addPermanentWidget(userHubCountLabel);
    infoStatusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    infoStatusBar->setSizeGripEnabled(false);
    
    //===== Actions =====
    queueAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Download queue"), this);
    queueAction->setToolTip(tr("Displays a list of all downloads currently in the queue"));
    downloadFinishedAction = new QAction(QIcon(":/ArpmanetDC/Resources/DownloadFinishedIcon.png"), tr("Finished downloads"), this);
    downloadFinishedAction->setToolTip(tr("Displays a list of all finished downloads"));
    searchAction = new QAction(QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"), this);
    searchAction->setToolTip(tr("Search for files"));
    shareAction = new QAction(QIcon(":/ArpmanetDC/Resources/ShareIcon.png"), tr("Share"), this);
    shareAction->setToolTip(tr("Select the files you want to share with other users"));
    settingsAction = new QAction(QIcon(":/ArpmanetDC/Resources/SettingsIcon.png"), tr("Settings"), this);
    settingsAction->setToolTip(tr("Displays the client settings page"));
    helpAction = new QAction(QIcon(":/ArpmanetDC/Resources/HelpIcon.png"), tr("Help"), this);
    helpAction->setToolTip(tr("Go here if you feel lost or confused..."));
    privateMessageAction = new QAction(QIcon(":/ArpmanetDC/Resources/EnvelopeIcon.png"), tr("Send PM"), this);
    reconnectAction = new QAction(QIcon(":/ArpmanetDC/Resources/ServerIcon.png"), tr("Reconnect"), this);
    reconnectAction->setToolTip(tr("Reconnect the hub connection"));
    openDownloadDirAction = new QAction(QIcon(":/ArpmanetDC/Resources/FolderIcon.png"), tr("Download directory"), this);
    openDownloadDirAction->setToolTip(tr("Opens the current download folder to view downloaded files"));
    pmAction = new QAction(QIcon(":/ArpmanetDC/Resources/EnvelopeIcon.png"), tr("Send PM"), this);
    openContainerAction = new QAction(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), tr("Open container"), this);
    openContainerAction->setToolTip(tr("Open a downloaded container to view and enqueue files from it"));

    //===== Menus =====
    userListMenu = new QMenu(this);
    userListMenu->addAction(privateMessageAction);

    userCommandListMenu = new QMenu(this);
    userCommandListMenu->setTitle(tr("Use as first parameter in user command"));
    userCommandListMenu->setIcon(QIcon(":/ArpmanetDC/Resources/UserIcon.png"));

    //========== System tray ==========
    systemTrayIcon = new QSystemTrayIcon(this);
    systemTrayIcon->setIcon(QIcon(*arpmanetDCLogoNormal));
    systemTrayIcon->setToolTip(tr("ArpmanetDC v%1").arg(VERSION_STRING));
    systemTrayIcon->show();

    systemTrayMenu = new QMenu(this);

    quitAction = new QAction(QIcon(":/ArpmanetDC/Resources/DeleteIcon.png"), "Quit", this);
    restoreAction = new QAction(QIcon(":/ArpmanetDC/Resources/GreenUpIcon.png"), "Restore", this);
    restoreAction->setEnabled(false);

    systemTrayMenu->addAction(restoreAction);
    systemTrayMenu->addAction(quitAction);

    systemTrayIcon->setContextMenu(systemTrayMenu);
}

void ArpmanetDC::placeWidgets()
{
    //Add actions to toolbar
    toolBar->addAction(searchAction);
    toolBar->addAction(shareAction);
    toolBar->addAction(reconnectAction);
    toolBar->addSeparator();
    toolBar->addAction(queueAction);
    toolBar->addAction(downloadFinishedAction);
    toolBar->addAction(openDownloadDirAction);
    toolBar->addAction(openContainerAction);
    toolBar->addSeparator();
    toolBar->addAction(settingsAction);
    toolBar->addAction(helpAction);
    toolBar->addSeparator();

    //Add search widget on toolbar
    QWidget *quickSearchWidget = new QWidget();
    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->addStretch(1);

    QVBoxLayout *searchVertLayout = new QVBoxLayout;
    searchVertLayout->addStretch(1);
    searchVertLayout->addWidget(quickSearchLineEdit);
    searchVertLayout->addStretch(1);
    searchLayout->addLayout(searchVertLayout);
    searchLayout->setContentsMargins(0,0,0,0);
    quickSearchWidget->setLayout(searchLayout);

    toolBar->addWidget(quickSearchWidget);

    //Place splitter
    splitterVertical->addWidget(mainChatTextEdit);
    splitterVertical->addWidget(userListTable);

    QList<int> sizes;
    sizes << 600 << 200;
    splitterVertical->setSizes(sizes);
    
    //Place vertical widgets
    QGridLayout *layout = new QGridLayout;
    layout->addWidget(splitterVertical,0,0);
    layout->addWidget(chatLineEdit,1,0);
    layout->addWidget(infoStatusBar,2,0);
    layout->setContentsMargins(0,0,0,0);

    QWidget *layoutWidget = new QWidget(this);
    layoutWidget->setLayout(layout);

    //Place main tab widget
    QWidget *tabWidget = new QWidget();
    QVBoxLayout *tabLayout = new QVBoxLayout();
    tabLayout->setContentsMargins(0,0,0,0);
    tabLayout->setSpacing(0);
    tabLayout->addWidget(layoutWidget);
    tabWidget->setLayout(tabLayout);
    
    tabs->addTab(tabWidget, QIcon(":/ArpmanetDC/Resources/ServerOfflineIcon.png"), tr("Arpmanet Chat"));
    tabs->setTabPosition(QTabWidget::North);
    tabTextColorNormal = tabs->tabBar()->tabTextColor(tabs->indexOf(tabWidget));
    tabs->tabBar()->tabButton(0,QTabBar::RightSide)->hide();

    //Place horizontal widgets
    splitterHorizontal->addWidget(tabs);
    splitterHorizontal->addWidget(transferWidget->widget());
    
    sizes.clear();
    sizes << 400 << 200;
    splitterHorizontal->setSizes(sizes);

    //Set MainWindow paramters
    setMenuBar(menuBar);
    addToolBar(toolBar);
    setStatusBar(statusBar);
    setIconSize(QSize(64,64));    
    setCentralWidget(splitterHorizontal);
    
    //Set the window in the center of the screen
    QSize appSize = sizeHint();
    int x = (QApplication::desktop()->screenGeometry().width() - appSize.width()) / 2;
    int y = (QApplication::desktop()->screenGeometry().height() - appSize.height()) / 2;
    move(x, y);
}

void ArpmanetDC::connectWidgets()
{
    //Context menu
    connect(userListTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showUserListContextMenu(const QPoint&)));
    connect(userListTable, SIGNAL(keyPressed(Qt::Key, QString)), this, SLOT(userListKeyPressed(Qt::Key, QString)));

    //Send chat message when enter is pressed
    connect(chatLineEdit, SIGNAL(returnPressed()), this, SLOT(chatLineEditReturnPressed()));
    connect(chatLineEdit, SIGNAL(keyPressed(Qt::Key, QString)), this, SLOT(chatLineEditKeyPressed(Qt::Key, QString)));

    //Quick search
    connect(quickSearchLineEdit, SIGNAL(returnPressed()), this, SLOT(quickSearchPressed()));

    connect(mainChatTextEdit, SIGNAL(anchorClicked(const QUrl &)), this, SLOT(mainChatLinkClicked(const QUrl &)));
    connect(mainChatTextEdit, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showMainChatContextMenu(const QPoint&)));
    
    //Connect actions
    connect(queueAction, SIGNAL(triggered()), this, SLOT(queueActionPressed()));
    connect(shareAction, SIGNAL(triggered()), this, SLOT(shareActionPressed()));
    connect(searchAction, SIGNAL(triggered()), this, SLOT(searchActionPressed()));
    connect(downloadFinishedAction, SIGNAL(triggered()), this, SLOT(downloadFinishedActionPressed()));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(settingsActionPressed()));
    connect(helpAction, SIGNAL(triggered()), this, SLOT(helpActionPressed()));
    connect(privateMessageAction, SIGNAL(triggered()), this, SLOT(privateMessageActionPressed()));
    connect(reconnectAction, SIGNAL(triggered()), this, SLOT(reconnectActionPressed()));
    connect(openDownloadDirAction, SIGNAL(triggered()), this, SLOT(openDownloadDirActionPressed()));
    connect(pmAction, SIGNAL(triggered()), this, SLOT(pmActionPressed()));
    connect(openContainerAction, SIGNAL(triggered()), this, SLOT(openContainerActionPressed()));

    connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

    connect(userCommandListMenu, SIGNAL(triggered(QAction *)), this, SLOT(userCommandMenuPressed(QAction *)));

    //Connect system tray
    connect(systemTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(systemTrayActivated(QSystemTrayIcon::ActivationReason)));

    //Tab widget
    connect(tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(tabDeleted(int)));
    connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
}

void ArpmanetDC::chatLineEditReturnPressed()
{
    //===== Client commands =====
    //Display all the buckets with their respective hosts
    if (chatLineEdit->text().compare("/debugbuckets") == 0)
    {
        appendChatLine("DEBUG Bucket contents");
        appendChatLine(pDispatcher->getDebugBucketsContents());
        chatLineEdit->setText("");
    }
    //Display all known hosts in your own buckets (since startup)
    else if (chatLineEdit->text().compare("/debugcidhosts") == 0)
    {
        appendChatLine("DEBUG CID Host contents");
        appendChatLine(pDispatcher->getDebugCIDHostContents());
        chatLineEdit->setText("");
    }
    //Scan network for hosts (overuse can be dangerous)
    else if (chatLineEdit->text().compare("/linscan") == 0)
    {
        appendChatLine("DEBUG Linear scan hosts on network");
        pDispatcher->initiateLinscan();
        chatLineEdit->setText("");
    }
    //Display client uptime
    else if (chatLineEdit->text().compare("/uptime") == 0 || chatLineEdit->text().compare("/u") == 0)
    {
        pHub->sendChatMessage(tr("/me uptime: %1").arg(uptimeFromInt(QDateTime::currentMSecsSinceEpoch() - uptime.toMSecsSinceEpoch())));
        chatLineEdit->setText("");
    }
    //Display link to update DC - since EVERYONE is CONSTANTLY asking for this... sheesh
    else if (chatLineEdit->text().compare("/dclink") == 0)
    {
        pHub->sendChatMessage(tr("DC Link: %1%2").arg(pSettingsManager->getSetting(SettingsManager::FTP_UPDATE_HOST)).arg(pSettingsManager->getSetting(SettingsManager::FTP_UPDATE_DIRECTORY)));
        chatLineEdit->setText("");
    }
    else if (chatLineEdit->text().compare("/w") == 0)
    {
        QString title = getWinampSongTitle();
        if (!title.isEmpty())
        {
            //TODO: Set Winamp string in settings
            pHub->sendChatMessage(tr("/me is listening to %1").arg(title));
        }
        else
            appendChatLine("<font color=\"darkGray\">Winamp is not running or feature is unsupported</font>");
        chatLineEdit->setText("");
    }
    //User command (starts with //)
    else if (chatLineEdit->text().startsWith(tr("//")))
    {
        pHub->sendChatMessage(processUserCommand(chatLineEdit->text()));
        chatLineEdit->setText("");
    }
    //Otherwise, just send the message normally
    else
        sendChatMessage();

}

void ArpmanetDC::sendChatMessage()
{
    //Send the chat message in the line
    if (!chatLineEdit->text().isEmpty())
    {
        pHub->sendChatMessage(chatLineEdit->text());
        chatLineEdit->setText("");
    }
}

void ArpmanetDC::quickSearchPressed()
{
    //Search for text
    SearchWidget *sWidget = new SearchWidget(searchCompleter, pTypeIconList, pTransferManager, quickSearchLineEdit->text(), this);
    quickSearchLineEdit->clear();  

    connect(sWidget, SIGNAL(search(quint64, QString, QByteArray, SearchWidget *)), this, SLOT(searchButtonPressed(quint64, QString, QByteArray, SearchWidget *)));
    connect(sWidget, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), pTransferManager, SLOT(queueDownload(int, QByteArray, QString, quint64, QHostAddress)));

    searchWidgetHash.insert(sWidget->widget(), sWidget);
    searchWidgetIDHash.insert(sWidget->id(), sWidget);

    tabs->addTab(sWidget->widget(), QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"));

    tabs->setCurrentIndex(tabs->indexOf(sWidget->widget()));

    //Wait for widget to open
    QApplication::processEvents();

    //Search
    sWidget->searchPressed();
}

void ArpmanetDC::searchActionPressed()
{
    SearchWidget *sWidget = new SearchWidget(searchCompleter, pTypeIconList, pTransferManager, this);
    
    connect(sWidget, SIGNAL(search(quint64, QString, QByteArray, SearchWidget *)), this, SLOT(searchButtonPressed(quint64, QString, QByteArray, SearchWidget *)));
    connect(sWidget, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), pTransferManager, SLOT(queueDownload(int, QByteArray, QString, quint64, QHostAddress)));

    searchWidgetHash.insert(sWidget->widget(), sWidget);
    searchWidgetIDHash.insert(sWidget->id(), sWidget);

    tabs->addTab(sWidget->widget(), QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"));

    tabs->setCurrentIndex(tabs->indexOf(sWidget->widget()));
}

void ArpmanetDC::queueActionPressed()
{
    //Check if queue widget exists already
    if (queueWidget)
    {
        //If it does, select it and return
        if (tabs->indexOf(queueWidget->widget()) != -1)
        {
            tabs->setCurrentIndex(tabs->indexOf(queueWidget->widget()));
            return;
        }
    }

    //Otherwise, create it
    queueWidget = new DownloadQueueWidget(pQueueList, pShare, this);
    
    tabs->addTab(queueWidget->widget(), QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Download Queue"));

    tabs->setCurrentIndex(tabs->indexOf(queueWidget->widget()));
}

void ArpmanetDC::downloadFinishedActionPressed()
{
    //Check if widget exists already
    if (finishedWidget)
    {
        //If it does, select it and return
        if (tabs->indexOf(finishedWidget->widget()) != -1)
        {
            tabs->setCurrentIndex(tabs->indexOf(finishedWidget->widget()));
            return;
        }
    }

    //Otherwise, create it
    finishedWidget = new DownloadFinishedWidget(pFinishedList, this);
        
    tabs->addTab(finishedWidget->widget(), QIcon(":/ArpmanetDC/Resources/DownloadFinishedIcon.png"), tr("Finished Downloads"));

    tabs->setCurrentIndex(tabs->indexOf(finishedWidget->widget()));
}

void ArpmanetDC::shareActionPressed()
{
    //Check if share widget exists already
    if (shareWidget)
    {
        //If it does, select it and return
        if (tabs->indexOf(shareWidget->widget()) != -1)
        {
            tabs->setCurrentIndex(tabs->indexOf(shareWidget->widget()));
            return;
        }
    }

    //Otherwise, create it
    shareWidget = new ShareWidget(pShare, this);
    connect(shareWidget, SIGNAL(saveButtonPressed()), this, SLOT(shareSaveButtonPressed()));
    
    tabs->addTab(shareWidget->widget(), QIcon(":/ArpmanetDC/Resources/ShareIcon.png"), tr("Share"));

    tabs->setCurrentIndex(tabs->indexOf(shareWidget->widget()));
}

void ArpmanetDC::reconnectActionPressed()
{
    //Clear userlists
    pUserList.clear();
    pOPList.clear();
    pADCUserList.clear();

    userListModel->removeRows(0, userListModel->rowCount());
    
    //Set parameters and reconnect hub
    pHub->setHubAddress(pSettingsManager->getSetting(SettingsManager::HUB_ADDRESS));
    pHub->setHubPort(pSettingsManager->getSetting(SettingsManager::HUB_PORT));
    pHub->setNick(pSettingsManager->getSetting(SettingsManager::NICKNAME));
    pHub->setPassword(pSettingsManager->getSetting(SettingsManager::PASSWORD));
    pHub->connectHub();
}

void ArpmanetDC::openDownloadDirActionPressed()
{
    QString path = QDir::toNativeSeparators(pSettingsManager->getSetting(SettingsManager::DOWNLOAD_PATH));
    QDesktopServices::openUrl(QUrl("file:///" + path));
}

//Open a downloaded container
void ArpmanetDC::openContainerActionPressed()
{
    //Get the container path
    QString containerPath;
    containerPath = QFileDialog::getOpenFileName(this, tr("Open container"), pSettingsManager->getSetting(SettingsManager::DOWNLOAD_PATH), tr("Containers (*.adcc)"));
    containerPath.replace("\\", "/");

    if (!containerPath.isEmpty())
    {
        //Process the container
        QFileInfo fi(containerPath);
        QString filePath = containerPath.left(containerPath.lastIndexOf("/")+1);
        emit processContainer(QHostAddress(), containerPath, filePath);
    }
}

//Shortcut PM action pressed - when right-clicking on a user name
void ArpmanetDC::pmActionPressed()
{
    QString nick = pmAction->data().toString();

    //Check if a tab exists with a PM for this nick
    QWidget *foundWidget = 0;
    QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
    while (i.hasNext())
    {
        if (i.peekNext().value()->otherNick().compare(nick) == 0)
        {
            foundWidget = i.value()->widget();
            break;
        }
        i.next();
    }

    //If no existing tab is found - create new one
    if (!foundWidget)
    {
        PMWidget *pmWidget = new PMWidget(nick, this);
        connect(pmWidget, SIGNAL(sendPrivateMessage(QString, QString, PMWidget *)), this, SLOT(pmSent(QString, QString, PMWidget *)));
        pmWidgetHash.insert(pmWidget->widget(), pmWidget);

        tabs->addTab(pmWidget->widget(), QIcon(":/ArpmanetDC/Resources/UserIcon.png"), tr("PM - %1").arg(nick));
        tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));

        if (!pUserList.contains(nick))
            pmWidget->userLoginChanged(false);
    }
    //Else switch to existing tab
    else
    {
        tabs->setCurrentIndex(tabs->indexOf(foundWidget));
    }
}

//Send user command with selected word as first argument
void ArpmanetDC::userCommandMenuPressed(QAction *action)
{
    //Get command name and arguments
    QString userCommandName = action->text();
    QString argument = action->data().toString();

    //Must be a valid command
    if (!pUserCommands->contains(userCommandName))
        return;

    //Build the command
    UserCommandStruct u = pUserCommands->value(userCommandName);
    QString command = tr("//%1 %2").arg(u.command).arg(argument);

    //Send it
    pHub->sendChatMessage(processUserCommand(command));
}

void ArpmanetDC::settingsActionPressed()
{
    //Check if share widget exists already
    if (settingsWidget)
    {
        //If it does, select it and return
        if (tabs->indexOf(settingsWidget->widget()) != -1)
        {
            tabs->setCurrentIndex(tabs->indexOf(settingsWidget->widget()));
            return;
        }
    }

    //Otherwise, create it
    settingsWidget = new SettingsWidget(pUserCommands, this);
    connect(settingsWidget, SIGNAL(settingsSaved()), this, SLOT(settingsSaved()));
        
    tabs->addTab(settingsWidget->widget(), QIcon(":/ArpmanetDC/Resources/SettingsIcon.png"), tr("Settings"));

    tabs->setCurrentIndex(tabs->indexOf(settingsWidget->widget()));
}

void ArpmanetDC::helpActionPressed()
{
    //Check if help widget exists already
    if (helpWidget)
    {
        //If it does, select it and return
        if (tabs->indexOf(helpWidget->widget()) != -1)
        {
            tabs->setCurrentIndex(tabs->indexOf(helpWidget->widget()));
            return;
        }
    }

    //Otherwise, create it
    helpWidget = new HelpWidget(this);
    
    tabs->addTab(helpWidget->widget(), QIcon(":/ArpmanetDC/Resources/HelpIcon.png"), tr("Help"));

    tabs->setCurrentIndex(tabs->indexOf(helpWidget->widget()));
}

void ArpmanetDC::privateMessageActionPressed()
{
    //Get selected users
    QModelIndex selectedIndex = userListTable->selectionModel()->selectedRows().first();//userListTable->selectionModel()->selection().indexes().first();
    //Get nick of first user
    QString nick = userSortProxy->data(userSortProxy->index(selectedIndex.row(), 2)).toString();

    //Check if a tab exists with a PM for this nick
    QWidget *foundWidget = 0;
    QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
    while (i.hasNext())
    {
        if (i.peekNext().value()->otherNick().compare(nick) == 0)
        {
            foundWidget = i.value()->widget();
            break;
        }
        i.next();
    }

    //If no existing tab is found - create new one
    if (!foundWidget)
    {
        PMWidget *pmWidget = new PMWidget(nick, this);
        connect(pmWidget, SIGNAL(sendPrivateMessage(QString, QString, PMWidget *)), this, SLOT(pmSent(QString, QString, PMWidget *)));
        pmWidgetHash.insert(pmWidget->widget(), pmWidget);

        tabs->addTab(pmWidget->widget(), QIcon(":/ArpmanetDC/Resources/UserIcon.png"), tr("PM - %1").arg(nick));
        tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));
    }
    //Else switch to existing tab
    else
    {
        tabs->setCurrentIndex(tabs->indexOf(foundWidget));
    }
}

//Show right-click menu on user list
void ArpmanetDC::showUserListContextMenu(const QPoint &pos)
{
    if (userListTable->selectionModel()->selectedRows().size() == 0)
        return;

    userListMenu->clear();
    userListMenu->addAction(privateMessageAction);

    //Get selected user
    QModelIndex selectedIndex = userListTable->selectionModel()->selectedRows().first();
    //Get nick of first user
    QString nick = userSortProxy->data(userSortProxy->index(selectedIndex.row(), 2)).toString();

    //Add user command list
    if (!pUserCommands->isEmpty())
    {
        userCommandListMenu->clear();
        foreach (QString name, pUserCommands->keys())
        {
            QAction *action = new QAction(QIcon(":/ArpmanetDC/Resources/UserIcon.png"), name, userCommandListMenu);
            action->setData(nick);
            userCommandListMenu->addAction(action);
            userCommandListMenu->setTitle(tr("Use %1 in user command").arg(nick));
        }
        userListMenu->addMenu(userCommandListMenu);
    }

    QPoint globalPos = userListTable->viewport()->mapToGlobal(pos);

    userListMenu->popup(globalPos);
}

void ArpmanetDC::showMainChatContextMenu(const QPoint &pos)
{
    //Get current textCursor and selected text
    QTextCursor textCursor = mainChatTextEdit->textCursor();
    QString previouslySelectedText = textCursor.selectedText();

    //Get word under current cursor
    textCursor = mainChatTextEdit->cursorForPosition(pos);
    textCursor.select(QTextCursor::WordUnderCursor);
    QString selectedWord = textCursor.selection().toPlainText();

    //Check for bounded word
    QTextCursor boundedTextCursor = mainChatTextEdit->cursorForPosition(pos); //Points to the cursor position under the mouse
    
    QTextCursor cursorPreviousSpace = mainChatTextEdit->document()->find(QRegExp("(?:\\s|\n)", Qt::CaseInsensitive), boundedTextCursor, QTextDocument::FindBackward);
    QTextCursor cursorNextSpace = mainChatTextEdit->document()->find(QRegExp("(?:\\s|\n)", Qt::CaseInsensitive), boundedTextCursor);

    //Check if matches are in the same block
    if (cursorPreviousSpace.block() != boundedTextCursor.block())
        cursorPreviousSpace = QTextCursor();
    if (cursorNextSpace.block() != boundedTextCursor.block())
        cursorNextSpace = QTextCursor();
    
    QString boundedWord;
    if (cursorPreviousSpace.isNull() && cursorNextSpace.isNull())
        //No spaces were found in the entire document - thus boundedWord is entire document
        boundedWord = boundedTextCursor.block().text();
    else if (cursorPreviousSpace.isNull())
    {
        //No previous space was found - start from beginning
        boundedTextCursor.movePosition(QTextCursor::StartOfBlock);
        boundedTextCursor.setPosition(cursorNextSpace.selectionStart(), QTextCursor::KeepAnchor);
        boundedWord = boundedTextCursor.selectedText();
    }
    else if (cursorNextSpace.isNull())
    {
        //No next space was found - end at end of document
        boundedTextCursor.setPosition(cursorPreviousSpace.selectionEnd());
        boundedTextCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        boundedWord = boundedTextCursor.selectedText();
    }
    else
    {
        //Both previous and next space was found - get word between
        boundedTextCursor.setPosition(cursorPreviousSpace.selectionEnd());
        boundedTextCursor.setPosition(cursorNextSpace.selectionStart(), QTextCursor::KeepAnchor);
        boundedWord = boundedTextCursor.selectedText();
    }

    //Check for nick in line - this isn't active right now since HTML isn't matched in the document - sigh
    /*QTextCursor nickTextCursor = mainChatTextEdit->cursorForPosition(pos); //Points to the cursor position under the mouse
    
    QString startNickHTML = tr("<font color=\"black\">");
    QString endNickHTML = tr("</font>");
    QTextCursor cursorBackwardStart = mainChatTextEdit->document()->find(startNickHTML, QTextDocument::FindBackward);
    QTextCursor cursorBackwardEnd = mainChatTextEdit->document()->find(endNickHTML, QTextDocument::FindBackward);
    QTextCursor cursorForwardStart = mainChatTextEdit->document()->find(startNickHTML);
    QTextCursor cursorForwardEnd = mainChatTextEdit->document()->find(endNickHTML);

    QString nick;
    if (!cursorBackwardStart.isNull() && !cursorForwardEnd.isNull()) //Check if cursors are valid
    {
        if ((cursorBackwardEnd.isNull() || (cursorBackwardEnd.position() < cursorBackwardStart.position())) 
            && (cursorForwardStart.isNull() || (cursorForwardStart.position() > cursorForwardEnd.position())))
        {
            nickTextCursor.setPosition(cursorBackwardStart.selectionEnd());
            nickTextCursor.setPosition(cursorForwardEnd.selectionStart(), QTextCursor::KeepAnchor);
            nick = nickTextCursor.selectedText();
        }
    }*/
                
    //Test if the word is a user nick*/
    QString selectedText;
    if (pUserList.contains(selectedWord))
    {
        //Single word matched - set selection to word
        mainChatTextEdit->setTextCursor(textCursor);
        selectedText = selectedWord;
    }
    else if (pUserList.contains(boundedWord))
    {
        //Space delimited word matched
        selectedText = boundedWord;
        mainChatTextEdit->setTextCursor(boundedTextCursor);
    }
    /*else if (pUserList.contains(nick))
    {
        //Nick matched with delimiters - set selection to nick
        selectedText = nick;
        mainChatTextEdit->setTextCursor(nickTextCursor);
    }*/
    else if (previouslySelectedText.isEmpty())
    {
        //No words matched and nothing was selected beforehand - just use the current word under the cursor
        selectedText = textCursor.selectedText();
    }
    else
        //No words matched but a previous selection exists - use that
        selectedText = previouslySelectedText;
    
    QMenu *standardMenu = mainChatTextEdit->createStandardContextMenu(pos);
    
    QAction *separatorAction = standardMenu->actions().first();

    bool addSeparator = false;

    //Add user command list
    if (!pUserCommands->isEmpty() && !selectedText.contains(" ") && !selectedText.isEmpty())
    {
        userCommandListMenu->clear();
        foreach (QString name, pUserCommands->keys())
        {
            QAction *action = new QAction(QIcon(":/ArpmanetDC/Resources/UserIcon.png"), name, userCommandListMenu);
            action->setData(selectedText);
            userCommandListMenu->addAction(action);
            userCommandListMenu->setTitle(tr("Use %1 in user command").arg(selectedText));
        }
        standardMenu->insertMenu(standardMenu->actions().first(), userCommandListMenu);
        addSeparator = true;
    }

    //Check if word is a user
    if (pUserList.contains(selectedText))
    {
        pmAction->setText(tr("Send PM to %1").arg(selectedText));
        pmAction->setData(selectedText);
        standardMenu->insertAction(standardMenu->actions().first(), pmAction);
        addSeparator = true;
    }

    if (addSeparator)
        standardMenu->insertSeparator(separatorAction);

    QPoint globalPos = mainChatTextEdit->viewport()->mapToGlobal(pos);

    standardMenu->popup(globalPos);
}

//Userlist keypresses
void ArpmanetDC::userListKeyPressed(Qt::Key key, QString keyStr)
{
    if (keyStr.isEmpty())
        return;

    QList<QModelIndex> matchList = userSortProxy->match(userSortProxy->index(0,2), Qt::DisplayRole, QVariant(keyStr));
 
    if (matchList.size() > 0)
    {
        QModelIndex index = matchList.first();
        userListTable->selectRow(index.row());

        //QList<QModelIndex> selectedList = userListTable->selectionModel()->selectedRows();
        userListTable->scrollTo(userSortProxy->index(index.row(), 0), QAbstractItemView::PositionAtCenter);
    }
}

//Chat line edit keypresses
void ArpmanetDC::chatLineEditKeyPressed(Qt::Key key, QString keyStr)
{
    if (key == Qt::Key_Tab)
    {
        //Get word we are cycling for
        QString word = pCurrentMatchChatString;
        QString chatWithoutWord = "";
        int index = pCurrentMatchChatString.lastIndexOf(" ");
        if (index >= 0)
        {
            //If a space was found -> get last word. Otherwise, use the whole text
            word = pCurrentMatchChatString.mid(index + 1);
            chatWithoutWord = pCurrentMatchChatString.left(index + 1);
        }

        //Respond to tab pressing
        if (tabCyclingIndex >= 0)
        {
            //Busy cycling through options
            if (!pCurrentMatchList.isEmpty())
            {
                if (++tabCyclingIndex == pCurrentMatchList.size())
                {
                    //Reset the counter if we've reached the end
                    tabCyclingIndex = 0;
                }

                //Set chat
                chatLineEdit->setText(chatWithoutWord + pCurrentMatchList.at(tabCyclingIndex));
            }
        }
        else
        {
            //First time we pressed tab
            pCurrentMatchList = nickMatchList(word);

            //We're using the first index
            tabCyclingIndex = 0;

            if (pCurrentMatchList.isEmpty())
            {
                tabCyclingIndex = -1;
                return;
            }

            //Set the appropriate chat
            chatLineEdit->setText(chatWithoutWord + pCurrentMatchList.first());
        }
    }
    else
    {
        //Save chat string for future use
        pCurrentMatchChatString = chatLineEdit->text();
        
        //Set that we're not busy cycling through options
        tabCyclingIndex = -1;
    }
}

//When a tab is deleted - free from memory
void ArpmanetDC::tabDeleted(int index)
{
    //Don't delete arpmanetDC tab
    if (index == 0)
        return;

    //Delete search tab
    if (searchWidgetHash.contains(tabs->widget(index)))
    {
        SearchWidget *widget = searchWidgetHash.value(tabs->widget(index));
        searchWidgetIDHash.remove(widget->id());
        widget->deleteLater();
        searchWidgetHash.remove(tabs->widget(index));
    }

    //Delete display container tab
    if (displayContainerWidgetHash.contains(tabs->widget(index)))
    {
        displayContainerWidgetHash.value(tabs->widget(index))->deleteLater();
        displayContainerWidgetHash.remove(tabs->widget(index));
    }

    //Delete PM tab
    if (pmWidgetHash.contains(tabs->widget(index)))
    {
        pmWidgetHash.value(tabs->widget(index))->deleteLater();
        pmWidgetHash.remove(tabs->widget(index));
    }

    //Delete share tab
    if (shareWidget)
    {
        if (shareWidget->widget() == tabs->widget(index))
        {
            shareWidget->deleteLater();
            shareWidget = 0;
        }
    }

    //Delete share tab
    if (queueWidget)
    {
        if (queueWidget->widget() == tabs->widget(index))
        {
            queueWidget->deleteLater();
            queueWidget = 0;
        }
    }

    //Delete share tab
    if (settingsWidget)
    {
        if (settingsWidget->widget() == tabs->widget(index))
        {
            settingsWidget->deleteLater();
            settingsWidget = 0;
        }
    }

    tabs->removeTab(index);
}

//Delete notifications when switching to that tab
void ArpmanetDC::tabChanged(int index)
{
    if (tabs->tabBar()->tabTextColor(index) == tabTextColorNotify)
    {
        if (pmWidgetHash.contains(tabs->widget(index)))
            updateNotify(--notifyCount);
        tabs->tabBar()->setTabTextColor(index, tabTextColorNormal);
    }
}

void ArpmanetDC::searchButtonPressed(quint64 id, QString searchStr, QByteArray searchPacket, SearchWidget *sWidget)
{
    //Search button was pressed on a search tab
    emit initiateSearch(id, searchPacket);

    //Replace the id
    searchWidgetIDHash.remove(searchWidgetIDHash.key(sWidget));
    searchWidgetIDHash.insert(id, sWidget);

    //pShare->querySearchString(QHostAddress("127.0.0.1"), QByteArray("ME!"), id, searchPacket);

    tabs->setTabText(tabs->indexOf(sWidget->widget()), tr("Search - %1").arg(searchStr.left(20)));

    //Add word to database and to searchWordList
    if (searchWordList->findItems(searchStr).isEmpty())
    {
        searchWordList->appendRow(new QStandardItem(searchStr.toLower()));
        searchWordList->sort(0);
    }
    emit saveAutoCompleteWordList(searchStr.toLower());
}

/*
//Received a search result from ShareSearch
void ArpmanetDC::ownResultReceived(quint64 id, QByteArray searchPacket)
{
   // if (searchWidgetIDHash.contains(id))
   //     searchWidgetIDHash.value(id)->addSearchResult(QHostAddress("127.0.0.1"), "ME!", searchPacket);
}
*/

//Received a search result from dispatcher
void ArpmanetDC::searchResultReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult)
{
    if (searchWidgetIDHash.contains(searchID))
        searchWidgetIDHash.value(searchID)->addSearchResult(senderHost, senderCID, searchResult);
}

void ArpmanetDC::returnHostCount(int count)
{
    CIDHostsLabel->setText(tr("%1").arg(count));
}

void ArpmanetDC::shareSaveButtonPressed()
{
    //Delete share tab
    if (shareWidget)
    {
        setStatus(tr("Share update procedure started. Parsing directories/paths..."));
        tabs->removeTab(tabs->indexOf(shareWidget->widget()));
        shareWidget->deleteLater();
        shareWidget = 0;
        //Show hashing progress
        hashingProgressBar->setTopText("Parsing");
        hashingProgressBar->setRange(0,0);
        hashRateTimer->start();
    }

    saveSharesPressed = true;
}

void ArpmanetDC::settingsSaved()
{
    //Save settings to database
    pSettingsManager->saveSettings();

    //Reconnect hub if necessary
    if (pSettingsManager->getSetting(SettingsManager::HUB_ADDRESS) != pHub->getHubAddress() || pSettingsManager->getSetting(SettingsManager::HUB_PORT) != pHub->getHubPort()
        || pSettingsManager->getSetting(SettingsManager::NICKNAME) != pHub->getNick() || pSettingsManager->getSetting(SettingsManager::PASSWORD) != pHub->getPassword())
        reconnectActionPressed();

    //Reconnect dispatcher if necessary
    QString externalIP = pSettingsManager->getSetting(SettingsManager::EXTERNAL_IP);
    quint16 externalPort = pSettingsManager->getSetting(SettingsManager::EXTERNAL_PORT);
    
    QHostAddress oldExternalIP;
    quint16 oldExternalPort;
    QMetaObject::invokeMethod(pDispatcher, "getDispatchIP", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QHostAddress, oldExternalIP));
    QMetaObject::invokeMethod(pDispatcher, "getDispatchPort", Qt::BlockingQueuedConnection, Q_RETURN_ARG(quint16, oldExternalPort));
    if (externalIP != oldExternalIP.toString() || externalPort != oldExternalPort)
        QMetaObject::invokeMethod(pDispatcher, "reconfigureDispatchHostPort", Qt::QueuedConnection, Q_ARG(QHostAddress, QHostAddress(externalIP)), Q_ARG(quint16, externalPort));

    //Reset CID from nick/password
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray().append(pSettingsManager->getSetting(SettingsManager::NICKNAME)));
    hash.addData(QByteArray().append(pSettingsManager->getSetting(SettingsManager::PASSWORD)));
    QByteArray cid = hash.result();
    QMetaObject::invokeMethod(pDispatcher, "setCID", Qt::QueuedConnection, Q_ARG(QByteArray, cid));

    //Reset the auto update timer if necessary
    int interval = pSettingsManager->getSetting(SettingsManager::AUTO_UPDATE_SHARE_INTERVAL);
    if (interval == 0) //Disabled
        updateSharesTimer->stop();
    else if (interval != updateSharesTimer->interval())
        updateSharesTimer->start(interval);

    //Reconfigure protocol preference order in transfer manager
    QMetaObject::invokeMethod(pTransferManager, "setProtocolOrderPreference", Qt::QueuedConnection, Q_ARG(QByteArray, pSettingsManager->getSetting(SettingsManager::PROTOCOL_HINT).toAscii()));

    //Delete settings tab
    if (settingsWidget)
    {
        setStatus(tr("Settings saved"));
        tabs->removeTab(tabs->indexOf(settingsWidget->widget()));
        settingsWidget->deleteLater();
        settingsWidget = 0;
    }
}

void ArpmanetDC::pmSent(QString otherNick, QString msg, PMWidget *pmWidget)
{
    //PM was sent in a tab
    pHub->sendPrivateMessage(otherNick, msg);
}

void ArpmanetDC::fileHashed(QString fileName, quint64 fileSize, quint64 totalShare)
{
    //Show on GUI when file has finished hashing
    setStatus(tr("Finished hashing file: %1").arg(fileName));
    shareSizeLabel->setText(tr("Share: %1").arg(bytesToSize(totalShare)));
   
    pFilesHashedSinceUpdate++;
    //pFileSizeHashedSinceUpdate += fileSize;

    pHub->setTotalShareSize(pFileSizeHashedSinceUpdate);
    //QApplication::processEvents();
}

void ArpmanetDC::directoryParsed(QString path)
{
    //Show on GUI when directory has been parsed
    setStatus(tr("Finished parsing directory/path: %1").arg(path));
    //QApplication::processEvents();
}

void ArpmanetDC::hashingDone(int msecs, int numFiles)
{
    QString timeStr = tr("%1 seconds").arg((double)msecs / 1000.0, 0, 'f', 2);

    //Show on GUI when hashing is completed
    hashRateTimer->stop();
    pFilesHashedSinceUpdate = numFiles;

    emit requestTotalShare(true);

    setStatus(tr("Shares updated in %1").arg(timeStr));
    hashingProgressBar->setRange(0,1);

    //Start processing containers if there are outstanding
    if (saveSharesPressed)
    {
        //Shares will be updated twice to account for the container being built after the first pass
        saveSharesPressed = false;
        emit saveContainers(pContainerHash, pContainerDirectory);
    }
}

void ArpmanetDC::parsingDone(int msecs)
{
    QString timeStr = tr("%1 seconds").arg((double)msecs / 1000.0, 0, 'f', 2);

    //Show on GUI when directory parsing is completed
    setStatus(tr("Finished directory/path parsing in %1. Checking for new/modified files...").arg(timeStr));
    hashingProgressBar->setTopText("Hashing");
}

//Slot called to return total share size from DB
void ArpmanetDC::returnTotalShare(quint64 size)
{
    //Set labels
    shareSizeLabel->setText(tr("Share: %1").arg(bytesToSize(size)));
    shareSizeLabel->setToolTip(tr("Share size: %1\nFiles shared: %2").arg(bytesToSize(size)).arg(pFilesHashedSinceUpdate));

    //Set hub share size
    pHub->setTotalShareSize(size);
}

void ArpmanetDC::downloadCompleted(QByteArray tth)
{
    //Remove download from queue
    QueueStruct file;
    if (pQueueList->contains(tth))
    {
        file = pQueueList->value(tth);
    }
    else
        return;

    if (queueWidget)
        queueWidget->removeQueuedDownload(tth);

    //Add file to finished downloads list if it's not a container
    if (!file.fileName.endsWith("." + QString(CONTAINER_EXTENSION)))
    {
        FinishedDownloadStruct item;
        item.fileName = file.fileName;
        item.filePath = file.filePath;
        item.fileSize = file.fileSize;
        item.tthRoot = file.tthRoot;
        item.downloadedDate = QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss:zzz");
   
        addFinishedDownloadToList(item);
    }
    else
    {
        //Insert the file into the hash for processing
        QueueStruct s(file);
        s.tthRoot = file.tthRoot;
        pContainerProcessHash.insert(file.filePath, s);

        //Check if already assembled
        if (pContainerAssembledHash.contains(file.filePath))
        {
            QString filePath = file.filePath;
            filePath.remove(file.fileName);
            emit processContainer(file.fileHost, file.filePath, filePath);

            //Remove container from hashes
            pContainerProcessHash.remove(file.filePath);
            pContainerAssembledHash.remove(file.filePath);
        }
    }
    
    //Delete from queue
    deleteFromQueue(tth);

    //Set status
    setStatus(tr("Download completed: %1").arg(file.fileName));
}

void ArpmanetDC::downloadStarted(QByteArray tth)
{
    if (pQueueList->contains(tth))
        setStatus(tr("Download started: %1").arg(pQueueList->value(tth).fileName));

    if (queueWidget)
        queueWidget->setQueuedDownloadBusy(tth);
}

//Requeue the download when it "failed"
void ArpmanetDC::requeueDownload(QByteArray tth)
{
    //Change the priority to low when requeued
    if (pQueueList->contains(tth))
    {
        (*pQueueList)[tth].priority = LowQueuePriority;
    }
    else
        return;

    //Notify the queueWidget
    if (queueWidget)
        queueWidget->requeueDownload(tth);
}

void ArpmanetDC::receivedPrivateMessage(QString otherNick, QString msg)
{
    //Check if a tab exists with a PM for this nick
    QWidget *foundWidget = 0;
    QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
    while (i.hasNext())
    {
        if (i.peekNext().value())
        {
            if (i.peekNext().value()->otherNick() == otherNick)
            {
                foundWidget = i.peekNext().key();
                break;
            }
        }
        i.next();
    }

    //If no existing tab is found - create new one
    if (!foundWidget)
    {
        PMWidget *pmWidget = new PMWidget(otherNick, this);
        connect(pmWidget, SIGNAL(sendPrivateMessage(QString, QString, PMWidget *)), this, SLOT(pmSent(QString, QString, PMWidget *)));
        pmWidgetHash.insert(pmWidget->widget(), pmWidget);

        tabs->addTab(pmWidget->widget(), QIcon(":/ArpmanetDC/Resources/UserIcon.png"), tr("PM - %1").arg(otherNick));
        
        //If on mainchat and not typing, switch to PM
        if (tabs->currentIndex() == 0 && chatLineEdit->text().isEmpty())
        {
            tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));
        }
        //Else notify tab
        else
        {
            //Don't notify if already in that tab
            if (tabs->currentIndex() != tabs->indexOf(pmWidget->widget()))
            {
                //Notify tab
                tabs->tabBar()->setTabTextColor(tabs->indexOf(pmWidget->widget()), tabTextColorNotify);
                updateNotify(++notifyCount);
            }
        }
        //tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));

        pmWidget->receivePrivateMessage(msg);
    }
    //Else notify existing tab
    else
    {
        //If on mainchat, switch to PM
        if (tabs->currentIndex() == 0 && chatLineEdit->text().isEmpty())
        {
            tabs->setCurrentIndex(tabs->indexOf(foundWidget));
        }
        //Else notify tab
        else
        {
            //Don't notify if already in that tab
            if (tabs->currentIndex() != tabs->indexOf(foundWidget))
            {
                //Notify tab
                tabs->tabBar()->setTabTextColor(tabs->indexOf(foundWidget), tabTextColorNotify);
                updateNotify(++notifyCount);
            }
        }
        //Process message
        pmWidgetHash.value(foundWidget)->receivePrivateMessage(msg);
    }
}

//FTP Update slots
void ArpmanetDC::ftpReturnUpdateResults(bool result, QString newVersion)
{
    if (helpWidget)
    {
        if (result)
            helpWidget->updateProgress(100,100);
        else
            helpWidget->updateProgress(-1,1);
    }

    //A new version exists
    if (result)
    {
        //Ask if the user wants to download it
        if (QMessageBox::information(this, tr("ArpmanetDC"), tr("A new version of the client has been released. Do you want to download ArpmanetDC v%1?").arg(newVersion),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
        {
            //Download new version
            emit ftpDownloadNewestVersion();
        }
    }
}

void ArpmanetDC::ftpDownloadCompleted(bool error)
{
    if (!error)
    {
        if (helpWidget)
            helpWidget->updateProgress(100,100);

        if (QMessageBox::information(this, tr("ArpmanetDC"), tr("Client download complete. Do you want to exit the client to run the new version?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
        {
            this->close();
        }
    }
}

void ArpmanetDC::ftpDataTransferProgress(qint64 done, qint64 total)
{
    //Report downloading progress if helpWidget is open
    if (helpWidget)
    {
        helpWidget->updateProgress(done, total);
    }
}

//Received a main chat message from the hub
void ArpmanetDC::appendChatLine(QString msg)
{
    //Change mainchat user nick format
    if (msg.left(4).compare("&lt;") == 0)
    {
        QString nick = msg.mid(4,msg.indexOf("&gt;")-4);
        msg.remove(0,msg.indexOf("&gt;")+4);

        //If using the /me command
        if (msg.left(5).compare(" /me ") == 0)
        {
            msg.remove(1,4);
            msg.prepend(tr("<i><b>* %1</b>").arg(nick));
            msg.append("</i>");
        }
        else
        {
            if (nick == "::error")
                msg = tr("<font color=\"red\"><b>%1</b></font>").arg(msg);
            else if (nick == "::info")
                msg = tr("<font color=\"green\"><b>%1</b></font>").arg(msg);
            else
                msg.prepend(tr("<b>%1</b>").arg(nick));
        }
    }

    //Replace new lines with <br/>
    msg.replace("\n"," <br/>");
    msg.replace("\r","");

    //===== Replace nicknames in order (highest order first) =====
    //Replace nick with red text
    convertNickname(pSettingsManager->getSetting(SettingsManager::NICKNAME), msg);
    
    //Replace OP names with green text
    convertOPName(msg);
    
    //Convert all remaining nicks
    //convertAllNicks(msg);

    //Convert plain text links to HTML links
    convertHTMLLinks(msg);

    //Convert plain text magnet links
    convertMagnetLinks(msg);

    //Delete the first line if buffer is full
    while (mainChatBlocks > pSettingsManager->getSetting(SettingsManager::MAX_MAINCHAT_BLOCKS))
    {
        QTextCursor tcOriginal = mainChatTextEdit->textCursor();
        QTextCursor tc = mainChatTextEdit->textCursor();
        tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tc.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        tc.removeSelectedText();
        tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                
        mainChatTextEdit->setTextCursor(tc);
        mainChatBlocks--;
    }

    //Output chat line with current time
    mainChatTextEdit->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(msg));
    mainChatBlocks++;

    //Replace text with emoticons
    insertEmoticonsInLastBlock();

    //If not on mainchat, notify tab
    if (tabs->currentIndex() != 0)
    {
        tabs->tabBar()->setTabTextColor(0, tabTextColorNotify);
    }
}

void ArpmanetDC::sortUserList()
{
    //Sort user list when timer has expired and sorting is required
    if (sortDue)
    {
        userSortProxy->sort(userListTable->horizontalHeader()->sortIndicatorSection(), userListTable->horizontalHeader()->sortIndicatorOrder());
        resizeRowsToContents(userListTable);
        sortDue = false;
    }
}

void ArpmanetDC::updateGUIEverySecond()
{
    //Called every second to update the GUI
    emit getHostCount();  

    //Calculate total share size
    totalShareSizeLabel->setText(tr("Total: %1").arg(bytesToSize(calculateTotalShareSize())));
}

//Calculate rates
void ArpmanetDC::calculateHashRate()
{
    QString rateMB = bytesToRate(pFileSizeHashedSinceUpdate);
    QString rateFiles = tr("%1 files/s").arg(pFilesHashedSinceUpdate);

    pFilesHashedSinceUpdate = 0;
    pFileSizeHashedSinceUpdate = 0;

    shareSizeLabel->setToolTip(tr("Hashing speed:\n%1\n%2").arg(rateMB).arg(rateFiles));
}

void ArpmanetDC::userListInfoReceived(QString nick, QString desc, QString mode, QString client, QString version, QString registerMode, quint16 openSlots, quint64 shareBytes)
{
    //Don't add empty nicknames
    if (nick.isEmpty())
        return;

    //Build new descriptions
    if (!mode.isEmpty())
        desc = tr("%1 %2").arg(client).arg(version);

    //Check if user is already present in the list
    //QList<QStandardItem *> foundItems = userListModel->findItems(nick, Qt::MatchExactly, 2);
    
    //Not present - create new
    //if (foundItems.isEmpty())
    if (!pUserList.contains(nick))
    {
        setAdditionalInfo(tr("User logged in: %1").arg(nick));

        //Add new row into table
        QList<QStandardItem *> row;
        //userListModel->appendRow(new QStandardItem());
        
        //Use different formats for Active/Passive users
        QStandardItem *item = new QStandardItem();
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        //Insert item into user list hash for fast retrieval later
        QPair<QStandardItem *, quint64> pair;
        pair.first = item;
        pair.second = shareBytes;
        pUserList.insert(nick, pair);

        QBrush brush = item->foreground();

        //----- Set name colours and nick sorting -----
        if (pOPList.contains(nick) || client.startsWith("Hub"))
        {
            //OP
            brush.setColor(Qt::darkGreen);
            item->setText(" " + nick);
        }
        else if (client.compare("ArpmanetDC") == 0)
        {
            //ArpmanetDC user
            if (!pADCUserList.contains(nick))
                pADCUserList.insert(nick); //Add user to ArpmanetDC set

            brush.setColor(Qt::black);
            item->setText(0xA0 + nick);
        }
        else
        {
            //Other client user
            brush.setColor(Qt::red);
            item->setText(0xA0 + nick);
        }
        item->setForeground(brush);

        //----- Set icons -----
        if (mode.compare("P") == 0)
        {
            //Passive user
            item->setIcon(QIcon(*userFirewallIcon));
        }
        else if (client.compare("ArpmanetDC") == 0)
        {
            //ArpmanetDC user
            if (version == VERSION_STRING)
                item->setIcon(QIcon(*arpmanetUserIcon)); //User has same version as you - normal icon
            else if (firstVersionLarger(VERSION_STRING, version))
                item->setIcon(QIcon(*oldVersionUserIcon)); //User has older version than you - skull icon
            else
                item->setIcon(QIcon(*newerVersionUserIcon)); //User has newer version than you - cool icon
        }
        else
        {
            //Active user - another client
            item->setIcon(QIcon(*userIcon));
        }

        //Add nick to model
        row.append(item);
        //userListModel->setItem(userListModel->rowCount()-1, 0, item);        

        //Add description to model
        QStandardItem *descItem = new QStandardItem(desc);
        descItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        //userListModel->setItem(userListModel->rowCount()-1, 1, descItem);
        row.append(descItem);

        //Add non displaying fields
        row.append(new QStandardItem(nick));
        row.append(new QStandardItem(desc));
        row.append(new QStandardItem(mode));
        row.append(new QStandardItem(client));
        row.append(new QStandardItem(version));
        //userListModel->setItem(userListModel->rowCount()-1, 2, new QStandardItem(nick));
        //userListModel->setItem(userListModel->rowCount()-1, 3, new QStandardItem(desc));
        //userListModel->setItem(userListModel->rowCount()-1, 4, new QStandardItem(mode));
        //userListModel->setItem(userListModel->rowCount()-1, 5, new QStandardItem(client));
        //userListModel->setItem(userListModel->rowCount()-1, 6, new QStandardItem(version));
        
        //Append row to model
        userListModel->appendRow(row);

        //Signal for sorting
        sortDue = true;

        //Update user count
        userHubCountLabel->setText(tr("Hub Users: %1/%2").arg(pADCUserList.size()).arg(pUserList.size()));

        //Check if PM windows are open for this user - notify
        QWidget *foundWidget = 0;
        QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
        while (i.hasNext())
        {
            if (i.peekNext().value())
            {
                if (i.peekNext().value()->otherNick() == nick)
                {
                    foundWidget = i.peekNext().key();
                    break;
                }
            }
            i.next();
        }

        //Notify existing tab
        if (foundWidget)
        {
            //Notify tab
            tabs->tabBar()->setTabTextColor(tabs->indexOf(foundWidget), tabTextColorNormal);

            //Set online status
            pmWidgetHash.value(foundWidget)->userLoginChanged(true);
        }
    }
    //Present - edit existing
    else
    {
        //Get first match (there should be only one)
        QStandardItem *item = pUserList.value(nick).first;
        int foundIndex = item->row();//foundItems.first()->index().row();
        //QStandardItem *item = userListModel->item(foundIndex, 0);

        //Set share size
        pUserList[nick].second = shareBytes;

        QBrush brush = userListModel->item(foundIndex,0)->foreground();

        //----- Set name colours and nick sorting -----
        if (pOPList.contains(nick) || client.startsWith("Hub"))
        {
            //OP
            brush.setColor(Qt::darkGreen);
            item->setText(" " + nick);
        }
        else if (client.compare("ArpmanetDC") == 0)
        {
            //ArpmanetDC user
            arpmanetDCUsers++; //Increase user count

            brush.setColor(Qt::black);
            item->setText(0xA0 + nick);
        }
        else
        {
            //Other client user
            brush.setColor(Qt::red);
            item->setText(0xA0 + nick);
        }
        item->setForeground(brush);

        //----- Set icons -----
        if (mode.compare("P") == 0)
        {
            //Passive user
            item->setIcon(QIcon(*userFirewallIcon));
        }
        else if (client.compare("ArpmanetDC") == 0)
        {
            //ArpmanetDC user
            if (version == VERSION_STRING)
                item->setIcon(QIcon(*arpmanetUserIcon)); //User has same version as you - normal icon
            else if (firstVersionLarger(VERSION_STRING, version))
                item->setIcon(QIcon(*oldVersionUserIcon)); //User has older version than you - skull icon
            else
                item->setIcon(QIcon(*newerVersionUserIcon)); //User has newer version than you - cool icon
        }
        else
        {
            //Active user - another client
            item->setIcon(QIcon(*userIcon));
        }
                
        if (!desc.isEmpty())
        {
            //Edit description
            userListModel->item(foundIndex, 1)->setText(desc);
            userListModel->item(foundIndex, 1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            //Edit non displaying field
            userListModel->item(foundIndex, 3)->setText(desc);
        }

        if (!mode.isEmpty())
            userListModel->item(foundIndex, 4)->setText(mode);

        if (!client.isEmpty())
            userListModel->item(foundIndex, 5)->setText(client);

        if (!version.isEmpty())
            userListModel->item(foundIndex, 6)->setText(version);
        
        //Signal for sorting
        sortDue = true;
    }
}

void ArpmanetDC::userListUserLoggedOut(QString nick)
{
    //Find the items matching the nickname in the model
    //QList<QStandardItem *> items = userListModel->findItems(nick, Qt::MatchFixedString, 2);
        
    //if (items.isEmpty())
    if (!pUserList.contains(nick))
        return;

    QStandardItem *item = pUserList.value(nick).first;

    //Remove nick from ArpmanetDC list if necessary
    //if (userListModel->item(items.first()->row(), 5)->text() == "ArpmanetDC")
    if (pADCUserList.contains(nick))
        pADCUserList.remove(nick);

    //Remove nick from userlist
    pUserList.remove(nick);

    //Remove the user from the list
    //if (userListModel->removeRows(items.first()->index().row(), 1))
    if (userListModel->removeRows(item->row(), 1))
        setAdditionalInfo(tr("User logged out: %1").arg(nick));
    else
        setAdditionalInfo(tr("User not in list: %1").arg(nick));

    //Update user count
    userHubCountLabel->setText(tr("Hub Users: %1/%2").arg(pADCUserList.size()).arg(pUserList.size()));

    //Check if a tab exists with a PM for this nick
    QWidget *foundWidget = 0;
    QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
    while (i.hasNext())
    {
        if (i.peekNext().value())
        {
            if (i.peekNext().value()->otherNick() == nick)
            {
                foundWidget = i.peekNext().key();
                break;
            }
        }
        i.next();
    }

    //Notify existing tab
    if (foundWidget)
    {
        //Notify tab
        tabs->tabBar()->setTabTextColor(tabs->indexOf(foundWidget), tabTextColorOffline);

        //Set offline status
        pmWidgetHash.value(foundWidget)->userLoginChanged(false);
    }
}

void ArpmanetDC::userListNickListReceived(QStringList list)
{
    //Add/update user list for every nick in nickList
    foreach (QString nick, list)
        userListInfoReceived(nick, "", "", "", "", "", 0, 0);
}

void ArpmanetDC::opListReceived(QStringList list)
{
    //Keep record of all the OPs in the hub to show them in another colour
    pOPList = list.toSet();

    foreach (QString nick, pOPList)
    {
        //Check if user is already present in the list - otherwise the myINFO section will deal with it
        //QList<QStandardItem *> foundItems = userListModel->findItems(nick, Qt::MatchExactly, 2);
        if (!pUserList.contains(nick))
            continue;
        
        //Get item in first column
        QStandardItem *item = pUserList.value(nick).first;

        //Set colour and nick sort type
        QBrush brush = item->foreground();
        brush.setColor(Qt::darkGreen);
        item->setForeground(brush);
        item->setText(" " + nick);
    }
}

void ArpmanetDC::hubOffline()
{
    tabs->setTabIcon(0, QIcon(":/ArpmanetDC/Resources/ServerOfflineIcon.png"));
    setStatus("ArpmanetDC hub went offline");

    //Clear model
    userListModel->removeRows(0, userListModel->rowCount());

    //Clear user list, op list and ArpmanetDC user list
    pUserList.clear();
    pOPList.clear();
    pADCUserList.clear();

    //Mark all PM windows offline
    QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
    while (i.hasNext())
    {
        i.next();

        //Notify tabs
        tabs->tabBar()->setTabTextColor(tabs->indexOf(i.key()), tabTextColorOffline);

        //Tell PM window user is offline
        i.value()->userLoginChanged(false);
    }
}

void ArpmanetDC::hubOnline()
{
    tabs->setTabIcon(0, QIcon(":/ArpmanetDC/Resources/ServerIcon.png"));
    setStatus("ArpmanetDC hub online");
}

void ArpmanetDC::bootstrapStatusChanged(int status)
{
    // bootstrapStatus
    // -2: Absoluut clueless en nowhere
    // -1: Besig om multicast bootstrap te probeer
    //  0: Besig om broadcast bootstrap te probeer
    //  1: Aanvaar broadcast, geen ander nodes naby, het bucket ID gegenereer.
    //  2: Suksesvol gebootstrap op broadcast
    //  3: Suksesvol gebootstrap op multicast

    bootstrapStatusLabel->setText(tr("%1").arg(status));
    
    //If not bootstrapped yet
    if (status <= 0)
    {
        connectionIconLabel->setPixmap(*unbootstrappedIcon);
        connectionIconLabel->setToolTip(tr("Bootstrapping..."));
    }
    //No other nodes nearby, waiting
    else if (status == 1)
    {
        connectionIconLabel->setPixmap(*bootstrappedIcon);
        connectionIconLabel->setToolTip(tr("Waiting for nodes"));
    }
    //If bootstrapped in either broadcast/multicast
    else if (status == 2)
    {
        connectionIconLabel->setPixmap(*fullyBootstrappedIcon);
        connectionIconLabel->setToolTip(tr("Broadcast bootstrapped"));
    }
    else if (status == 3)
    {
        connectionIconLabel->setPixmap(*fullyBootstrappedIcon);
        connectionIconLabel->setToolTip(tr("Multicast bootstrapped"));
    }

    bootstrapStatusLabel->setToolTip(connectionIconLabel->toolTip());
}

void ArpmanetDC::setStatus(QString msg)
{
    //Save history
    pStatusHistoryList->append(tr("[%1]: %2").arg(QTime::currentTime().toString()).arg(msg));
    if (pStatusHistoryList->size() > pSettingsManager->getSetting(SettingsManager::MAX_LABEL_HISTORY_ENTRIES))
        pStatusHistoryList->removeFirst();

    QString history;
    for (int i = 0; i < pStatusHistoryList->size(); i++)
    {
        if (!history.isEmpty())
            history += "\n";
        history.append(pStatusHistoryList->at(i));
    }
   
    statusLabel->setToolTip(history);
    
    //Elide text to avoid stupid looking shifting of permanent widgets in the status bar
    QFontMetrics fm(statusLabel->font());
    statusLabel->setText(fm.elidedText(msg, Qt::ElideMiddle, statusLabel->width()));
}

//Sets the additional info label to msg
void ArpmanetDC::setAdditionalInfo(QString msg)
{
    //Save history
    pAdditionalInfoHistoryList->append(tr("[%1]: %2").arg(QTime::currentTime().toString()).arg(msg));
    if (pAdditionalInfoHistoryList->size() > pSettingsManager->getSetting(SettingsManager::MAX_LABEL_HISTORY_ENTRIES))
        pAdditionalInfoHistoryList->removeFirst();

    QString history;
    for (int i = 0; i < pAdditionalInfoHistoryList->size(); i++)
    {
        if (!history.isEmpty())
            history += "\n";
        history.append(pAdditionalInfoHistoryList->at(i));
    }
   
    additionalInfoLabel->setToolTip(history);
    
    //Elide text to avoid stupid looking shifting of permanent widgets in the status bar
    QFontMetrics fm(additionalInfoLabel->font());
    additionalInfoLabel->setText(fm.elidedText(msg, Qt::ElideMiddle, additionalInfoLabel->width()));
}

//Get the current hashing progress
void ArpmanetDC::hashingProgress(qint64 bytesThisSecond, qint64 fileProgressBytes, qint64 fileSize)
{
    pFileSizeHashedSinceUpdate += bytesThisSecond;
}

//Get the queue from the database
void ArpmanetDC::returnQueueList(QHash<QByteArray, QueueStruct> *queue)
{
    if (pQueueList)
        delete pQueueList;
    pQueueList = queue;

    //Readd the queue to transfermanager
    QString finalPath;
    foreach (QueueStruct item, *pQueueList)
    {
        if (item.priority == HighQueuePriority)
        {
            finalPath = item.filePath;
            emit queueDownload((int)item.priority, item.tthRoot, finalPath, item.fileSize, item.fileHost);
        }
    }
    foreach (QueueStruct item, *pQueueList)
    {
        if (item.priority == NormalQueuePriority)
        {
            finalPath = item.filePath;
            emit queueDownload((int)item.priority, item.tthRoot, finalPath, item.fileSize, item.fileHost);
        }
    }
    foreach (QueueStruct item, *pQueueList)
    {
        if (item.priority == LowQueuePriority)
        {
            finalPath = item.filePath;
            emit queueDownload((int)item.priority, item.tthRoot, finalPath, item.fileSize, item.fileHost);
        }
    }

    setStatus(tr("Download queue loaded"));
}

//Add a download to the queue
void ArpmanetDC::addDownloadToQueue(QueueStruct item)
{
    if (!pQueueList->contains(item.tthRoot))
    {
        //Insert into global queue structure
        pQueueList->insert(item.tthRoot, item);
        
        //Insert into database
        emit saveQueuedDownload(item);

        //Insert into queueWidget
        if (queueWidget)
        {
            //Check if queueWidget is open
            if (tabs->indexOf(queueWidget->widget()) != -1)
                queueWidget->addQueuedDownload(item);
        }
    }
}

//Remove download from the queue
void ArpmanetDC::deleteFromQueue(QByteArray tth)
{
    if (pQueueList->contains(tth))
    {
        //Remove from transfer manager queue
        emit removeQueuedDownload((int)pQueueList->value(tth).priority, tth);

        //Remove from queue
        pQueueList->remove(tth);

        //Remove from transfer widget list
        transferWidget->removeTransferEntry(tth, TRANSFER_TYPE_DOWNLOAD);

        //Remove from database
        emit removeQueuedDownload(tth);

        //Delete the state bitmap of this download from the database
        emit deleteBucketFlushStateBitmap(tth);
    }
}

//Change queue item priority
void ArpmanetDC::setQueuePriority(QByteArray tth, QueuePriority priority)
{
    if (pQueueList->contains(tth))
    {
        QueueStruct s = pQueueList->take(tth);

        //Set transfer manager priority
        emit changeQueuedDownloadPriority((int)s.priority, (int)priority, s.tthRoot);

        //Set priority in queue
        s.priority = priority;
        pQueueList->insert(tth, s);  

        //Set priority in database
        emit setQueuedDownloadPriority(tth, priority);
    }
}

//Add a finished download to the finished list
void ArpmanetDC::addFinishedDownloadToList(FinishedDownloadStruct item)
{
    if (!pFinishedList->contains(item.tthRoot))
    {
        //Insert into global queue structure
        pFinishedList->insert(item.tthRoot, item);
        
        //Insert into database
        emit saveFinishedDownload(item);

        //Insert into finishedWidget
        if (finishedWidget)
        {
            //Check if finishedWidget is open
            if (tabs->indexOf(finishedWidget->widget()) != -1)
                finishedWidget->addFinishedDownload(item);
        }
    }
}

//Remove a finished download
void ArpmanetDC::deleteFinishedDownload(QByteArray tth)
{
    if (pFinishedList->contains(tth))
    {
        //Delete from list
        pFinishedList->remove(tth);

        //Delete from database
        emit removeFinishedDownload(tth);
    }
}

//Remove all downloads from the list
void ArpmanetDC::clearFinishedDownloadList()
{
    //Clear list
    pFinishedList->clear();

    //Signal the database to clear everything
    emit clearFinishedDownloads();
}

//Returns the finished download list
void ArpmanetDC::returnFinishedList(QHash<QByteArray, FinishedDownloadStruct> *list)
{
    if (pFinishedList)
        delete pFinishedList;
    pFinishedList = list;

    setStatus(tr("Finished downloads list loaded"));
}

//Stops a transfer
void ArpmanetDC::removeTransfer(QByteArray tth, int transferType, QHostAddress hostAddr)
{
    deleteFromQueue(tth);

    emit stopTransfer(tth, transferType, hostAddr);

    if (queueWidget)
        queueWidget->removeQueuedDownload(tth);
}

//Queue the containers for saving after shares have been updated
void ArpmanetDC::queueSaveContainers(QHash<QString, ContainerContentsType> containerHash, QString containerDirectory)
{
    //Save container info
    pContainerHash = containerHash;
    pContainerDirectory = containerDirectory;
}

//Return the contents of a container downloaded
void ArpmanetDC::returnProcessedContainer(QHostAddress host, ContainerContentsType index, QList<ContainerLookupReturnStruct> data, QString downloadPath, QString containerName)
{
    //Create a display container widget and connect it
    DisplayContainerWidget *cWidget = new DisplayContainerWidget(host, index, data, containerName, downloadPath, this);
    connect(cWidget, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), this, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)));

    //Keep track of it
    displayContainerWidgetHash.insert(cWidget->widget(), cWidget);
    
    //Create a tab for it
    tabs->addTab(cWidget->widget(), QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), tr("Container - %1").arg(containerName));
    tabs->setCurrentIndex(tabs->indexOf(cWidget->widget()));
}

//Called when a file has been assembled correctly
void ArpmanetDC::fileAssemblyComplete(QString fileName)
{
    //Process containers
    if (fileName.endsWith("." + QString(CONTAINER_EXTENSION)))
    {
        //A step to avoid containers within containers for now... mostly since a container doesn't include a host to download from.
        //At least I don't think an infinite loop of containers can be built since the hashes for the container inside will differ from the one pointing to its own directory
        //TODO: post information along with buckets to have access to the host so containers within containers can be built
        if (pContainerProcessHash.contains(fileName))
        {
            QueueStruct file = pContainerProcessHash.value(fileName);
            QString filePath = file.filePath;
            filePath.remove(file.fileName);
            emit processContainer(file.fileHost, file.filePath/* + file.fileName*/, filePath);

            //Remove container from hashes
            pContainerProcessHash.remove(file.filePath);
            pContainerAssembledHash.remove(file.filePath);
        }
        else
            //If not completed yet, add to assembled hash for later processing
            pContainerAssembledHash.insert(fileName);
    }

}

//Called to set user commands
void ArpmanetDC::returnUserCommands(QHash<QString, UserCommandStruct> *commands)
{
    setUserCommands(commands);

    if (settingsWidget)
        settingsWidget->returnUserCommands(commands);
}

//System tray was activated
void ArpmanetDC::systemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick && restoreAction->isEnabled())
        restoreAction->trigger();
}

//Auto complete word list loaded from databases
void ArpmanetDC::searchWordListReceived(QStandardItemModel *wordList)
{
    setStatus(tr("Autocomplete list loaded"));
    //searchWordList = wordList;
    //searchCompleter->setModel(wordList);
    searchCompleter->completionPrefix();
}

void ArpmanetDC::mainChatLinkClicked(const QUrl &link)
{
    //Process internal links
    QString scheme = link.scheme();
    if (scheme == "magnet")
    {
        //Magnet link
        QString strLink = link.toString();

        QString type = tr("urn:tree:tiger:");
        int pos = strLink.indexOf(type);
        QString tth = strLink.mid(pos + type.size(), 39);

        //Search for tth
        SearchWidget *sWidget = new SearchWidget(searchCompleter, pTypeIconList, pTransferManager, tth, this);
 
        connect(sWidget, SIGNAL(search(quint64, QString, QByteArray, SearchWidget *)), this, SLOT(searchButtonPressed(quint64, QString, QByteArray, SearchWidget *)));
        connect(sWidget, SIGNAL(queueDownload(int, QByteArray, QString, quint64, QHostAddress)), pTransferManager, SLOT(queueDownload(int, QByteArray, QString, quint64, QHostAddress)));

        searchWidgetHash.insert(sWidget->widget(), sWidget);
        searchWidgetIDHash.insert(sWidget->id(), sWidget);

        tabs->addTab(sWidget->widget(), QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"));

        tabs->setCurrentIndex(tabs->indexOf(sWidget->widget()));

        //Wait for widget to open
        QApplication::processEvents();

        //Search
        sWidget->searchPressed();
    }
    else
        QDesktopServices::openUrl(link);
}

QString ArpmanetDC::processUserCommand(QString command)
{
    if (command.isEmpty())
        return command;

    if (pUserCommands->isEmpty())
        return command;

    //Delete dual forward slashes (//)
    command.remove(0,2);

    //Split command into command string and parameters
    QStringList commandList = command.split(" ");
    
    QHashIterator<QString, UserCommandStruct> i(*pUserCommands);
    while (i.hasNext())
    {
        i.next();
        //Match the command with the commands from the list
        if (i.value().command == commandList.first())
        {
            //Found command - now replace variables
            QString outputStr = i.value().output;
            if (i.value().parameterCount > 0)
            {
                //Iterate and fill parameter spaces
                for (int k = 0; k < i.value().parameterCount; ++k)
                {
                    if (commandList.size() > k+1)
                        outputStr.replace(tr("%%1").arg(k+1), commandList.at(k+1));
                    else
                        outputStr.replace(tr("%%1").arg(k+1), "");
                }
            }
            return outputStr;
        }
    }

    //If command was not found
    return command;
}

void ArpmanetDC::convertNickname(QString nick, QString &msg)
{
    //Replace nick with coloured text
    int currentIndex = 0;
    QString regex = tr("(%1)").arg(QRegExp::escape(nick));
    QRegExp rx(regex, Qt::CaseInsensitive);

    int pos = 0;
    //Check for regex's and replace
    while ((pos = rx.indexIn(msg, pos)) != -1)
    {
        //Extract link
        QString nickExtracted = rx.cap(1);
        
        //Construct coloured nick string
        QString nickColoured = tr("<font color=\"red\">%1</font>").arg(nickExtracted);

        //Replace link
        msg.remove(pos, nickExtracted.size());
        msg.insert(pos, nickColoured);

        pos += nickColoured.size();
    }
}

void ArpmanetDC::convertOPName(QString &msg)
{
    //Replace OP names with coloured text
    QString regex;
    QRegExp rx(regex, Qt::CaseInsensitive);

    foreach (QString nick, pOPList)
    {
        regex = tr("(%1)(?!</font>)").arg(QRegExp::escape(nick));
        rx.setPattern(regex);

        int pos = 0;
        //Check for regex's and replace
        while ((pos = rx.indexIn(msg, pos)) != -1)
        {
            //Extract link
            QString nickExtracted = rx.cap(1);
        
            //Construct coloured OP name string
            QString nickColoured = tr("<font color=\"green\">%1</font>").arg(nickExtracted);

            //Replace link
            msg.remove(pos, nickExtracted.size());
            msg.insert(pos, nickColoured);

            pos += nickColoured.size();
        }
    }
}

void ArpmanetDC::convertAllNicks(QString &msg)
{
    //Replace all nicks with escaped versions
    QString regex;
    QRegExp rx(regex, Qt::CaseInsensitive);

    QHashIterator<QString, QPair<QStandardItem *, quint64> > i(pUserList);
    while (i.hasNext())
    {
        i.next();

        //Match nicknames that don't have a colour specified yet
        regex = tr("(%1)(?!</font>)").arg(QRegExp::escape(i.key()));
        rx.setPattern(regex);

        int pos = 0;
        //Check for regex's and replace
        while ((pos = rx.indexIn(msg, pos)) != -1)
        {
            //Extract link
            QString nickExtracted = rx.cap(1);
        
            //Construct coloured OP name string
            QString nickColoured = tr("<font color=\"black\">%1</font>").arg(nickExtracted);

            //Replace link
            msg.remove(pos, nickExtracted.size());
            msg.insert(pos, nickColoured);

            pos += nickColoured.size();
        }
    }

}

void ArpmanetDC::convertHTMLLinks(QString &msg)
{
    //Replace html links with hrefs
    int currentIndex = 0;
    QString regex = "(href\\s*=\\s*['\"]?)?((www\\.|(http|https|ftp|news|file)+\\:\\/\\/)[&#95;._a-z0-9\\-]+\\.[a-z0-9\\/&#95;:@=.+?,##%&!~\\-_]*[^.|\\'|\\# |!|\\(|?|,| |>|<|;|\\)])";
    QRegExp rx(regex, Qt::CaseInsensitive);
    
    int pos = 0;
    //Check for regex's and replace
    while ((pos = rx.indexIn(msg, pos)) != -1)
    {
        //Extract link
        QString link = msg.mid(pos, rx.matchedLength());
        if (link.left(5).compare("href=") == 0)
        {
            //Skip if already an href
            pos += 2*link.size();
            continue;
        }

        //Construct href
        QString hrefLink = tr("<a href=\"%1\">%1</a>").arg(link);

        //Replace link
        msg.remove(pos, link.size());
        msg.insert(pos, hrefLink);

        //Replace www. links with http://www
        msg.replace("href=\"www", "href=\"http://www");
        pos += hrefLink.size();
    }
}

void ArpmanetDC::convertMagnetLinks(QString &msg)
{
    //Replace magnet links with hrefs
    int currentIndex = 0;
    QString regex = "(magnet:\\?xt\\=urn:(?:tree:tiger|sha1):([a-z0-9]{32,39})([a-z0-9\\/&#95;:@=.+?,##%&~\\-_(!)'\\[\\]]*))";
    QRegExp rx(regex, Qt::CaseInsensitive);

    int pos = 0;
    //Check for regex's and replace
    while ((pos = rx.indexIn(msg, pos)) != -1)
    {
        //Extract link
        QString link = rx.cap(1);
        QString hash = rx.cap(2);
        QString extra = rx.cap(3);

        QString size, fileName;
        QString sizeStr;

        //Parse filename and size
        if (!extra.isEmpty())
        {
            //Parse size
            QString sizeRegEx = "&xl=([\\d]+)";
            QRegExp sRx(sizeRegEx, Qt::CaseInsensitive);

            int posS = 0;
            if ((posS = sRx.indexIn(extra, posS)) != -1)
                size = sRx.cap(1);

            //Convert to correct unit
            double sizeInt = size.toDouble();
            sizeStr = bytesToSize(size.toULongLong());

            //Parse filename
            //QString fileNameRegEx = "&dn=([a-z0-9+\\-_.\\[\\]()']*(?:[\\.]+[a-z0-9+\\-_!\\[\\]]*))";
            QString fileNameRegEx = "&dn=([a-z0-9\\/&#95;:@=.+?,##%&~\\-_(!)'\\[\\]]*(?:[\\.]+[a-z0-9\\/&#95;:@=.+?,##%&~\\-_(!)'\\[\\]]*))";
            QRegExp fRx(fileNameRegEx, Qt::CaseInsensitive);

            //Replace +'s with spaces
            if ((posS = fRx.indexIn(extra, posS)) != -1)
                fileName = fRx.cap(1).replace("+", " ");
        }

        if (link.left(5).compare("href=") == 0)
        {
            //Skip if already an href
            pos += 2*link.size();
            continue;
        }

        //Construct href
        QString hrefLink = tr("<a href=\"%1\">%2 (%3)</a>").arg(link).arg(fileName).arg(sizeStr);

        //Replace link
        msg.remove(pos, link.size());
        msg.insert(pos, hrefLink);

        pos += hrefLink.size();
    }
}

//Replace text emoticons with images
void ArpmanetDC::insertEmoticonsInLastBlock()
{
    //Iterate through the last block
    QTextCursor originalTextCursor = mainChatTextEdit->textCursor();
    originalTextCursor.movePosition(QTextCursor::End);
    originalTextCursor.movePosition(QTextCursor::StartOfBlock);
    
    foreach (QString entry, pEmoticonList)
    {
        QTextCursor matchedCursor;
        while (!(matchedCursor = mainChatTextEdit->document()->find(entry, originalTextCursor)).isNull())
        {
            //Remove the text emoticon
            matchedCursor.removeSelectedText();

            //Insert a space
            matchedCursor.insertText(" ");
                                    
            //Insert image at text cursor location
            matchedCursor.insertImage(pEmoticonResourceList->getPixmapFromName(entry).toImage(), entry);
        }
    }
}

//Load emoticons into icon database
void ArpmanetDC::loadEmoticonsFromFile()
{
    QString path = tr(":/ArpmanetDC/Resources/Emoticons.png");
    pEmoticonList.clear();
    pEmoticonList << ":(" << " :crazy:" << " ;)" << " :)" << " :D" << " :wut:" << " 8)" << " x|" << " :-)" << " :!:" << " o)" << " :geek:" << " ^^" << " :love:" << " xD" << "";
    pEmoticonList << " :o" << " 8|" << "" << " :p" << "" << " :gross:" << " :-)" << "" << " :shutup:" << " ;D" << " :ninja:" << "" << "" << "";

    pEmoticonResourceList = new ResourceExtractor(path, pEmoticonList, 18, this);
}

//Get nick match list
QStringList ArpmanetDC::nickMatchList(QString partialNick)
{
    QMap<QString, QString> sortingMap;
    QStringList matchesUpper;

    //Iterate through all nicks and match
    QHashIterator<QString, QPair<QStandardItem *, quint64> > i(pUserList);
    while (i.hasNext())
    {
        i.next();
        if (i.key().toUpper().startsWith(partialNick.toUpper()))
        {
            sortingMap.insert(i.key().toUpper(), i.key());
            matchesUpper.append(i.key().toUpper());
        }
    }

    //Sort case insensitively
    matchesUpper.sort();

    //Map back into string list
    QStringList matchesSorted;
    foreach (QString upperStr, matchesUpper)
        matchesSorted.append(sortingMap.value(upperStr));

    return matchesSorted;
}

QString ArpmanetDC::getWinampSongTitle()
{
#ifdef Q_OS_WIN
    //Get Winamp song title
    HWND hwndWinamp = FindWindow(L"Winamp v1.x", NULL);
    LPWSTR windowTextLPWSTR = new WCHAR[200];
    char *windowTextCharPtr = new char[200];
    int length = GetWindowText(hwndWinamp, windowTextLPWSTR, 200);
    size_t count = wcstombs(windowTextCharPtr, windowTextLPWSTR, 200);
    QByteArray windowTextBA(windowTextCharPtr, count);
    QString windowText(windowTextBA.data());
    delete [] windowTextLPWSTR;
    delete [] windowTextCharPtr;
    QString separatorStr("*** ");
    if (windowText.contains(separatorStr)) //Scrolling
        windowText = windowText.mid(windowText.indexOf(separatorStr)+separatorStr.size());
    QString regex = "\\d+\\.(.*)\\s\\-\\sWinamp";
    QString regex2 = "(.*)\\s\\-\\sWinamp";
    QRegExp regExp(regex, Qt::CaseInsensitive);
    if (regExp.indexIn(windowText) >= 0)
        return regExp.cap(1);
    else
    {
        regExp.setPattern(regex2);
        if (regExp.indexIn(windowText) >= 0)
            return regExp.cap(1);
        else
            return "";
    }
#else
    return "";
#endif
}

QHostAddress ArpmanetDC::getIPGuess()
{
    //For jokes, get the actual IP of the computer and use the first one for the dispatcher
    //QList<QHostAddress> ips;
    //QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    foreach (QNetworkInterface interf, QNetworkInterface::allInterfaces())
    {
        //Only check the online interfaces capable of broadcasting that's not the local loopback
        if (interf.flags().testFlag(QNetworkInterface::IsRunning) 
            && interf.flags().testFlag(QNetworkInterface::CanBroadcast) 
            && !interf.flags().testFlag(QNetworkInterface::IsLoopBack)
            && interf.flags().testFlag(QNetworkInterface::IsUp))
        {
            foreach (QNetworkAddressEntry entry, interf.addressEntries())
            {
                //Only add IPv4 addresses
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                {
                    //ips.append(entry.ip());
                    return entry.ip();
                }
            }
        }
    }

    return QHostAddress();
}

QString ArpmanetDC::getDefaultDownloadPath()
{
    return QDir::homePath();
}

//Calculate total share size
quint64 ArpmanetDC::calculateTotalShareSize()
{
    quint64 totalShare = 0;
    QHashIterator<QString, QPair<QStandardItem *, quint64> > i(pUserList);
    while (i.hasNext())
    {
        i.next();
        totalShare += i.value().second;
    }
    return totalShare;
}

//Used to determine if an icon change should be done for notifications
void ArpmanetDC::updateNotify(int count)
{
    if (notifyCount == 0)
    {
        //No PM tabs are set to notify
        setWindowIcon(QIcon(*arpmanetDCLogoNormal));
        systemTrayIcon->setIcon(QIcon(*arpmanetDCLogoNormal));
    }
    else if (notifyCount == 1)
    {
        //There are still PM notifications
        setWindowIcon(QIcon(*arpmanetDCLogoNotify));
        systemTrayIcon->setIcon(QIcon(*arpmanetDCLogoNotify));
    }
}

//User commands
QHash<QString, UserCommandStruct> *ArpmanetDC::userCommands()
{
    return pUserCommands;
}

void ArpmanetDC::setUserCommands(QHash<QString, UserCommandStruct> *commands)
{
    if (!commands)
        return;

    //Set the data without changing the data address as the pointer is shared amount widgets
    (*pUserCommands) = *commands;
}

sqlite3 *ArpmanetDC::database() const
{
    return db;
}

QueueStruct ArpmanetDC::queueEntry(QByteArray tth)
{
    if (pQueueList->contains(tth))
        return pQueueList->value(tth);
    QueueStruct q;
    q.fileSize = 0;
    return q;
}

TransferManager *ArpmanetDC::transferManagerObject() const
{
    return pTransferManager;
}

ShareSearch *ArpmanetDC::shareSearchObject() const
{
    return pShare;
}

Dispatcher *ArpmanetDC::dispatcherObject() const
{
    return pDispatcher;
}

TransferWidget *ArpmanetDC::transferWidgetObject() const
{
    return transferWidget;
}

ResourceExtractor *ArpmanetDC::resourceExtractorObject() const
{
    return pTypeIconList;
}

//Get the bootstrap node number
quint32 ArpmanetDC::getBootstrapNodeNumber()
{
    if (CIDHostsLabel->text().isEmpty())
        return 0;
    else
        return CIDHostsLabel->text().toUInt();
}

quint32 ArpmanetDC::getBoostrapStatus()
{
    if (bootstrapStatusLabel->text().isEmpty())
        return 0;
    else
        return bootstrapStatusLabel->text().toUInt();
}

QSize ArpmanetDC::sizeHint() const
{
    //Check size of primary screen
    QRect screenSize = QApplication::desktop()->screenGeometry(QApplication::desktop()->primaryScreen());
    
    //Return an appropriate size for the screen size
    if (screenSize.width() >= 1280)
    {
        if (screenSize.height() >= 1024) //SXVGA or larger (1280x1024)
            return QSize(1200, 800); 
        else if (screenSize.height() >= 768) //WXGA (1280x768)
            return QSize(1200, 750);
    }
    else if (screenSize.width() >= 1024)
    {
        if (screenSize.height() >= 768)
            return QSize(1000, 750); //XGA (1024x768)
        else if (screenSize.height() >= 600)
            return QSize(1000, 550); //WSVGA (1024x600)
    }
    else if (screenSize.width() >= 800)
    {
        if (screenSize.height() >= 600)
            return QSize(750, 550); //SVGA (800x600)
        else if (screenSize.height() >= 480)
            return QSize(750, 450); //WVGA (854x480 or 800x480)
    }
    
    return QSize(600, 450); //VGA (seriously, any lower is just sad)
}

//Event handlers
void ArpmanetDC::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);

    if (e->type() == QEvent::WindowStateChange)
    {
        QWindowStateChangeEvent *wEvent = (QWindowStateChangeEvent*)e;
        Qt::WindowStates state = wEvent->oldState();
        if (state != Qt::WindowMinimized && isMinimized())
        {
            wasMaximized = isMaximized();
            windowSize = size();
#ifdef Q_OS_WIN32
            //Trick necessary to hide window in Windows 7 (the call to hide should not be in the event function)
            //Gives problems in Linux (especially on a tiling windows manager such as Xmonad)!
            QTimer::singleShot(0, this, SLOT(hide()));
#endif
            restoreAction->setEnabled(true);
        }
        else if (state.testFlag(Qt::WindowMinimized))
        {
            if (wasMaximized)
                showMaximized();
            else
                resize(windowSize);
            restoreAction->setEnabled(false);
            activateWindow();
        }
    }
}

//Event returned when everything is saved in transferManager
void ArpmanetDC::closeClientEventReturn()
{
    if (pCloseEvent)
        //Send the event to the main window
        QMainWindow::closeEvent(pCloseEvent);
}

//Event signalled when the user closes the client
void ArpmanetDC::closeEvent(QCloseEvent *e)
{
    //Save the event for future use
    pCloseEvent = e;

    //Tell transferManager that the client is closing
    emit closeClientEvent();

    //Will maybe later add an option to bypass the close operation and ask the user 
    //if the program should be minimized to tray rather than closed... For now it works normally
    
    //QMainWindow::closeEvent(pCloseEvent);
}

void ArpmanetDC::checkSharedMemory()
{
    //Check if new magnet links have been added to the shared memory from other instances
    if (!pSharedMemory->isAttached())
        if (!pSharedMemory->attach())
            qDebug() << "ArpmanetDC::checkSharedMemory: Could not attach to shared memory";

    pSharedMemory->lock();
    QByteArray data((const char *)pSharedMemory->constData(), pSharedMemory->size()); //Read data
    
    if (!QString(data).isEmpty())
    {
        //Something was loaded into shared memory
        memset((char *)pSharedMemory->data(), '\0', pSharedMemory->size()); //Set data to zero
    }

    pSharedMemory->unlock();

    QString message(data);
    if (message.compare("DUPLICATE_INSTANCE") == 0)
    {
        //Restore instance if minimized
        if (restoreAction->isEnabled())
        {
            restoreAction->trigger();
        }

        //Duplicate instances were started
        activateWindow();
        QMessageBox::information((QWidget *)this, tr("ArpmanetDC"), tr("You can only run a single instance of ArpmanetDC.\nPlease close all other instances first before trying again."));
    }
    else if (message.startsWith("magnet"))
    {
        //Other instance sent a magnet link to this process
        mainChatLinkClicked(QUrl(message));
    }
}

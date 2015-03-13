# ArpmanetDC #

![http://arpmanetdc.googlecode.com/git/Resources/Logo128x128.png](http://arpmanetdc.googlecode.com/git/Resources/Logo128x128.png)

An awesome UDP-based peer-to-peer-plus-direct-connect application for distributed file distribution over a heterogeneous network environment.

This project requires the [Qt4](http://qt.nokia.com) toolkit >= 4.8, the [Crypto++](http://www.cryptopp.com) libraries for TTH hashing, as well as the [sqlite3](http://www.sqlite.org) libraries for the database backend.

## Progress ##

Project started: 9 Nov 2011

Current version: 0.1.8

Current stage: Alpha

**26 Nov 2011 - The project has just passed the 10,000 lines of code mark!**


**September 2012**

Some major restructuring around transfers is taking place, which in its currently bit-banged form can be described as <i>barely functional</i>.

The long term goal is to break the whole hierarchy of different types of segments down to just one (a segment :) that reads/writes through a connection, which is managed by the shiny new connection manager. Different protocols can be wrapped up inside connections, of which TCP is to be the first.

TCP can obviously not be run through our dispatch system, therefore we need to hang on to the FSTP transfers a little bit more as a failsafe alternative until we have uTP running. uTP will be our first dispatched connection based protocol, paving the way for our experimental work with FECTP transfers.

### Progress per feature ###
| **Stage** | **Feature** | **Notes** |
|:----------|:------------|:----------|
| <font color='orange'>Implementing</font> | Connection manager | Subsystem to manage connection based protocols, such as uTP, TCP, etc. |
| <font color='orange'>Implementing</font> | TCP server | Catches TCP connections |
| <font color='orange'>Implementing</font> | TCP connection | First connected protocol, others will follow along its path. |
| <font color='orange'>Implementing</font> | Connected segment | Transfer segment that reads and writes through a Connection of any kind. |
| <font color='red'>Inception</font> | uTP server | Catch and manage uTP connections the correct way |
| <font color='red'>Inception</font> | uTP connection | Connection wrapper around a uTP socket |
| <font color='orange'>Implementing</font> | Stats collector | Keep track of p2p transfer and control data of various kinds. |
| <font color='green'>Beta</font> | DC++ Hub integration | Chat and private messaging |
| <font color='green'>Beta</font> | Sharing | Hashing what you choose to share |
| <font color='green'>Beta</font> | Settings and Help |  |
| <font color='green'>Beta</font> | Bootstrapping | Finding friends automatically |
| <font color='green'>Beta</font> | Searching | P2P decentralised search|
| <font color='green'>Beta</font> | Smart scanning for bootstrap | Finding even moar friends automatically |
| <font color='green'>Beta</font> | Download queue (GUI side) | List management tested |
| <font color='green'>Beta</font> | FSTP transfer protocol |  |
| <font color='green'>Beta</font> | Finished downloads | List management tested |
| <font color='green'>Beta</font> | Transfer management (GUI side) | Transfers can be stopped |
| <font color='green'>Beta</font> | Network topology mapper | Keeps track of the broadcast/multicast buckets |
| <font color='green'>Beta</font> | P2P message dispatcher | First-in-line protocol decoding and encoding. |
| <font color='green'>Beta</font> | Download queue (Transfer side) |  |
| <font color='blue'>Alpha</font> | Segmented transfers | Implemented, seems to work well. |
| <font color='blue'>Alpha</font> | Containers | Queueing, browsing, processing |
| <font color='red'>Inception</font> | FECTP |  |


## Screenshots (Click for larger version) ##
### Chat (X11) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-chat-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-chat.png)

### Chat (Windows) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-chat-windows-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-chat-windows.png)

### Search (X11) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-search-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-search.png)

### Search (Windows) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-search-windows-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-search-windows.png)

### Share (X11) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-share-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-share.png)

### Share (Windows) ###

![![](http://arpmanetdc.googlecode.com/git/screenshots/small/ArpmanetDC-share-windows-small.png)](http://arpmanetdc.googlecode.com/git/screenshots/ArpmanetDC-share-windows.png)
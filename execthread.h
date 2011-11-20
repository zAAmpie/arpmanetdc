#ifndef EXECTHREAD_H
#define EXECTHREAD_H

#include <QObject>
#include <QThread>

//Super basic class for constructing a QThread which has an event loop
class ExecThread : public QThread
{
	Q_OBJECT

public:
	//Constructor
	ExecThread(QObject *parent = 0);

protected:
	//Main EXEC function
	void run();
};

#endif
#ifndef EXECTHREAD_H
#define EXECTHREAD_H

#include <QObject>
#include <QThread>

class ExecThread : public QThread
{
	Q_OBJECT

public:
	ExecThread(QObject *parent = 0);

protected:
	void run();
};

#endif
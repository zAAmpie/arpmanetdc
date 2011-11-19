#include "execthread.h"

ExecThread::ExecThread(QObject *parent) : QThread(parent)
{
	connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

void ExecThread::run()
{
	exec();
}
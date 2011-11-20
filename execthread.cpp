#include "execthread.h"

//Constructor
ExecThread::ExecThread(QObject *parent) : QThread(parent)
{
	//Automatically destroy the thread
	connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
}

//Main EXEC function
void ExecThread::run()
{
	//Start the event loop
	exec();
}
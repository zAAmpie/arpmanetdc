#include "arpmanetdc.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	ArpmanetDC w;
	w.show();
	return a.exec();
}

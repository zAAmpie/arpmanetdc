#ifndef CUSTOMTABLEITEMS_H
#define CUSTOMTABLEITEMS_H

#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTableView>
#include <QVariant>
#include <QPen>
#include <QSet>

//Custom delegate to display HTML code in QTableView
class HTMLDelegate : public QStyledItemDelegate
{
public:
	HTMLDelegate(QTableView *tableView);
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
private:
	QPen pGridPen;
};

//Custom tab widget with public access to the tabBar
class CTabWidget : public QTabWidget
{
public:
	CTabWidget(QWidget *parent = 0);
	QTabBar *tabBar() const { return (QTabWidget::tabBar()); }
};

#endif
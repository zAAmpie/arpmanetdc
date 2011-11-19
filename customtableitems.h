#ifndef CUSTOMTABLEITEMS_H
#define CUSTOMTABLEITEMS_H

#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTableView>
#include <QFileSystemModel>
#include <QPersistentModelIndex>
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

//Custom tab widget
class CTabWidget : public QTabWidget
{
public:
	CTabWidget(QWidget *parent = 0);
	QTabBar *tabBar() const { return (QTabWidget::tabBar()); }
};

//Custom FileSystemModel
class CFileSystemModel : public QFileSystemModel
{
public:
	CFileSystemModel(QObject *parent = 0);

	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);
	
private:
	QSet<QPersistentModelIndex> checklist;	
};

#endif
#ifndef CUSTOMTABLEITEMS_H
#define CUSTOMTABLEITEMS_H

#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTableView>
#include <QVariant>
#include <QPen>
#include <QString>
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


//Custom QStandardItem to allow for integer/size sorting
class CStandardItem : public QStandardItem
{
public:
    enum CStandardItemType {IntegerType, DoubleType, SizeType, RateType, CaseInsensitiveTextType};
    CStandardItem(CStandardItemType type, const QString &value) : QStandardItem(value) {pType = type; setEditable(false);}
    CStandardItem(CStandardItemType type, const QString &value, const QIcon &icon) : QStandardItem(icon, value) {pType = type; setEditable(false);}

    bool operator<(const QStandardItem &other) const;
private:
    CStandardItemType pType;
};

//Custom tab widget with public access to the tabBar
class CTabWidget : public QTabWidget
{
public:
	CTabWidget(QWidget *parent = 0);
	QTabBar *tabBar() const { return (QTabWidget::tabBar()); }
};

#endif
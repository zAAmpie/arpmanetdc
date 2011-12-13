#ifndef CUSTOMTABLEITEMS_H
#define CUSTOMTABLEITEMS_H

#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTableView>
#include <QVariant>
#include <QPen>
#include <QString>
#include <QSet>
#include <QProgressBar>
#include <QDateTime>

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

//Custom delegate to display progress bars in QTableView
class ProgressDelegate : public QStyledItemDelegate
{
public:
    ProgressDelegate();
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


//Custom QStandardItem to allow for integer/size sorting
class CStandardItem : public QStandardItem
{
public:
    enum CStandardItemType {IntegerType, DoubleType, SizeType, RateType, CaseInsensitiveTextType, PriorityType, ProgressType, DateType};
    CStandardItem(CStandardItemType type, const QString &value) : QStandardItem(value) {pType = type; setEditable(false);}
    CStandardItem(CStandardItemType type, const QString &value, const QIcon &icon) : QStandardItem(icon, value) {pType = type; setEditable(false);}

    void setFormat(const QString &format);
    QString format();

    bool operator<(const QStandardItem &other) const;
private:
    CStandardItemType pType;
    QString pFormat;
};

//Custom tab widget with public access to the tabBar
class CTabWidget : public QTabWidget
{
public:
	CTabWidget(QWidget *parent = 0);
	QTabBar *tabBar() const { return (QTabWidget::tabBar()); }
};

//Custom progressbar that shows text on top
class TextProgressBar : public QProgressBar
{
public:
    //No implementation
    TextProgressBar(QString text, QWidget *parent = 0) : QProgressBar(parent) {pText = text;}

    QString topText() const;
    void setTopText(const QString &text);
protected:
    void paintEvent(QPaintEvent *);

private:
    QString pText;
};

#endif
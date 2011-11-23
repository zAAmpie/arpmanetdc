#include "customtableitems.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>

HTMLDelegate::HTMLDelegate(QTableView *tableView)
{
	int gridHint = tableView->style()->styleHint(QStyle::SH_Table_GridLineColor, new QStyleOptionViewItemV4());
	QColor gridColor = static_cast<QRgb>(gridHint);
	pGridPen = QPen(gridColor, 0, tableView->gridStyle());
}

//Determines the custom delegate's size
QSize HTMLDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());

	QSize iconSize = options.icon.actualSize(options.rect.size());
	int iconExtra = 10;

    return QSize(doc.idealWidth() + iconSize.width() + iconExtra, doc.size().height());
}

//Draws the HTML code
void HTMLDelegate::paint(QPainter* painter, const QStyleOptionViewItem & option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    painter->save();

	//Draw grid lines
	QPen oldPen = painter->pen();
	painter->setPen(pGridPen);
	//painter->drawLine(option.rect.topRight(), option.rect.bottomRight()); //vertical line
	//painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight()); //horizontal line
	painter->setPen(oldPen);

    QTextDocument doc;
    doc.setHtml(options.text);

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    // shift text right to make icon visible
    QSize iconSize = options.icon.actualSize(options.rect.size());
	int iconExtra = 10;
    painter->translate(options.rect.left()+ iconSize.width() + iconExtra, options.rect.top());
    QRect clip(0, 0, options.rect.width()+ iconSize.width() + iconExtra, options.rect.height());

    //doc.drawContents(painter, clip);

    painter->setClipRect(clip);
    QAbstractTextDocumentLayout::PaintContext ctx;
    // set text color to red for selected item
    if (option.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, QColor("white"));
    ctx.clip = clip;
    doc.documentLayout()->draw(painter, ctx);

	

    painter->restore();
}

//Sort items according to their individual types
bool CStandardItem::operator<(const QStandardItem &other) const
{
    if (pType == IntegerType)
    {
        return text().toLongLong() < other.text().toLongLong();
    }
    if (pType == DoubleType)
    {
        return text().toDouble() < other.text().toDouble();
    }
    else if (pType == SizeType)
    {
        QString regex = "(\\d+\\.\\d+)\\s*([a-z]*)";
        QRegExp rx(regex, Qt::CaseInsensitive);

        double valThis, valThat;
        QString unitThis, unitThat;
        if (rx.indexIn(text()) != -1)
        {
            valThis = rx.cap(1).toDouble();
            unitThis = rx.cap(2);
        }
        if (rx.indexIn(other.text()) != -1)
        {
            valThat = rx.cap(1).toDouble();
            unitThat = rx.cap(2);
        }

        if (unitThis.compare("TiB") == 0 || unitThis.compare("TB") == 0)
            valThis *= 1<<40;
        if (unitThis.compare("GiB") == 0 || unitThis.compare("GB") == 0)
            valThis *= 1<<30;
        if (unitThis.compare("MiB") == 0 || unitThis.compare("MB") == 0)
            valThis *= 1<<20;
        if (unitThis.compare("KiB") == 0 || unitThis.compare("KB") == 0)
            valThis *= 1<<10;

        if (unitThat.compare("TiB") == 0 || unitThat.compare("TB") == 0)
            valThat *= 1<<40;
        if (unitThat.compare("GiB") == 0 || unitThat.compare("GB") == 0)
            valThat *= 1<<30;
        if (unitThat.compare("MiB") == 0 || unitThat.compare("MB") == 0)
            valThat *= 1<<20;
        if (unitThat.compare("KiB") == 0 || unitThat.compare("KB") == 0)
            valThat *= 1<<10;

        return valThis < valThat;
    }
    else if (pType == CaseInsensitiveTextType)
    {
        return text().toUpper() < other.text().toUpper();
    }

    return text() < other.text();
}

CTabWidget::CTabWidget(QWidget *parent) : QTabWidget(parent)
{

}
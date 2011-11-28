#include "customtableitems.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include "util.h"

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
    else if (pType == DoubleType)
    {
        return text().toDouble() < other.text().toDouble();
    }
    else if (pType == RateType)
    {
        return rateToBytes(text()) < rateToBytes(other.text());
    }
    else if (pType == SizeType)
    {
        return sizeToBytes(text()) < sizeToBytes(other.text());
    }
    else if (pType == CaseInsensitiveTextType)
    {
        return text().toUpper() < other.text().toUpper();
    }
    else if (pType == PriorityType)
    {
        QString thisText = text().replace("High","C").replace("Normal","B").replace("Low","A");
        QString otherText = other.text().replace("High","C").replace("Normal","B").replace("Low","A");
        return thisText < otherText;
    }

    return text() < other.text();
}

CTabWidget::CTabWidget(QWidget *parent) : QTabWidget(parent)
{

}

QString TextProgressBar::topText() const
{
    return pText;
}

void TextProgressBar::setTopText(const QString &text)
{
    pText = text;
}

void TextProgressBar::paintEvent(QPaintEvent *paintEvent)
{
    QProgressBar::paintEvent(paintEvent);

    if (minimum() == 0 && maximum() == 0)
    {
        QRect rect = paintEvent->rect();
        
        QPainter painter(this);
        painter.setPen(Qt::gray);
        painter.drawText(rect, Qt::AlignCenter, pText);
    }
}
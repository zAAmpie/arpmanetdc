#include "customtableitems.h"
#include "checkableproxymodel.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include "util.h"
#include <QtGui>

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
    quint16 iconWidth = iconSize.width() < 0 ? 0 : iconSize.width();
	int iconExtra = 10;

    //return QSize(doc.idealWidth() + iconSize.width() + iconExtra, doc.size().height());
    return QSize(doc.idealWidth() + iconWidth + iconExtra, options.rect.height());
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
    quint16 iconWidth = iconSize.width() < 0 ? 0 : iconSize.width();
	int iconExtra = 10;
    painter->translate(options.rect.left()+ iconWidth + iconExtra, options.rect.top());
    //QRect clip(0, 0, options.rect.width()+ iconSize.width() + iconExtra, options.rect.height());
    QRect clip(0, 0, options.rect.width() - iconExtra - iconWidth, options.rect.height());

    //doc.drawContents(painter, clip);

    painter->setClipRect(clip);
    QAbstractTextDocumentLayout::PaintContext ctx;
    // set text color to red for selected item
    if (option.state & QStyle::State_Selected && options.state & QStyle::State_Active)
        ctx.palette.setColor(QPalette::Text, QColor("white"));
    else if (option.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, QColor("black"));
    ctx.clip = clip;
    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

ProgressDelegate::ProgressDelegate()
{
    //Constructor
}

QSize ProgressDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    return QSize(options.rect.width(), options.rect.height());
}

void ProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    painter->save();

    //Get value
    bool ok;
    QChar type = options.text.at(0);
    int value = options.text.mid(1).toInt(&ok);
    
    //Draw the main control
    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    if (!ok)
    {
        painter->restore();
        return;
    }

    //Colors
    QColor progressColor;
    if (type == 'D') //TRANSFER_TYPE_DOWNLOAD
        progressColor = QColor(79, 189, 54); //Green
    else if (type == 'U') //TRANSFER_TYPE_UPLOAD
        progressColor = QColor(37, 149, 214); //Blue

    QColor frameColor(Qt::darkGray);
    QColor textColor(Qt::black);
    if (options.state & QStyle::State_Selected && options.state & QStyle::State_Active)
    {
        frameColor = QColor(Qt::white);
        textColor = QColor(Qt::white);
        progressColor = QColor(Qt::lightGray);
    }
    else if (options.state & QStyle::State_Selected)
        frameColor = QColor(Qt::darkGray);
    
    

    //Draw the progress bar frame
    painter->setRenderHint(QPainter::Antialiasing);

    int width = options.rect.width();
    int height = options.rect.height();
    QRectF rect(options.rect);
    rect.adjust(1,2,-1,-2);

    painter->setPen(frameColor);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, 3, 3);

    //Draw the progress bar
    if (value > 0)
    {
        qreal val = value > 100 ? 100 : value;
        qreal valWidth = ((qreal)val * (qreal)rect.width()) / 100;
        QRectF valRect(rect.left(), rect.top(), valWidth, rect.height());
        valRect.adjust(2,2,-2,-2);

        if (valRect.right() < valRect.left())
            valRect.setRight(valRect.left());
           
        QLinearGradient gradient(valRect.topLeft(), valRect.bottomLeft());
        gradient.setColorAt(1, progressColor.darker(150));
        gradient.setColorAt(0, progressColor);

        painter->setBrush(QBrush(gradient));
        //painter->setBrush(QBrush(progressColor));
        painter->setPen(progressColor);
        painter->drawRoundedRect(valRect, 2, 2);
    }

    //Draw the percentage text
    painter->setPen(textColor);
    QString drawString;
    if (value != 100)
        drawString = tr("%1%").arg(value);
    else
        drawString = tr("Finished");
    if (type == 'U') //TRANSFER_TYPE_UPLOAD
        drawString.prepend("±"); //Upload progress are drawn as estimates
    painter->drawText(rect, Qt::AlignCenter, drawString);

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
    else if (pType == ProgressType)
    {
        QString thisText = text().mid(1);
        QString otherText = other.text().mid(1);
        return thisText.toLongLong() < otherText.toLongLong();
    }
    else if (pType == DateType)
    {
        if (pFormat.isEmpty())
            return QDateTime::fromString(text(), "dd/MM/yyyy HH:mm:ss:zzz") < QDateTime::fromString(other.text(), "dd/MM/yyyy HH:mm:ss:zzzz");
        else
            return QDateTime::fromString(text(), pFormat) < QDateTime::fromString(other.text(), pFormat);
    }
    else if (pType == TimeDurationType)
    {
        //Format: HH:mm:ss i.e. 00:02:35
        QStringList timesThis = text().split(":");
        QStringList timesThat = other.text().split(":");

        //If list contains hours/minutes/seconds
        if (timesThis.size() == 3 && timesThat.size() == 3)
        {
            //Compare seconds
            if (timesThis.at(0).toInt() == timesThat.at(0).toInt() && timesThis.at(1).toInt() == timesThat.at(1).toInt())
                return timesThis.at(2).toInt() < timesThat.at(2).toInt();
            
            //Compare minutes
            if (timesThis.at(0).toInt() == timesThat.at(0).toInt())
                return timesThis.at(1).toInt() < timesThat.at(1).toInt();

            //Compare hours
            return timesThis.at(0).toInt() < timesThat.at(0).toInt();
        }
    }

    return text() < other.text();
}

void CStandardItem::setFormat(const QString &format)
{
    pFormat = format;
}

QString CStandardItem::format()
{
    return pFormat;
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

void CDragTreeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        dragStartPosition = event->pos();

    QTreeView::mousePressEvent(event);
}

void CDragTreeView::mouseMoveEvent(QMouseEvent *event)
{
    // if not left button - return
    if (!(event->buttons() & Qt::LeftButton)) 
    {
        QTreeView::mouseMoveEvent(event);
        return;
    }
 
    if ((event->pos() - dragStartPosition).manhattanLength() < QApplication::startDragDistance())
    {
        QTreeView::mouseMoveEvent(event);
        return;
    }

    // if no item selected, return (else it would crash)
    if (selectionModel()->selectedRows().isEmpty()) return;
 
    QSortFilterProxyModel *pProxyModel = reinterpret_cast<CheckableProxyModel *>(model());
    QFileSystemModel *pModel = reinterpret_cast<QFileSystemModel *>(pProxyModel->sourceModel());
    
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
 
    // construct list of QUrls
    // other widgets accept this mime type, we can drop to them
    QList<QUrl> list;
    for (int i = 0; i < selectionModel()->selectedRows().size(); i++)
    {
        QModelIndex selectedIndex = selectionModel()->selectedRows().at(i);
    
        list.append(QUrl(pModel->filePath(pProxyModel->mapToSource(selectedIndex))));
    }
 
    // mime stuff
    mimeData->setUrls(list);
    drag->setMimeData(mimeData);
 
    // start drag
    drag->start(Qt::CopyAction | Qt::MoveAction);

    QTreeView::mouseMoveEvent(event);
}

void CDragTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void CDropTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    QStringList formats = event->mimeData()->formats();
    if (formats.contains("text/uri-list"))
        event->acceptProposedAction();        
}

void CDropTreeView::dropEvent(QDropEvent *event)
{
    QList<QUrl> list;

    if (event->mimeData()->hasUrls())
        list = event->mimeData()->urls();
    emit droppedURLList(list);

    event->acceptProposedAction();
}

void CDropTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}
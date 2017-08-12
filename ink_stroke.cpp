#include "ink_stroke.h"

InkStroke::InkStroke(QObject *parent)
    : InkStroke(QColor(), QJsonArray(), parent)
{ }

InkStroke::InkStroke(const QColor& color, QObject* parent)
    : InkStroke(color, QJsonArray(), parent)
{ }

InkStroke::InkStroke(const QJsonObject& stroke, QObject* parent)
    : InkStroke(stroke["color"].toString(), stroke["points"].toArray(), parent)
{ }

InkStroke::InkStroke(const QColor& color, const QJsonArray points, QObject* parent)
    : QObject(parent)
    , m_color(color)
    , m_points(points)
{ 
    int count = pointCount();
    m_allPoints.reserve(count);
    for (int i=0; i < count; ++i)
    {
        const InkPoint& ip = point(i);
        m_allPoints.push_back(qMakePair(ip.point, ip.size));
    }
}

QColor InkStroke::color() const
{
    return m_color;
}

void InkStroke::setColor(QColor clr)
{
    if(m_color != clr)
    {
        m_color = clr;
    }
}


InkPoint InkStroke::point(int index) const
{
    QJsonObject jsonPt(m_points.at(index).toObject());
    InkPoint inkPt { QPoint(jsonPt.value("x").toInt(),
                   jsonPt.value("y").toInt()),
                   jsonPt.value("w").toDouble() };
    return inkPt;
}

QJsonObject InkStroke::toJson() const
{
    return QJsonObject{
        {"color", m_color.name()},
        {"points", m_points}
    };
}

void InkStroke::drawSmoothStroke(QPainter& painter, const QPointF& previous,
                                 const QPointF& point, const QPointF& next)  const
{
    QPointF c1 = (previous + point) / 2;
    QPointF c2 = (next + point) / 2;
    QPointF cc = (c1 + c2) / 2;
    QPointF adjust = (point + cc) / 2;

    // Not smooth enough! Do more process.
    if((adjust - cc).manhattanLength() > 1)
    {
        painter.drawLine(c1, (c1 + adjust) / 2);
        drawSmoothStroke(painter, c1, adjust, c2);
        painter.drawLine((c2 + adjust) / 2, c2);
    }
    else
    {
        painter.drawLine(c1, adjust);
        painter.drawLine(adjust, c2);
    }
}

void InkStroke::draw(QPainter& painter, bool mono, double scale) const
{
    int ptCount = pointCount();
    if(ptCount == 0) return;

    QPen pen;
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    if(mono)
    {
        pen.setColor(Qt::black);
    }
    else
    {
        pen.setColor(this->color());
    }

    auto bound = boundRect();


    if((bound.width() < 10 && bound.height() < 10) || ptCount < 2)
    {
        float width = 0.0;
        for(int i = 0; i < ptCount; i++)
        {
            width += getPoint(i).second;
        }
        pen.setWidthF(width / ptCount);
        painter.setPen(pen);
        painter.drawPoint(bound.center());
    }
    else
    {
        QPointF ptStart, ptEnd;
        pen.setWidthF(((getPoint(0).second + getPoint(1).second)/2)*scale);
        painter.setPen(pen);
        ptStart = QPointF(getPoint(0).first.x()*scale, getPoint(0).first.y()*scale);
        ptEnd = QPointF(ptStart + QPointF(getPoint(1).first.x()*scale, getPoint(1).first.y()*scale))/ 2;
        painter.drawLine(ptStart, ptEnd);

        for(int i = 1; i < ptCount - 1; i++)
        {
            pen.setWidthF(getPoint(i).second*scale);
            painter.setPen(pen);
            drawSmoothStroke(painter,
                             QPointF(getPoint(i - 1).first.x()*scale, getPoint(i - 1).first.y()*scale),
                             QPointF(getPoint(i).first.x()*scale, getPoint(i).first.y()*scale ),
                             QPointF(getPoint(i + 1).first.x()*scale, getPoint(i + 1).first.y()*scale));
        }

        pen.setWidthF((getPoint(ptCount- 2).second + getPoint(ptCount- 1).second) *scale / 2);
        painter.setPen(pen);
        ptEnd = QPointF(getPoint(ptCount - 1).first.x()*scale, getPoint(ptCount - 1).first.y()*scale);
        ptStart = QPointF(QPointF(getPoint(ptCount - 2).first.x()*scale, getPoint(ptCount - 2).first.y()*scale) + ptEnd) / 2;
        painter.drawLine(ptStart, ptEnd);
    }
}

QRect InkStroke::boundRect() const
{
    QPolygon polygon;
    int ptCount = pointCount();
    if(ptCount == 0)
    {
        return QRect();
    }

    for(int i = 0; i < ptCount; i++)
    {
        polygon << this->getPoint(i).first;
    }
    polygon << this->getPoint(0).first;

    return polygon.boundingRect().adjusted(-30, -30, 30, 30);
}

void InkStroke::addPoint(const QPoint& point, double pen_width)
{
    m_points.push_back(QJsonObject{
                           {"x", point.x()},
                           {"y", point.y()},
                           {"w", pen_width}
                       });

    m_allPoints.push_back(qMakePair(point,pen_width));

    emit pointAdded(point, pen_width);
}

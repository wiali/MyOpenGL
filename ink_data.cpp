#include <QDebug>
#include <QPolygon>

#include "ink_data.h"

bool operator==(const InkStroke& stroke1, const InkStroke& stroke2)
{
    return (stroke1.m_color == stroke2.m_color && stroke1.m_points == stroke2.m_points);
}

InkData::InkData()
    : QObject(nullptr)
    , m_needSave(true)
    , m_currentStroke(new InkStroke)
    , m_canvasSize(QSize(1920,1080))
{ }

InkData::InkData(const QString &jsonStrokes)
    : InkData()
{
    fromJsonString(jsonStrokes);
}

void InkData::addCurrentStroke(bool notify)
{
    if(m_currentStroke->pointCount() > 0)
    {
        auto stroke = m_currentStroke;

        if(m_needSave)
        {
            m_strokes.push_back(stroke);
        }

        m_currentStroke.reset(new InkStroke);

        if(notify)
        {
            emit strokeAdded(stroke, m_currentStroke);
        }
    }
}

void InkData::removeStroke(int index, bool notify)
{
    auto stroke = this->stroke(index);
    m_strokes.removeAt(index);

    if(notify)
    {
        emit strokeRemoved(index, stroke);
    }
}

void InkData::clear()
{
    m_strokes.clear();
    emit cleared();
}

int InkData::strokeCount()
{
    return m_strokes.size();
}

QSharedPointer<InkStroke> InkData::stroke(int index)
{
    return m_strokes[index];
}

void InkData::merge(const InkData& inkData)
{
    for(auto stroke : inkData.m_strokes)
    {
        m_strokes.push_back(stroke);
    }
}

QString InkData::toJsonString()
{
    QJsonArray array;
    for(auto stroke : m_strokes)
    {
        array.append(stroke->toJson());
    }

    return QString(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool InkData::fromJsonString(const QString& jsonStrokes)
{
    auto doc = QJsonDocument::fromJson(jsonStrokes.toUtf8());
    clear();

    if(doc.isArray())
    {
        for(auto stroke : doc.array())
        {
            m_strokes.append(QSharedPointer<InkStroke>::create(stroke.toObject()));
        }
        return true;
    }

    return false;
}

void InkData::clone(const InkData& inkData)
{
    m_strokes = inkData.m_strokes;
}

bool InkData::equal(const InkData& inkData)
{
    return (m_strokes == inkData.m_strokes);
}

QSharedPointer<InkStroke> InkData::currentStroke() const
{
    return m_currentStroke;
}

QSize InkData::canvasSize()
{
    return m_canvasSize;
}

void InkData::setCanvasSize(QSize newSize)
{
    if (m_canvasSize != newSize)
    {
        m_canvasSize = newSize;
        emit canvasSizeChanged(newSize);
    }
}

void InkData::insertStroke(int index, QSharedPointer<InkStroke> stroke, bool notify)
{
    m_strokes.insert(index, stroke);

    if (notify)
    {
        emit strokeInserted(index, stroke);
    }
}

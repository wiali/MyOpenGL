#ifndef INK_DATA_H
#define INK_DATA_H


#include <QPair>
#include <QList>
#include <QPoint>
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QRect>


#include "ink_stroke.h"

class InkData : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QSize canvasSize READ canvasSize WRITE setCanvasSize NOTIFY canvasSizeChanged)

public:
    InkData();
    InkData(const QString& jsonStrokes);

    /*! \brief Add stroke
     */
    void addCurrentStroke(bool notify = true);

    /*!
     * \brief Inserts the stroke at end
     * \param stroke Stroke to be inserted.
     * \param index Index of insertion.
     * \param notify Should the stroke change be notified?
     */
    void insertStroke(int index, QSharedPointer<InkStroke> stroke, bool notify);

    /*! \brief Get stroke count
     */
    int strokeCount();

    /*! \brief Get a stroke
     */
    QSharedPointer<InkStroke> stroke(int index);

    /*! \brief Remove a stroke
     */
    void removeStroke(int index, bool notify = true);

    /*! \brief Remove all the strokes.
     */
    void clear();

    /*! \brief Merge the InkData object's data
     */
    void merge(const InkData& inkData);

    /*! \brief Convert the ink data to json string.
     *  We can use the json string to create a ink data object.
     */
    QString toJsonString();

    /*! \brief Initialize the object using the json string.
     */
    bool fromJsonString(const QString& jsonStrokes);

    void clone(const InkData& inkData);

    bool equal(const InkData& inkData);

    void setSaveStroke(bool save) { m_needSave = save; }

    QSharedPointer<InkStroke> currentStroke() const;

    QSize canvasSize();

    void setCanvasSize(QSize newSize);

signals:

    /*! \brief Emit this signal when a ink stroke has been added.
     */
    void strokeAdded(QSharedPointer<InkStroke> addedStroke, QSharedPointer<InkStroke> newStroke);

    /*! \brief Emit this signal when a ink stroke has been removed.
     */
    void strokeRemoved(int index, QSharedPointer<InkStroke> stroke);

    /*! \brief Emit this signal when a ink stroke has been inserted.
     */
    void strokeInserted(int index, QSharedPointer<InkStroke> stroke);

    void cleared();

    void canvasSizeChanged(QSize newSize);

private:
    QVector<QSharedPointer<InkStroke>> m_strokes;
    QSharedPointer<InkStroke> m_currentStroke;
    bool m_needSave;

    /*! \brief Holds canvas size for Ink Data. It's mat screen 1920x1280 by default,
     *         but for CaptureWT it can be 1920x1080 for the front facing camera image.
     */
    QSize m_canvasSize;
};

#endif // INK_DATA_H

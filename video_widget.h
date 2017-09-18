#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QTouchEvent>
#include <QWidget>
#include <QSharedPointer>
#include <QGestureEvent>
#include <QOpenGLFramebufferObject>
#include <QTransform>

#include <video_source/videopipeline.h>

#include "model/live_capture_model.h"
#include "components/live_video_stream_compositor.h"

class QAbstractVideoSurface;
class VideoWidgetSurface;
class RenderingThread;
class VideoWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    VideoWidget(QWidget *parent = 0);

    virtual ~VideoWidget();

    QSharedPointer<LiveVideoStreamCompositor> getCompositor() { return m_compositor; }
    LiveVideoStreamCompositor::VideoStreamMappings getRenderMapping() { return m_renderMapping; }

public slots:
    void setModel(QSharedPointer<LiveCaptureModel> model,
                  QSharedPointer<LiveVideoStreamCompositor> compositor,
                  bool viewportManipulationEnabled);

private slots:
    void onVideoStreamStateChanged();
    void onCompositorUpdated();
    void updateTransform();

signals:
    void viewportManipulationEnabledChanged(bool viewportManipulationEnabled);
    void geometryChanged();

protected:
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;

    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    bool event(QEvent *event) Q_DECL_OVERRIDE;

    virtual void initializeGL() override;

private:
    struct VideoStreamZoomAndPan;

    bool gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
    void panTriggered(QPanGesture *gesture);

    QTransform calculateTransform(const QPointF& pan, qreal zoom);
    void updateZoomAndPan(qreal zoom, QPointF pan);
    void updateZoomAndPan();

    void moveCenter(QPointF offset);
    bool zoomTo(QPointF relativePosition, qreal zoomLevel);

    /*! \brief This is needed to track gesture and mouse to work around the problem of Pan Gesture
               events are not raised when PinchGesture is not active.
     */
    bool m_pinchGestureActive;

    QTransform m_transform;

    QPointF m_clickedPos;

    /*! \brief This is needed to track gesture and mouse to work around the problem of Pan Gesture
               events are not raised when PinchGesture is not active.
     */
    bool m_mouseMoveActive;

    qreal m_lastScaleFactor;
    qreal m_startZoomFactor;    
    QSharedPointer<video::source::SourcePipeline> m_sourcePipeline;
    QSharedPointer<LiveVideoStreamCompositor> m_compositor;
    QVector<QMetaObject::Connection> m_connections;
    QHash<QSharedPointer<VideoStreamSourceModel>, VideoStreamZoomAndPan> m_videoStreamZoomAndPan;
    QSharedPointer<LiveCaptureModel> m_model;    
    float m_mouseWheelZoomSensitivity;
    qreal m_touchSensitivity;
    qreal m_touchTreshold;
    QPointF m_zoomRange;
    bool m_viewportManipulationEnabled;
    QPointF m_touchStartPoint1, m_touchStartPoint2;
    LiveVideoStreamCompositor::VideoStreamMappings m_renderMapping;

    QSharedPointer<RenderingThread> m_renderingThread;

    void startThread();
    void stopThread();
};

#endif

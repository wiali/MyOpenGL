#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QTouchEvent>
#include <QWidget>
#include <QSharedPointer>
#include <QGestureEvent>
#include <QOpenGLFramebufferObject>

#include <video_source/videopipeline.h>

#include "model/live_capture_model.h"
#include "components/live_video_stream_compositor.h"

class QAbstractVideoSurface;
class VideoWidgetSurface;
class RenderingThread;
class VideoWidget : public QOpenGLWidget
{
    Q_OBJECT
    Q_PROPERTY(bool viewportManipulationEnabled READ viewportManipulationEnabled WRITE setViewportManipulationEnabled NOTIFY viewportManipulationEnabledChanged)
public:
    VideoWidget(QWidget *parent = 0);

    bool viewportManipulationEnabled() const;

    QSharedPointer<LiveVideoStreamCompositor> getCompositor() { return m_compositor; }
    LiveVideoStreamCompositor::VideoStreamMappings getRenderMapping() { return m_renderMapping; }

public slots:
    void setModel(QSharedPointer<LiveCaptureModel> model,
                  QSharedPointer<LiveVideoStreamCompositor> compositor);
    void setViewportManipulationEnabled(bool viewportManipulationEnabled);

private slots:

    void onViewportChanged();
    void onVideoStreamStateChanged();
    void onCompositorUpdated();

signals:

    void viewportManipulationEnabledChanged(bool viewportManipulationEnabled);

protected:
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
    virtual void initializeGL() override;

    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    bool event(QEvent *event) Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent* e);

private:
    bool gestureEvent(QGestureEvent *event);

    void pinchTriggered(QPinchGesture *gesture);
    void panTriggered(QPanGesture *gesture);    

    void updateVideoRect();

    void startThread();

    void stopThread();

    /*! \brief This is needed to track gesture and mouse to work around the problem of Pan Gesture
               events are not raised when PinchGesture is not active.
     */
    bool m_pinchGestureActive;

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

    QSharedPointer<LiveCaptureModel> m_model;    
    float m_mouseWheelZoomSensitivity;
    qreal m_touchSensitivity;
    qreal m_touchTreshold;
    QPointF m_zoomRange;
    bool m_viewportManipulationEnabled;
    QPointF m_touchStartPoint1, m_touchStartPoint2;
    LiveVideoStreamCompositor::VideoStreamMappings m_renderMapping;

    void tryUpdateViewport(QRectF updatedViewport);
    void moveCenter(QPointF offset);
    bool zoomTo(QPointF relativePosition, qreal zoomLevel);
    QPointF normalizeToVideoRect(QPoint widgetPos);

    QSharedPointer<RenderingThread> m_renderingThread;
};

#endif

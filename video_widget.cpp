#include <QApplication>

#include "video_widget.h"
#include "common/utilities.h"


class RenderingThread : public QThread
{
public:
    RenderingThread(VideoWidget* widget)
        :surface(new QOffscreenSurface), m_widget(widget)

    {
        context = QSharedPointer<QOpenGLContext>::create();
        context->setShareContext(m_widget->context());
        context->setFormat(m_widget->context()->format());
        context->create();
        context->moveToThread(this);

        surface->setFormat(context->format());
        surface->create();
        surface->moveToThread(this);
    }
    
    void initialize()
    {
        QOpenGLFramebufferObjectFormat framebufferFormat;
        framebufferFormat.setAttachment(
            QOpenGLFramebufferObject::CombinedDepthStencil);

        framebufferSize = QSize(4200, 2800);
        renderFbo = QSharedPointer<QOpenGLFramebufferObject>::create(
            framebufferSize,
            framebufferFormat);

        displayFbo = QSharedPointer<QOpenGLFramebufferObject>::create(
            framebufferSize,
            framebufferFormat);

        initialized = true;
    }

    void stop()
    {
        mutex.lock();
        exiting = true;
        mutex.unlock();
    }

    void lock()
    {
        mutex.lock();
    }

    void unlock()
    {
        mutex.unlock();
    }

    void renderFrame()
    {
        // Bind the framebuffer for rendering.
        //renderFbo->bind();

        auto f = QOpenGLContext::currentContext()->functions();

        f->glClearColor(0, 0, 0, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto compositor = m_widget->getCompositor();
        auto renderMapping = m_widget->getRenderMapping();
        if (compositor)
        {
            compositor->blitFramebuffer(renderFbo.data(), renderMapping);
        }

        // Flush the pipeline
        //glFlush();

        //// Release the framebuffer
        //renderFbo->release();
        //// Take the current framebuffer texture ID
        tex = renderFbo->texture();

        //// Swap the framebuffers for double-buffering.
        //std::swap(renderFbo, displayFbo);
    }

    GLuint framebufferTexture()
    {
        return tex;
    }

protected:
    void run()
    {
        for (;;)
        {
            // Lock the rendering mutex.
            QMutexLocker lock(&mutex);

            // Stops the thread if exit flag is set.
            if (exiting)
                break;

            // Make the OpenGL context current on offscreen surface.
            context->makeCurrent(surface.data());

            // Initialize if not done.
            if (!initialized) // just think initialized by widget
            {
                //initialize(d);
                initialized = true;
            }

            // Renders the frame
            renderFrame();

            // Release OpenGL context
            context->doneCurrent();

            //context->moveToThread(application);

            // Notify UI about new frame.
            QMetaObject::invokeMethod(m_widget, "update");
        }
    }

private:
    // OpenGL context
    QSharedPointer<QOpenGLContext> context;
    // Offscreen surface
    QSharedPointer<QOffscreenSurface> surface;
    // OpengL widget
    VideoWidget* m_widget;
    // Size of framebuffers
    QSize framebufferSize;

    // Rendering mutex
    QMutex mutex;
    // True if the application is exiting
    bool exiting = false;
    // True if the OpenGL is initialized
    bool initialized = false;

    // Framebuffer texture ID for the UI thread.
    GLuint tex = 0;

    // Framebuffer for thread to render the rotating quad.
    QSharedPointer<QOpenGLFramebufferObject> renderFbo;
    // Framebuffer for UI to display
    QSharedPointer<QOpenGLFramebufferObject> displayFbo;

};


VideoWidget::VideoWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_viewportManipulationEnabled(true)
    , m_lastScaleFactor(0)
    , m_startZoomFactor(1)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);

    grabGesture(Qt::PanGesture);
    grabGesture(Qt::PinchGesture);

    QPalette palette = this->palette();
    palette.setColor(QPalette::Background, Qt::black);
    setPalette(palette);

    auto settings = Utilities::applicationSettings("video_preview");

    m_mouseWheelZoomSensitivity = settings->value("mouse_wheel_zoom_sensitivity", 1500).toFloat();
    m_touchSensitivity = settings->value("touch_sensitivity", 250).toReal();
    m_touchTreshold = settings->value("touch_treshold", 5).toReal();
    m_zoomRange = QPointF(settings->value("zoom_range_min", 1).toReal(), settings->value("zoom_range_max", 10).toReal());    
}

void VideoWidget::resizeGL(int width, int height)
{
    auto f = QOpenGLContext::currentContext()->functions();

    int side = qMin(width, height);
    f->glViewport((width - side) / 2, (height - side) / 2, side, side);

    QOpenGLWidget::resizeGL(width, height);

    updateVideoRect();
}

void VideoWidget::initializeGL()
{
    //startThread();
}

void VideoWidget::paintGL()
{
    //if (!m_renderingThread)
    //    return;

    //m_renderingThread->lock();
    
    if (m_model)
    {
        if (m_renderMapping.count() != m_model->selectedVideoStreamSources().count()) {
            updateVideoRect();
        }

        //const GLuint textureId =
        //    m_renderingThread->framebufferTexture();

        //makeCurrent();

        ////glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_2D, textureId);

        //doneCurrent();

        auto f = QOpenGLContext::currentContext()->functions();

        f->glClearColor(0, 0, 0, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (m_compositor)
        {
            m_compositor->blitFramebuffer(nullptr, m_renderMapping);
        }

        //if (Utilities::g_inkToolBox)
        {
            //GlobalUtilities::delay(0);
            //Utilities::g_inkToolBox->repaint();
            //QMetaObject::invokeMethod(Utilities::g_inkToolBox.data(), "repaint");
        }

        //if (m_compositor)
        //{
        //    makeCurrent();
        //    m_compositor->showFramebuffer(nullptr, m_renderMapping);
        //    doneCurrent();
        //}
    }

    //m_renderingThread->unlock();

    //QOpenGLWidget::paintGL();
}


bool VideoWidget::viewportManipulationEnabled() const
{
    return m_viewportManipulationEnabled;
}

void VideoWidget::setViewportManipulationEnabled(bool viewportManipulationEnabled)
{
    if (m_viewportManipulationEnabled != viewportManipulationEnabled)
    {
        m_viewportManipulationEnabled = viewportManipulationEnabled;

        emit viewportManipulationEnabledChanged(m_viewportManipulationEnabled);
    }
}

void VideoWidget::onCompositorUpdated() { update(); }

void VideoWidget::setModel(QSharedPointer<LiveCaptureModel> model, QSharedPointer<LiveVideoStreamCompositor> compositor)
{
    m_model = model;
    m_compositor = compositor;

    for (auto videoStreamSource : m_model->videoStreamSources())
    {
        connect(videoStreamSource.data(), &VideoStreamSourceModel::viewportChanged, this, &VideoWidget::onViewportChanged);
    }

    connect(model.data(), &LiveCaptureModel::videoStreamStateChanged, this, &VideoWidget::onVideoStreamStateChanged);
    connect(m_compositor.data(), &LiveVideoStreamCompositor::updated, this, &VideoWidget::onCompositorUpdated);

    onVideoStreamStateChanged();
}

void VideoWidget::onVideoStreamStateChanged()
{
    foreach(auto connection, m_connections)
    {
        disconnect(connection);
    }
    m_connections.clear();    

    onViewportChanged();
}

void VideoWidget::updateVideoRect()
{
    if (m_compositor)
    {
        m_renderMapping = m_compositor->videoStreamMappings(size());
    }
}

void VideoWidget::onViewportChanged()
{
    updateVideoRect();
    update();
}

QPointF VideoWidget::normalizeToVideoRect(QPoint widgetPos)
{
    const auto firstStream = m_model->selectedVideoStreamSources().first();
    const auto videoRect = m_renderMapping[firstStream].destination;
    const QPointF videoPosition(widgetPos.x() - videoRect.x(), videoRect.height() - (widgetPos.y() - videoRect.y()));

    return QPointF(videoPosition.x() / videoRect.width(), videoPosition.y() / videoRect.height());
}

void VideoWidget::wheelEvent(QWheelEvent* event)
{
    if (m_viewportManipulationEnabled)
    {
        if(m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running &&
                m_model->captureState() == LiveCaptureModel::CaptureState::NotCapturing)
        {
            const auto relativePosition = normalizeToVideoRect(event->pos());

            QPointF angleDelta(static_cast<qreal>(event->angleDelta().x() / m_mouseWheelZoomSensitivity),
                               static_cast<qreal>(event->angleDelta().y() / m_mouseWheelZoomSensitivity));
            auto delta = angleDelta.x() + angleDelta.y();

            if (zoomTo(relativePosition, delta))
            {
                event->accept();
            }
            else
            {
                qInfo() << this << "Failed to zoom" << relativePosition << delta;
            }
        }
    }

    QWidget::wheelEvent(event);
}

bool VideoWidget::zoomTo(QPointF relativePosition, qreal delta)
{
    bool result = false;

    if (auto camera = m_model->fullscreenVideoStreamModel())
    {
        auto viewport = camera->viewport();
        auto newWidth = 1.0 / (1.0 / viewport.width() + delta);
        const auto maxWidth = 1.0 / m_zoomRange.x();
        const auto minWidth = 1.0 / m_zoomRange.y();

        // Limit the zoom by configured boundaries
        newWidth = qBound(minWidth, newWidth, maxWidth);
        auto updatedDelta = viewport.width() - newWidth;

        // Stop moving viewport if we are near maximum/minimum zoom level
        if (abs(viewport.width() - newWidth) > 0.0001)
        {
            // Since we want to retain position under cursor after zoom we need to calculate new viewport and shift
            auto left = viewport.left() + relativePosition.x() * updatedDelta;
            auto top = viewport.top() + relativePosition.y() * updatedDelta;

            tryUpdateViewport(QRectF(left, top, newWidth, newWidth));

            result = true;
        }
    }

    return result;
}

void VideoWidget::moveCenter(QPointF offset)
{
    if (auto camera = m_model->fullscreenVideoStreamModel())
    {
        auto viewport = camera->viewport();
        QPointF zoomedOffset(offset.x() * viewport.width(), 2.0*(offset.y() * viewport.height()));
        viewport.moveCenter(viewport.center() - zoomedOffset);

        tryUpdateViewport(viewport);
    }
}

void VideoWidget::tryUpdateViewport(QRectF updatedViewport)
{
    if (auto camera = m_model->fullscreenVideoStreamModel())
    {
        auto left = updatedViewport.left();
        auto top = updatedViewport.top();
        auto right = updatedViewport.right();
        auto bottom = updatedViewport.bottom();

        // ToDo:: Rewrite this to something more readable!
        if (left < 0)
        {
            double deltaX = abs(left);
            left += deltaX;
            right += deltaX;
        }
        if (right > 1.0)
        {
            double deltaX = abs(right - 1.0);
            left -= deltaX;
            right -= deltaX;
        }

        if (top < 0)
        {
            double deltaY = abs(top);
            top += deltaY;
            bottom += deltaY;
        }
        if (bottom > 1.0)
        {
            double deltaY = abs(bottom - 1.0);
            top -= deltaY;
            bottom -= deltaY;
        }

        QRectF viewport(left, top, right - left, bottom - top);

        camera->setViewport(viewport);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_viewportManipulationEnabled)
    {
        // ignore synthesized mouse events when touch gesture is active
        if (m_pinchGestureActive && event->source() != Qt::MouseEventNotSynthesized)
        {
            m_mouseMoveActive = true;
            return;
        }

        // if we are not pressing the mouse button, or the button was not clicked on us, lets ignore it
        if(m_clickedPos.isNull() || !event->buttons().testFlag(Qt::LeftButton))
        {
            return;
        }

        auto position = normalizeToVideoRect(event->globalPos());

        // move stage item while restricting to limits when necessary
        moveCenter(position - m_clickedPos);

        // save click global position. we are using globalPos() instead of pos() to avoid shaking when widget moves
        m_clickedPos = position;

        // lets not forget to call base after we state that we have accepted this event
        event->accept();
    }

    QWidget::mouseMoveEvent(event);
}

void VideoWidget::mousePressEvent(QMouseEvent* event)
{
    // ignore synthesized mouse events when touch gesture is active
    if (m_pinchGestureActive && event->source() != Qt::MouseEventNotSynthesized)
    {
        return;
    }

    // make sure we are in front and visible
    raise();
    show();

    if(m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running)
    {
        // save click global position. we are using globalPos() instead of pos() to avoid shaking when widget moves
        m_clickedPos = normalizeToVideoRect(event->globalPos());

        // lets not forget to call base after we state that we have accepted this event
        event->accept();
        QWidget::mousePressEvent(event);
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // ignore mouse events synthesized from touch as we handle touch gestures explicitly
    if (m_mouseMoveActive && event->source() != Qt::MouseEventNotSynthesized)
    {
        m_mouseMoveActive = false;
        return;
    }

    if (event->button() == Qt::LeftButton && !m_clickedPos.isNull()
        && m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running)
    {
        // we calculate the new position
        auto position = normalizeToVideoRect(event->globalPos());

        // update position with strict movement restriction to parent widget area
        moveCenter(position - m_clickedPos);

        // reset clickedPos
        m_clickedPos = QPointF();

        // lets not forget to call base after we state that we have accepted this event
        event->accept();
        QWidget::mouseReleaseEvent(event);
    }
}

bool VideoWidget::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::Gesture: {
        return gestureEvent(static_cast<QGestureEvent*>(event));
    }
    }
    return QWidget::event(event);
}

bool VideoWidget::gestureEvent(QGestureEvent *event)
{
    if (QGesture *pinch = event->gesture(Qt::PinchGesture))
    {
        pinchTriggered(static_cast<QPinchGesture *>(pinch));
    }
    else if (QGesture *pan = event->gesture(Qt::PanGesture))
    {
        panTriggered(static_cast<QPanGesture *>(pan));
    }

    event->accept();
    return true;
}

void VideoWidget::pinchTriggered(QPinchGesture *gesture)
{
    // track gesture state for workaround because pan gesture is not sent when pinch is not active
    if (gesture->state() == Qt::GestureStarted)
    {
        if (auto camera = m_model->fullscreenVideoStreamModel())
        {
            m_startZoomFactor = 1.0 / camera->viewport().width();
        }

        m_lastScaleFactor = gesture->totalScaleFactor();
        m_pinchGestureActive = true;
    }
    else if (gesture->state() == Qt::GestureFinished || gesture->state() == Qt::GestureCanceled)
    {
        m_pinchGestureActive = false;
    }

    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        auto point = normalizeToVideoRect(gesture->centerPoint().toPoint());

        zoomTo(point, m_startZoomFactor * (gesture->totalScaleFactor() - m_lastScaleFactor));

        m_lastScaleFactor = gesture->totalScaleFactor();
    }
    if (changeFlags & QPinchGesture::CenterPointChanged) {
        auto delta = normalizeToVideoRect(gesture->centerPoint().toPoint()) -
                     normalizeToVideoRect(gesture->lastCenterPoint().toPoint());
        moveCenter(delta);
    }
}

void VideoWidget::panTriggered(QPanGesture *gesture)
{
    Q_UNUSED(gesture)
    // TODO: Panning when pinching is incomplete. Panning when not pinching is handled via mouseMoveEvent
}

void VideoWidget::startThread()
{
    if (m_renderingThread)
        stopThread();

    makeCurrent();
    m_renderingThread = QSharedPointer<RenderingThread>::create(this);

    m_renderingThread->initialize();

    m_renderingThread->start();
    doneCurrent();
}

void VideoWidget::stopThread()
{
    if (m_renderingThread && m_renderingThread->isRunning())
    {
        m_renderingThread->stop();
        m_renderingThread->quit();
        m_renderingThread->wait();
    }
    m_renderingThread.reset();
}

void VideoWidget::closeEvent(QCloseEvent* /*e*/)
{
    stopThread();
}
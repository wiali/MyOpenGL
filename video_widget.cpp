#include <QApplication>

#include "video_widget.h"
#include "common/utilities.h"

static const GLfloat Vertex[4][2][2] =
{
    { { -1.0f, -1.0f },{ 0.0f, 0.0f } },
    { { 1.0f, -1.0f },{ 1.0f, 0.0f } },
    { { 1.0f, 1.0f },{ 1.0f, 1.0f } },
    { { -1.0f, 1.0f },{ 0.0f, 1.0f } },
};

class RenderingThread : public QThread
{
public:
    RenderingThread(VideoWidget* widget)
        : m_surface(new QOffscreenSurface)
        , m_widget(widget)
        , m_framebufferSize(widget->width(), widget->height())
    {
        m_context = QSharedPointer<QOpenGLContext>::create();
        m_context->setShareContext(m_widget->context());
        m_context->setFormat(m_widget->context()->format());
        m_context->create();
        m_context->moveToThread(this);

        m_surface->setFormat(m_context->format());
        m_surface->create();
        m_surface->moveToThread(this);

        connect(widget, &VideoWidget::geometryChanged, this, &RenderingThread::updateFrameBuffer);
    }
    
    void initialize()
    {
        m_initialized = true;
    }

    void stop()
    {
        m_mutex.lock();
        m_exiting = true;
        m_mutex.unlock();
    }

    void lock()
    {
        m_mutex.lock();
    }

    void unlock()
    {
        m_mutex.unlock();
    }

    void renderFrame()
    {
        auto compositor = m_widget->getCompositor();
        auto renderMapping = m_widget->getRenderMapping();
        if (compositor && !renderMapping.empty())
        {
            if (!m_renderFbo || m_destination != renderMapping.begin()->destination)
            {
                updateFrameBuffer();
                m_destination = renderMapping.begin()->destination;
            }
            compositor->blitFramebuffer(m_renderFbo.data(), renderMapping);
            m_textureId = m_renderFbo->texture();
        }
    }

    GLuint framebufferTexture()
    {
        return m_textureId;
    }

    void update()
    {
        m_waitForFrameReady.wakeAll();
    }

protected:
    void run()
    {
        while (1)
        {
            // Stops the thread if exit flag is set.
            if (m_exiting)
                break;

            //wait the frame ready signal to update frame buffer.
            {
                QMutex frameReadymutex;
                QMutexLocker frameReadyLocker(&frameReadymutex);
                m_waitForFrameReady.wait(&frameReadymutex);
            }

            // Lock the rendering mutex.
            QMutexLocker lock(&m_mutex);

            // Make the OpenGL context current on offscreen surface.
            m_context->makeCurrent(m_surface.data());

            // Initialize if not done.
            if (!m_initialized)
            {
                m_initialized = true;
            }

            // Render the frame
            renderFrame();

            // Release OpenGL context
            m_context->doneCurrent();

            // Notify UI about new frame.
            QMetaObject::invokeMethod(m_widget, "update");
        }
    }

private slots:
    void updateFrameBuffer()
    {
        QOpenGLFramebufferObjectFormat framebufferFormat;
        framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

        m_framebufferSize = QSize(m_widget->width(), m_widget->height());
        m_renderFbo = QSharedPointer<QOpenGLFramebufferObject>::create(
            m_framebufferSize, framebufferFormat);
    }

private:
    // OpenGL context
    QSharedPointer<QOpenGLContext> m_context;
    // Offscreen surface
    QSharedPointer<QOffscreenSurface> m_surface;
    // OpengL widget
    VideoWidget* m_widget;
    // Size of frame buffer
    QSize m_framebufferSize;

    // Rendering mutex
    QMutex m_mutex;
    // True if the application is exiting
    bool m_exiting = false;
    // True if the OpenGL is initialized
    bool m_initialized = false;

    // Framebuffer texture ID for the UI thread.
    GLuint m_textureId = 0;

    // Framebuffer for thread to render the live stream.
    QSharedPointer<QOpenGLFramebufferObject> m_renderFbo;

    QRect m_destination;

    QWaitCondition m_waitForFrameReady;
};


struct VideoWidget::VideoStreamZoomAndPan {
public:
    VideoStreamZoomAndPan() : zoom(1) {}

    qreal zoom;
    QPointF pan;
};

VideoWidget::VideoWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_lastScaleFactor(0)
    , m_startZoomFactor(1)
    , m_viewportManipulationEnabled(true) {
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

void VideoWidget::resizeGL(int width, int height) {
    QOpenGLWidget::resizeGL(width, height);
    updateZoomAndPan();
    emit geometryChanged();
}

void VideoWidget::updateZoomAndPan() {
    if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
        if (m_compositor && m_compositor->frameSize().isValid()) {
            const auto zoomAndPan = m_videoStreamZoomAndPan[firstStream];
            updateZoomAndPan(zoomAndPan.zoom, zoomAndPan.pan);
        }
    }
}

void VideoWidget::paintGL() {

    if (!m_renderingThread)
        return;

    if (m_model) {
        if (m_renderMapping.count() != m_model->selectedVideoStreamSources().count()) {
            updateZoomAndPan();
        }

        if (m_renderMapping.empty())
            return;

        m_renderingThread->lock();

        auto f = QOpenGLContext::currentContext()->functions();

        f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLuint textureId = m_renderingThread->framebufferTexture();

        f->glBindTexture(GL_TEXTURE_2D, textureId);
        glBegin(GL_QUADS);
        for (int i = 0; i < 4; ++i)
        {
            glTexCoord2f(Vertex[i][1][0], Vertex[i][1][1]);
            glVertex2f(Vertex[i][0][0], Vertex[i][0][1]);
        }
        glEnd();

        m_renderingThread->unlock();
    }
}

void VideoWidget::onCompositorUpdated()
{
    if (m_renderingThread)
    {
        m_renderingThread->update();
    }
}

void VideoWidget::setModel(QSharedPointer<LiveCaptureModel> model,
                           QSharedPointer<LiveVideoStreamCompositor> compositor,
                           bool viewportManipulationEnabled){
    m_model = model;
    m_compositor = compositor;
    m_viewportManipulationEnabled = viewportManipulationEnabled;

    updateTransform();

    connect(model.data(), &LiveCaptureModel::videoStreamStateChanged, this, &VideoWidget::onVideoStreamStateChanged);
    connect(m_compositor.data(), &LiveVideoStreamCompositor::updated, this, &VideoWidget::onCompositorUpdated);

    if (!m_viewportManipulationEnabled) {
        connect(model.data(), &LiveCaptureModel::viewportChanged, this, &VideoWidget::updateTransform);
    }

    onVideoStreamStateChanged();
}

void VideoWidget::onVideoStreamStateChanged() {
    foreach(auto connection, m_connections) {
        disconnect(connection);
    }
    m_connections.clear();

    updateZoomAndPan();
}

void VideoWidget::updateTransform() {
    if (m_compositor) {
        if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
            const auto frameSize = m_compositor->frameSize();

            if (m_viewportManipulationEnabled && frameSize.isValid()) {
                const auto zoomAndPan = m_videoStreamZoomAndPan[firstStream];
                m_transform = calculateTransform(zoomAndPan.pan, zoomAndPan.zoom);
                const auto inverse = m_transform.inverted();
                auto absoluteViewport = inverse.mapRect(rect());

                // Invert Y-axis
                absoluteViewport = QRect(absoluteViewport.x(),
                                         frameSize.height() - absoluteViewport.bottom(),
                                         absoluteViewport.width(), absoluteViewport.height());

                QRectF viewport(static_cast<qreal>(absoluteViewport.x()) / static_cast<qreal>(frameSize.width()),
                                static_cast<qreal>(absoluteViewport.y()) / static_cast<qreal>(frameSize.height()),
                                static_cast<qreal>(absoluteViewport.width()) / static_cast<qreal>(frameSize.width()),
                                static_cast<qreal>(absoluteViewport.height()) / static_cast<qreal>(frameSize.height()));

                m_model->setViewport(viewport);

                m_renderMapping = m_compositor->videoStreamMappings(rect(), m_transform);
            } else {
                // Update viewport to make sure it includes only video frame
                auto viewport = m_model->viewport();
                m_transform = Utilities::transformFromViewport(&viewport, frameSize, rect());

                QRectF videoFrameRectangle(viewport.left() * frameSize.width(),
                                           viewport.top() * frameSize.height(),
                                           viewport.width() * frameSize.width(),
                                           viewport.height() * frameSize.height());

                QRect transformRectangle(QPoint(), videoFrameRectangle.size().toSize());

                m_renderMapping = m_compositor->videoStreamMappings(rect(),
                                                                    videoFrameRectangle.toRect(),
                                                                    transformRectangle,
                                                                    m_transform, true);
            }

            update();
        }
    }
}

QTransform VideoWidget::calculateTransform(const QPointF& pan, qreal zoom) {
    QTransform transform;

    if (m_compositor) {
        const auto frameSize = m_compositor->frameSize();
        auto scaledSize = frameSize.scaled(size(), Qt::KeepAspectRatio);

        // Pan the image
        transform.translate(pan.x(), pan.y());

        // Scale to widget size (but maintain aspect ratio)
        transform.scale(static_cast<qreal>(scaledSize.width()) / static_cast<qreal>(frameSize.width()) * zoom,
                        static_cast<qreal>(scaledSize.height()) / static_cast<qreal>(frameSize.height()) * zoom);
    }

    return transform;
}

void VideoWidget::wheelEvent(QWheelEvent* event) {
    if (m_viewportManipulationEnabled) {
        if(m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running &&
                m_model->captureState() == LiveCaptureModel::CaptureState::NotCapturing) {
            const auto relativePosition = event->pos();

            QPointF angleDelta(static_cast<qreal>(event->angleDelta().x() / m_mouseWheelZoomSensitivity),
                               static_cast<qreal>(event->angleDelta().y() / m_mouseWheelZoomSensitivity));
            auto delta = angleDelta.x() + angleDelta.y();

            if (zoomTo(relativePosition, delta)) {
                event->accept();
            }
            else {
                qInfo() << this << "Failed to zoom" << relativePosition << delta;
            }
        }
    }

    QWidget::wheelEvent(event);
}

bool VideoWidget::zoomTo(QPointF relativePosition, qreal delta) {
    bool result = false;

    if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
        const auto zoomAndPan = m_videoStreamZoomAndPan[firstStream];
        auto newZoom = zoomAndPan.zoom + delta;

        // Limit the zoom by configured boundaries
        newZoom = qBound(m_zoomRange.x(), newZoom, m_zoomRange.y());
        const auto updatedDelta = zoomAndPan.zoom - newZoom;

        // Stop moving viewport if we are near maximum/minimum zoom level
        if (qAbs(updatedDelta) > std::numeric_limits<qreal>::epsilon()) {
            // Invert screen y-coordinate because OpenGL is in inverted Y
            relativePosition.setY(height() - relativePosition.y());

            // Calculate where in target image current cursor position points to
            auto absolutePosition = m_transform.inverted().map(relativePosition);

            // Calculate new transformation
            auto newTransform = calculateTransform(zoomAndPan.pan, newZoom);

            // And now calculate image position back to viewer coordinate system with updated transformation
            auto newRelativePosition = newTransform.map(absolutePosition);

            // Since we want to maintain same position under the cursor we calculate the difference and shift
            auto shift = relativePosition - newRelativePosition;

            updateZoomAndPan(newZoom, zoomAndPan.pan + shift);

            result = true;
        }
    }

    return result;
}

void VideoWidget::updateZoomAndPan(qreal zoom, QPointF pan) {
    if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
        const auto frameSize = m_compositor->frameSize();

        if (frameSize.isValid()) {
            const auto unpannedViewport = calculateTransform(QPointF(), zoom).inverted().mapRect(rect());
            const auto scaledSize = frameSize.scaled(size(), Qt::KeepAspectRatio) * zoom;
            const auto newTransform = calculateTransform(pan, zoom);
            auto absoluteViewport = newTransform.inverted().mapRect(rect());
            auto newCenter = rect().center();

            // If the viewport is bigger than image just center it in the viewer
            if (unpannedViewport.width() > frameSize.width()) {
                pan.setX(width() - scaledSize.width());
                pan.rx() /= 2.0;
            } else {
                if (absoluteViewport.left() < 0) absoluteViewport.moveLeft(0);

                if (absoluteViewport.right() > frameSize.width()) {
                    absoluteViewport.moveRight(frameSize.width() - 1);
                }

                // Calculate back to window coordinate system
                newCenter.setX(newTransform.mapRect(absoluteViewport).center().x());
            }

            if (unpannedViewport.height() > frameSize.height()) {
                pan.setY(height() - scaledSize.height());
                pan.ry() /= 2.0;
            } else {
                if (absoluteViewport.top() < 0) absoluteViewport.moveTop(0);

                if (absoluteViewport.bottom() > frameSize.height()) {
                    absoluteViewport.moveBottom(frameSize.height() - 1);
                }                

                // Calculate back to window coordinate system
                newCenter.setY(newTransform.mapRect(absoluteViewport).center().y());
            }

            pan += rect().center() - newCenter;

            m_videoStreamZoomAndPan[firstStream].zoom = zoom;
            m_videoStreamZoomAndPan[firstStream].pan = pan;
            updateTransform();
        }
    }
}

void VideoWidget::moveCenter(QPointF offset) {
    if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
        const auto zoomAndPan = m_videoStreamZoomAndPan[firstStream];

        // Invert Y
        offset.ry() *= -1;
        updateZoomAndPan(zoomAndPan.zoom, zoomAndPan.pan + offset);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_viewportManipulationEnabled) {
        // ignore synthesized mouse events when touch gesture is active
        if (m_pinchGestureActive && event->source() != Qt::MouseEventNotSynthesized) {
            m_mouseMoveActive = true;
        } else if (!m_clickedPos.isNull() && event->buttons().testFlag(Qt::LeftButton)) {
            // if we are not pressing the mouse button, or the button was not clicked on us, lets ignore it
            auto position = event->globalPos();

            // move stage item while restricting to limits when necessary
            moveCenter(position - m_clickedPos);

            // save click global position. we are using globalPos() instead of pos() to avoid shaking when widget moves
            m_clickedPos = position;

            // lets not forget to call base after we state that we have accepted this event
            event->accept();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void VideoWidget::mousePressEvent(QMouseEvent* event) {
    // ignore synthesized mouse events when touch gesture is active
    if (!m_pinchGestureActive || event->source() == Qt::MouseEventNotSynthesized) {
        // make sure we are in front and visible
        raise();
        show();

        if (m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running) {
            // save click global position. we are using globalPos() instead of pos() to avoid shaking when widget moves
            m_clickedPos = event->globalPos();

            // lets not forget to call base after we state that we have accepted this event
            event->accept();
            QWidget::mousePressEvent(event);
        }
    }
}

void VideoWidget::mouseReleaseEvent(QMouseEvent *event) {
    // ignore mouse events synthesized from touch as we handle touch gestures explicitly
    if (m_mouseMoveActive && event->source() != Qt::MouseEventNotSynthesized) {
        m_mouseMoveActive = false;
        return;
    }

    if (event->button() == Qt::LeftButton && !m_clickedPos.isNull()
            && m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running) {
        // we calculate the new position
        auto position = event->globalPos();

        // update position with strict movement restriction to parent widget area
        moveCenter(position - m_clickedPos);

        // reset clickedPos
        m_clickedPos = QPointF();

        // lets not forget to call base after we state that we have accepted this event
        event->accept();
        QWidget::mouseReleaseEvent(event);
    }
}

bool VideoWidget::event(QEvent* event) {
    if (event->type() == QEvent::Gesture) {
        return gestureEvent(static_cast<QGestureEvent*>(event));
    }
    return QWidget::event(event);
}

bool VideoWidget::gestureEvent(QGestureEvent *event) {
    if(m_model->videoStreamState() == LiveCaptureModel::VideoStreamState::Running &&
            m_model->captureState() == LiveCaptureModel::CaptureState::NotCapturing)
    {
        if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
            pinchTriggered(static_cast<QPinchGesture *>(pinch));
        } else if (QGesture *pan = event->gesture(Qt::PanGesture)) {
            panTriggered(static_cast<QPanGesture *>(pan));
        }

        event->accept();
    }
    return true;
}

void VideoWidget::pinchTriggered(QPinchGesture *gesture) {
    // track gesture state for workaround because pan gesture is not sent when pinch is not active
    if (gesture->state() == Qt::GestureStarted) {
        if (auto firstStream = m_model->fullscreenVideoStreamModel()) {
            const auto zoomAndPan = m_videoStreamZoomAndPan[firstStream];
            m_startZoomFactor = zoomAndPan.zoom;
        }

        m_lastScaleFactor = gesture->totalScaleFactor();
        m_pinchGestureActive = true;
    } else if (gesture->state() == Qt::GestureFinished || gesture->state() == Qt::GestureCanceled) {
        m_pinchGestureActive = false;
    }

    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        auto point = gesture->centerPoint().toPoint();
        zoomTo(point, m_startZoomFactor * (gesture->totalScaleFactor() - m_lastScaleFactor));

        m_lastScaleFactor = gesture->totalScaleFactor();
    }
    if (changeFlags & QPinchGesture::CenterPointChanged) {
        auto delta = gesture->centerPoint().toPoint() -
                gesture->lastCenterPoint().toPoint();
        moveCenter(delta);
    }
}

void VideoWidget::panTriggered(QPanGesture *gesture) {
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

void VideoWidget::initializeGL()
{
    glEnable(GL_TEXTURE_2D);
    startThread();
}

VideoWidget::~VideoWidget()
{
    stopThread();
}

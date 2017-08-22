#include "ink_layer_glwidget.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QTime>

#define GL_GLEXT_PROTOTYPES

const int PROGRAM_VERTEX_ATTRIBUTE = 0;
const int PROGRAM_COLOR_ATTRIBUTE = 1;
const int SMALL_PEN_SIZE = 10;
const int ERASER_SIZE = 30;
const int BASE_PRESSURE = (1024 / 2);

const int CIRCLE_POINTS_NUM = 100;
const float EPSILON = 0.00001;
const int VBO_SIZE = 1000000;

QString loadProgram(QString fileLocation)
{
    QFile file(fileLocation);
    file.open(QFile::ReadOnly);
    return file.readAll();
}

QString vertexProgram()
{
    return loadProgram("./assets/shaders/lines.vert");
}

QString fragProgram()
{
    return loadProgram("./assets/shaders/lines.frag");
}

QString geomProgram()
{
    return loadProgram("./assets/shaders/lines1.geom");
}

QImage loadTexture()
{
    return QImage("./assets/textures/pattern1.png");
}


QVector<QPointF> plot_line(QPointF a, QPointF b, float width)
{
    QVector<QPointF> ret;

    int x0 = a.x();
    int y0 = a.y();
    int x1 = b.x();
    int y1 = b.y();

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */

    for (;;) {  /* loop */
        ret.push_back(QPointF(x0, y0));
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }

    return ret;
}

QVector<QPointF> interpolation(QPointF a, QPointF b, float width)
{
    //width = 2;
    QVector<QPointF> ret;

    int dx = b.x() - a.x();
    int dy = b.y() - a.y();

    if (abs(dx) < width && abs(dy) < width)
    {
        ret.push_back(a);
        ret.push_back(b);
        return ret;
    }

    QPointF temp;

    bool xDirection = false;
    bool yDirection = false;
    float slop_x = 0.0f;
    float slop_y = 0.0f;
    if (abs(dy) < EPSILON)
    {
        slop_x = dx > 0 ? 1.0 : -1.0;
        xDirection = true;
    }
    if (abs(dx) < EPSILON)
    {
        slop_y = dy > 0 ? 1.0 : -1.0;
        yDirection = true;
    }

    if (!xDirection && !yDirection)
    {
        slop_x = dx > 0 ? 1.0 : -1.0;
        slop_y = (dy > 0 ? 1.0 : -1.0) * fabs(1.0 * dy / dx);
    }

    int steps = xDirection ? dx : yDirection ? dy : dx;

    for (int i = 0; i < abs(steps); i+= width)
    {
        QPointF c(a.x() + i*slop_x, a.y() + i*slop_y);
        ret.push_back(c);
    }
    
    return ret;
}

InkLayerGLWidget::InkLayerGLWidget(QWidget* mockParent, QWidget *parent)
    : QOpenGLWidget(parent),
    m_mockParent(mockParent),
    m_clearColor(Qt::black),
    m_program(new QOpenGLShaderProgram),
    m_color(Qt::yellow)
    , m_basePenWidth(SMALL_PEN_SIZE)
    , m_eraserSize(ERASER_SIZE)
    , m_eraserMode(false)
    , m_penEraserMode(false)
    , m_penMode(true)
    , m_penDrawing(false)
    , m_enablePen(true)
    , m_enableRemoveStroke(true)
    , m_mouseDrawing(false)
    , m_penPointColor(Qt::black)
    , m_strokes(new InkData())
    , m_vertex_index(0)
{
    setWindowFlags(Qt::SubWindow);
    setAutoFillBackground(false);
    // lets make sure we have a transparent background
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    //setAttribute(Qt::WA_PaintOnScreen);
    //setAttribute(Qt::WA_TransparentForMouseEvents);
    //setAttribute(Qt::WA_ShowWithoutActivating);

    setWindowFlags(windowFlags() | Qt::Tool  | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput);

    m_mockParent = this;

    // and get prepared to adapt if main window get resized
    connect(m_mockParent, SIGNAL(penHoverEntered(const POINTER_PEN_INFO&)),
        this, SLOT(onPenHoverEntered(const POINTER_PEN_INFO&)));
    connect(m_mockParent, SIGNAL(penHoverExited(const POINTER_PEN_INFO&)),
        this, SLOT(onPenHoverExited(const POINTER_PEN_INFO&)));
    connect(m_mockParent, SIGNAL(penPressDown(const POINTER_PEN_INFO&)),
        this, SLOT(onPenDown(const POINTER_PEN_INFO&)));
    connect(m_mockParent, SIGNAL(penPressUp(const POINTER_PEN_INFO&)),
        this, SLOT(onPenUp(const POINTER_PEN_INFO&)));
    connect(m_mockParent, SIGNAL(penMove(const POINTER_PEN_INFO&)),
        this, SLOT(onPenMove(const POINTER_PEN_INFO&)));

    setInkData(m_strokes);
}

InkLayerGLWidget::~InkLayerGLWidget()
{
    makeCurrent();
    m_color_vbo.destroy();
    m_mesh_vbo.destroy();
    doneCurrent();
}

void InkLayerGLWidget::setClearColor(const QColor &color)
{
    m_clearColor = color;
    update();
}

void InkLayerGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    m_textures = QSharedPointer<QOpenGLTexture>::create(loadTexture());

    m_color_vbo.create();
    m_color_vbo.setUsagePattern(QOpenGLBuffer::DynamicCopy);
    m_color_vbo.bind();
    m_vertColors.resize(VBO_SIZE);
    for (int i = 0; i < VBO_SIZE; i++)
    {
        m_vertColors[i] = { 1.0f, 1.0f,0.0f };
    }
    m_color_vbo.allocate(m_vertColors.constData(), m_vertColors.count() * sizeof(QVector3D));

    m_mesh_vbo.create();
    m_mesh_vbo.setUsagePattern(QOpenGLBuffer::DynamicCopy);
    m_mesh_vbo.bind();
    m_vertices.resize(VBO_SIZE);
    m_mesh_vbo.allocate(m_vertices.constData(), m_vertices.count() * sizeof(QVector3D));

    glEnable(GL_DEPTH_TEST);

    //glEnable(GL_LINE_SMOOTH);
    ////glShadeModel(GL_FLAT);
    //glEnable(GL_POINT_SMOOTH);
    //glHint(GL_POINT_SMOOTH, GL_NICEST);
    //glEnable(GL_POLYGON_SMOOTH);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    //glEnable(GL_MULTISAMPLE);

    //glDisable(GL_LIGHT0);
    //glDisable(GL_LIGHTING);
    //glDisable(GL_TEXTURE_2D);
    //glDisable(GL_BLEND);

    //glPolygonMode(GL_BACK, GL_FILL);

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_DST_ALPHA, GL_ONE);
    //glBlendFunc(GL_DST_ALPHA, GL_ONE);


    //When you switch the Y-axis direction every front-face becomes a back-face, 
    //so that means you need to either disable face-culling or change the winding of the front-face.
    //glFrontFace(GL_BACK);
    
    //glDisable(GL_CULL_FACE);
    //glEnable(GL_FRONT_FACE);

    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    QString vsrc = vertexProgram();
    vshader->compileSourceCode(vsrc);

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    QString fsrc = fragProgram();
    fshader->compileSourceCode(fsrc);

    QOpenGLShader *gshader = new QOpenGLShader(QOpenGLShader::Geometry, this);
    QString gsrc = geomProgram();
    gshader->compileSourceCode(gsrc);

    m_program->addShader(vshader);
    m_program->addShader(fshader);
    m_program->addShader(gshader);

    m_program->bindAttributeLocation("ciPosition", PROGRAM_VERTEX_ATTRIBUTE);
    m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);

    m_program->bindAttributeLocation("ciColor", PROGRAM_COLOR_ATTRIBUTE);
    m_program->enableAttributeArray(PROGRAM_COLOR_ATTRIBUTE);

    m_program->enableAttributeArray(PROGRAM_COLOR_ATTRIBUTE);
    m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);

    m_program->link();

    m_win_scale = m_program->uniformLocation("WIN_SCALE");
    m_miter_limit = m_program->uniformLocation("MITER_LIMIT");
    m_thickness = m_program->uniformLocation("THICKNESS");
    m_matrixUniform = m_program->uniformLocation("ciModelViewProjection");

    m_program->bind();

    QMatrix4x4 m;
    //m.ortho(-0.5f, +0.5f, +0.5f, -0.5f, 4.0f, 15.0f);
    m.ortho(0.0f, width(), height(), 0.0f, 4.0f, 15.0f);
    m.translate(0.0f, 0.0f, -10.0f);

    //OpenGl coordinates is inverse Y with the window coordinates.
    //m.scale(1.0f, -1.0f, -1.0f);

    //glViewport(0, 0, width(), height());

    m_program->setUniformValue(m_matrixUniform, m);
    m_program->setUniformValue(m_win_scale, size());
    m_program->setUniformValue(m_miter_limit, 0.75f);
    m_program->setUniformValue(m_thickness, 50.0f);    
}

void InkLayerGLWidget::paintGL()
{
    glClearColor(m_clearColor.redF(), m_clearColor.greenF(), m_clearColor.blueF(), m_clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QTime time;
    time.start();

    if (m_strokes)
    {
        // Draw current stroke
        auto currentStroke = m_strokes->currentStroke();

        if (currentStroke->pointCount() > 1)
        {
            m_vertex_index = 0;
            render();
        }
        qInfo() << "Aden1: " << time.elapsed();
    }

    if (m_vertex_index >0)
    {
        time.restart();

        m_color_vbo.bind();
        m_color_vbo.write(0, &m_vertColors[0], m_vertColors.count() * sizeof(QVector3D));
        m_program->setAttributeBuffer(PROGRAM_COLOR_ATTRIBUTE, GL_FLOAT, 0, 3);

        m_mesh_vbo.bind();
        m_mesh_vbo.write(0, &m_vertices[0], m_vertices.count() * sizeof(QVector3D));
        m_program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0, 3);

        glDrawElements(GL_LINES_ADJACENCY_EXT, m_indices.size() * sizeof(uint16_t), GL_UNSIGNED_SHORT, &m_indices[0]);

        m_textures->bind();

        //glMultiDrawArrays(GL_POLYGON, &plygons_starts[0], &eachPolygonCounts[0], plygonCount);

        qInfo() << "Aden2: " << time.elapsed();
    }
}

void InkLayerGLWidget::resizeGL(int width, int height)
{
    int side = qMin(width, height);
    //glViewport((width - side) / 2, (height - side) / 2, side, side);
    glViewport(0,0, width, height);
}

void InkLayerGLWidget::setPenMode(bool penMode)
{
    m_penMode = penMode;
    setAttribute(Qt::WA_TransparentForMouseEvents, m_penMode);
}

void InkLayerGLWidget::eraseStroke(const QPoint& pos)
{
    if (!m_enableRemoveStroke)
    {
        emit inkDataErasing(mapToGlobal(pos));
        return;
    }

    if (m_strokes.isNull())
    {
        qWarning() << "Please initialize the ink data firstly!";
        return;
    }

    bool erased = false;
    int strokeCount = m_strokes->strokeCount();

    for (int i = strokeCount - 1; i >= 0; i--)
    {
        auto stroke = m_strokes->stroke(i);
        int ptCount = stroke->pointCount();
        for (int j = 0; j < ptCount; j++)
        {
            auto inkPt = stroke->point(j);
            if (QPoint(pos - inkPt.point).manhattanLength() < m_eraserSize)
            {
                m_strokes->removeStroke(i);
                erased = true;
                break;
            }
        }
    }

    if (erased) update();

    emit inkDataErasing(pos);
}

void InkLayerGLWidget::setInkData(QSharedPointer<InkData> strokes)
{
    m_strokes = strokes;

    emit inkDataChanged(strokes);
}

void InkLayerGLWidget::setColor(const QColor &c)
{
    if (c.isValid())
        m_color = c;
}

void InkLayerGLWidget::setPenPointColor(const QColor &penPointColor)
{
    if (penPointColor.isValid())
    {
        m_penPointColor = penPointColor;
        changeCursor(m_basePenWidth);
    }
}

QColor InkLayerGLWidget::penPointColor() const
{
    return m_penPointColor;
}

QColor InkLayerGLWidget::color() const
{
    return m_color;
}

QString InkLayerGLWidget::penInfoStr(const POINTER_PEN_INFO& penInfo)
{
    QPoint position(penInfo.pointerInfo.ptPixelLocation.x, penInfo.pointerInfo.ptPixelLocation.y);
    QString result = QString("pressure: %1; Position [%2, %3]").arg(penInfo.pressure)
        .arg(mapFromGlobal(position).x())
        .arg(mapFromGlobal(position).y());

    if ((penInfo.penFlags & PEN_FLAG_ERASER) || (penInfo.penFlags & PEN_FLAG_INVERTED))
    {
        result += "; Eraser button has pressed";
    }
    else if (penInfo.penFlags & PEN_FLAG_BARREL)
    {
        result += "Barrel button has pressed";
    }

    return result;
}

void InkLayerGLWidget::onPenDown(const POINTER_PEN_INFO& penInfo)
{
    //qDebug() << "Pen Down : " << penInfoStr(penInfo);
    if (m_enablePen && penInfo.pressure > 0)
    {
        QPoint posInGlobal(penInfo.pointerInfo.ptPixelLocation.x, penInfo.pointerInfo.ptPixelLocation.y);

        m_penDrawing = true;

        QPoint pt(mapFromGlobal(posInGlobal));

        // Drawing with mouse at this moment.
        if (m_strokes->currentStroke()->pointCount() > 0)
        {
            addStroke();
        }

        if (m_eraserMode)
        {
            // Erase line
            eraseStroke(pt);
        }
        else if ((penInfo.penFlags & PEN_FLAG_ERASER) || (penInfo.penFlags & PEN_FLAG_INVERTED))
        {
            // Entering Pen Eraser Mode
            m_penEraserMode = true;

            // Erase line
            eraseStroke(pt);
        }
        else if (penInfo.penFlags & PEN_FLAG_BARREL)
        {
            // Pen barrel button has been pressed.
        }
        else
        {
            double width = m_basePenWidth * penInfo.pressure * 1.0 / BASE_PRESSURE;
            addPoint(pt, width);
        }
    }
}

void InkLayerGLWidget::onPenUp(const POINTER_PEN_INFO& penInfo)
{
    Q_UNUSED(penInfo)

        if (!m_penDrawing) return;

    //qDebug() << "Pen Up" << penInfoStr(penInfo);
    if (m_penEraserMode)
    {
        m_penEraserMode = false;
    }
    else
    {
        addStroke();
    }

    m_penDrawing = false;
}

void InkLayerGLWidget::onPenMove(const POINTER_PEN_INFO& penInfo)
{
    //qDebug() << "Pen move: "  << penInfoStr(penInfo);
    if (m_enablePen && penInfo.pressure > 0)
    {
        if (!m_penDrawing) return;

        QPoint posInGlobal(penInfo.pointerInfo.ptPixelLocation.x, penInfo.pointerInfo.ptPixelLocation.y);
        QPoint pt(mapFromGlobal(posInGlobal));
        if (m_penEraserMode || m_eraserMode)
        {
            // Erase line
            eraseStroke(pt);
        }
        else
        {
            double width = m_basePenWidth * penInfo.pressure * 1.0 / BASE_PRESSURE;
            addPoint(pt, width);
        }
    }
}

void InkLayerGLWidget::addPoint(const QPoint& point, double width)
{
    if (width > 0)
    {
        auto currentStroke = m_strokes->currentStroke();
        currentStroke->addPoint(point, width);
        currentStroke->setColor(m_color);

        update();
        //update(currentStroke->boundRect());

        emit inkPointAdded(point, width);
    }
}

void InkLayerGLWidget::addStroke()
{
    if (m_strokes.isNull())
    {
        qWarning() << "Please initialize the ink data first!";
        return;
    }

    if (m_strokes->currentStroke()->pointCount() > 0)
    {
        auto r = m_strokes->currentStroke()->boundRect();
        m_strokes->addCurrentStroke();
        emit inkStrokeAdded();
    }
}

void InkLayerGLWidget::updateColor(const QColor& color)
{
    m_color = QColor(color);
}


void InkLayerGLWidget::penSizeChanged(int penSize)
{
    m_basePenWidth = penSize;
    changeCursor(m_basePenWidth);
}

void InkLayerGLWidget::colorChanged(const QColor& color)
{
    updateColor(color);
}

void InkLayerGLWidget::enterEraserMode()
{
    if (m_eraserMode) return;

    changeCursor(m_eraserSize);
    update();

    m_eraserMode = true;
}

void InkLayerGLWidget::enablePen(bool enable)
{
    m_enablePen = enable;
}

void InkLayerGLWidget::enableRemoveStroke(bool enable)
{
    m_enableRemoveStroke = enable;
}

void InkLayerGLWidget::enterDrawMode()
{
    changeCursor(m_basePenWidth);

    if (m_eraserMode)
    {
        m_eraserMode = false;
    }
}

void InkLayerGLWidget::changeCursor(int penSize)
{
    QPixmap *pix = new QPixmap(penSize + 2, penSize + 2);
    pix->fill(Qt::transparent);
    QPainter *paint = new QPainter(pix);
    paint->setRenderHints(QPainter::HighQualityAntialiasing | QPainter::SmoothPixmapTransform);
    QPen pen(m_penPointColor);
    pen.setWidth(2);
    paint->setPen(pen);
    paint->drawEllipse(2, 2, penSize - 2, penSize - 2);
    QCursor cursor = QCursor(*pix, -penSize / 2, -penSize / 2);
    setCursor(cursor);
}

void InkLayerGLWidget::hideEvent(QHideEvent *event)
{
    m_enablePen = false;

    return QOpenGLWidget::hideEvent(event);
}

void InkLayerGLWidget::showEvent(QShowEvent *event)
{
    m_enablePen = true;

    return QOpenGLWidget::showEvent(event);
}

void InkLayerGLWidget::mousePressEvent(QMouseEvent *event)
{
    m_strokes->clear();

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE * 1;

    emit penPressDown(penInfo);
}

void InkLayerGLWidget::mouseReleaseEvent(QMouseEvent *event)
{

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE *1;
    
    emit penPressUp(penInfo);
}

void InkLayerGLWidget::mouseMoveEvent(QMouseEvent *event)
{

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE * 1;

    emit penMove(penInfo);
}

void InkLayerGLWidget::render()
{
    QVector<QVector2D> m_points;

    // Draw current stroke
    auto currentStroke = m_strokes->currentStroke();

    int ptCount = currentStroke->pointCount();
    if (ptCount < 2) return;

    float pen_width;
    float scale = 1.0f;

    for (int i = 1; i < ptCount; i++)
    {
        pen_width = currentStroke->getPoint(i).second*scale;

        auto ptStart = QVector2D(currentStroke->getPoint(i - 1).first.x()*scale, currentStroke->getPoint(i - 1).first.y()*scale);
        auto ptEnd = QVector2D(currentStroke->getPoint(i).first.x()*scale, currentStroke->getPoint(i).first.y()*scale);

        m_points << ptStart << ptEnd;
    }

    // brute-force method: recreate mesh if anything changed

    // create a new vector that can contain 3D vertices

    // to improve performance, make room for the vertices + 2 adjacency vertices
    //m_vertices.reserve(m_points.size() + 2);

    // first, add an adjacency vertex at the beginning
    m_vertices[m_vertex_index++] = (2.0f * QVector3D(m_points[0], 0) - QVector3D(m_points[1], 0));

    // next, add all 2D points as 3D vertices
    QVector<QVector2D>::iterator itr;
    for (itr = m_points.begin(); itr != m_points.end(); ++itr)
        m_vertices[m_vertex_index++] = (QVector3D(*itr, 0));

    // next, add an adjacency vertex at the end
    size_t n = m_points.size();
    m_vertices[m_vertex_index++] = (2.0f * QVector3D(m_points[n - 1], 0) - QVector3D(m_points[n - 2], 0));

    // now that we have a list of vertices, create the index buffer
    n = m_vertex_index - 2;

    m_indices.clear();
    m_indices.reserve(n * 4);

    for (size_t i = 1; i < n; ++i)
    {
        m_indices.push_back(i - 1);
        m_indices.push_back(i);
        m_indices.push_back(i + 1);
        m_indices.push_back(i + 2);
    }

    // finally, create the mesh
}

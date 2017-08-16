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
    //m_vbo.destroy();
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

    //makeObject();
    m_textures = QSharedPointer<QOpenGLTexture>::create(loadTexture());

    m_vertex_vbo.create();
    m_vertex_vbo.setUsagePattern(QOpenGLBuffer::DynamicCopy);
    m_vertex_vbo.bind();
    m_vertPoints.resize(VBO_SIZE * 2);
    m_vertex_vbo.allocate(m_vertPoints.constData(), m_vertPoints.count() * sizeof(GLfloat));

    m_color_vbo.create();
    m_color_vbo.setUsagePattern(QOpenGLBuffer::DynamicCopy);
    m_color_vbo.bind();
    m_vertColors.resize(VBO_SIZE * 3);
    for (int i = 0; i < VBO_SIZE; i++)
    {
        m_vertColors[i * 3] = 1.0f;
        m_vertColors[i * 3 + 1] = 1.0f;
        m_vertColors[i * 3 + 2] = 0.0f;
    }
    m_color_vbo.allocate(m_vertColors.constData(), m_vertColors.count() * sizeof(GLfloat));

    m_mesh_vbo.create();
    m_mesh_vbo.setUsagePattern(QOpenGLBuffer::DynamicCopy);
    m_mesh_vbo.bind();
    m_vertices.resize(VBO_SIZE * 3);
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
    
    glDisable(GL_CULL_FACE);
    glEnable(GL_FRONT_FACE);

    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    static const char *vsrc = vertexProgram().toStdString().c_str();
    vshader->compileSourceCode(vsrc);

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    static const char *fsrc = fragProgram().toStdString().c_str();
    fshader->compileSourceCode(fsrc);

    QOpenGLShader *gshader = new QOpenGLShader(QOpenGLShader::Geometry, this);
    static const char *gsrc = geomProgram().toStdString().c_str();

    gshader->compileSourceCode(gsrc);

    m_program->addShader(vshader);
    m_program->addShader(fshader);
    m_program->addShader(gshader);

    m_program->bindAttributeLocation("ciPosition", PROGRAM_VERTEX_ATTRIBUTE);
    m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);

    //m_program->bindAttributeLocation("colAttr", PROGRAM_COLOR_ATTRIBUTE);
    //m_program->enableAttributeArray(PROGRAM_COLOR_ATTRIBUTE);

    m_program->link();

    /*  glEnableVertexAttribArray(PROGRAM_VERTEX_ATTRIBUTE);
      glEnableVertexAttribArray(PROGRAM_TEXCOORD_ATTRIBUTE);*/
    

    //m_posAttr = m_program->attributeLocation("vertAttr");
    //m_colAttr = m_program->attributeLocation("colAttr");
    m_win_scale = m_program->uniformLocation("WIN_SCALE");
    m_miter_limit = m_program->uniformLocation("MITER_LIMIT");
    m_thickness = m_program->uniformLocation("THICKNESS");
    m_matrixUniform = m_program->uniformLocation("matrix");

    m_program->bind();

    QMatrix4x4 m;
    m.ortho(-0.5f, +0.5f, +0.5f, -0.5f, 4.0f, 15.0f);
    m.translate(0.0f, 0.0f, -10.0f);

    //OpenGl coordinates is inverse Y with the window coordinates.
    m.scale(1.0f, -1.0f, -1.0f);

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
        //m_all_lines.clear();
        //m_polygonCounts.clear();
        // Draw current stroke
        auto currentStroke = m_strokes->currentStroke();

        if (currentStroke->pointCount() > 1)
        {
            m_vertex_index = 0;
            m_polygonCounts.clear();

            //draw(currentStroke);
            render();
        }
        qInfo() << "Aden1: " << time.elapsed();
    }

    time.restart();
    //QVector<GLfloat> vertPoints;
    //for (auto line : m_all_lines)
    //    vertPoints.append(line.second);

    qInfo() << "Aden2: " << time.elapsed();

    time.restart();

    int floatCounts = m_vertex_index;
    int pointCounts = m_vertex_index / 2;

    //if (floatCounts > m_vertColors.size())
    {
        //QVector<GLfloat> vertColors(floatCounts - m_vertColors.size());
        //int deltaCount = vertColors.size() / 3;
        //for (int i = 0; i < pointCounts; i++)
        //{
        //    m_vertColors[i * 3] = 1.0f;
        //    m_vertColors[i * 3 + 1] = 1.0f;
        //    m_vertColors[i * 3 + 2] = 0.0f;
        //}

        //m_vertColors.append(vertColors);
    }

    qInfo() << "Aden3: " << time.elapsed();

    if (0)//vertPoints.empty())
    {
        //have to clockwise
        GLfloat vertData[] =
        {
            -0.2f, -0.2f, 0.f,            
            -0.2f, +0.2f, 0.f,            
            +0.2f, +0.2f, 0.f,
            +0.2f, -0.2f, 0.f,
        };

        GLfloat colors[] =
        {
            1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
        };

        for (int i = 0; i <12; i++)
        {
            m_vertPoints << vertData[i];
            m_vertColors << colors[i];
        }

        pointCounts = 4;
    }

    time.restart();

    //glVertexAttribPointer(m_posAttr, 3, GL_FLOAT, GL_FALSE, 0, &m_vertPoints[0]);

    //glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, &m_vertColors[0]);

    qInfo() << "Aden4: " << time.elapsed();

    time.restart();

    if (m_vertex_index >0)
    {
        int plygonCount = m_polygonCounts.size();
        QVector<GLsizei> eachPolygonCounts;
        QVector<GLint> plygons_starts;

        plygons_starts << 0;
        for (int i = 0; i < plygonCount; i++)
        {
            eachPolygonCounts << m_polygonCounts[i];
            
            if(i != 0 )
                plygons_starts <<  i* m_polygonCounts[i];
        }

        qInfo() << "Aden5: " << time.elapsed();

        time.restart();

        //m_vertex_vbo.bind();
        //m_vertex_vbo.write(0, &m_vertPoints[0], m_vertPoints.count() * sizeof(GLfloat));
        //m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
        //m_program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0, 2);

        //m_color_vbo.bind();
        //m_color_vbo.write(0, &m_vertColors[0], m_vertColors.count() * sizeof(GLfloat));
        //m_program->enableAttributeArray(PROGRAM_COLOR_ATTRIBUTE);
        //m_program->setAttributeBuffer(PROGRAM_COLOR_ATTRIBUTE, GL_FLOAT, 0, 3);

        m_mesh_vbo.bind();
        m_mesh_vbo.write(0, &m_vertices[0], m_vertices.count() * sizeof(QVector3D));
        m_program->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
        m_program->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0, 3);

        glDrawElements(GL_LINES_ADJACENCY_EXT, m_indices.size() * sizeof(uint16_t), GL_UNSIGNED_SHORT, m_indices.data());

        m_textures->bind();

        //glMultiDrawArrays(GL_POLYGON, &plygons_starts[0], &eachPolygonCounts[0], plygonCount);

        qInfo() << "Aden6: " << time.elapsed();
    }
    else
        glDrawArrays( GL_POLYGON, 0, pointCounts);


}

void InkLayerGLWidget::resizeGL(int width, int height)
{
    int side = qMin(width, height);
    glViewport((width - side) / 2, (height - side) / 2, side, side);
}

//void InkLayerGLWidget::mousePressEvent(QMouseEvent *event)
//{
//    lastPos = event->pos();
//}

//void InkLayerGLWidget::mouseMoveEvent(QMouseEvent *event)
//{
//    int dx = event->x() - lastPos.x();
//    int dy = event->y() - lastPos.y();
//
//    if (event->buttons() & Qt::LeftButton) {
//        rotateBy(8 * dy, 8 * dx, 0);
//    }
//    else if (event->buttons() & Qt::RightButton) {
//        rotateBy(8 * dy, 0, 8 * dx);
//    }
//    lastPos = event->pos();
//}

//void InkLayerGLWidget::mouseReleaseEvent(QMouseEvent * /* event */)
//{
//    emit clicked();
//}

void InkLayerGLWidget::makeObject()
{
    //start from bottom left 
    static const int coords_pos[4][3] =
    {
        { -1, -1, 0 },{ +1, -1, 0 },{ +1, +1, 0 },{ -1, +1, 0 }
    };

    QVector<GLfloat> vertData;
    for (int i = 0; i < 4; ++i)
    {
        // vertex position
        vertData.append(coords_pos[i][0]);
        vertData.append(coords_pos[i][1]);
        vertData.append(coords_pos[i][2]);
    }

    //m_vbo.create();
    //m_vbo.bind();
    //m_vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
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
                updatePixmap(stroke->boundRect());
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

    updatePixmap();
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

void InkLayerGLWidget::updatePixmap(const QRect& clipRect)
{
    return;
    if (m_strokes.isNull())
    {
        return;
    }

    QRect clipRegion(clipRect);
    if (clipRect.isNull() || clipRect.isEmpty() || !clipRect.isValid())
    {
        clipRegion = QRect(0, 0, width(), height());
    }

    for (int i = 0; i < m_strokes->strokeCount(); i++)
    {
        auto stroke = m_strokes->stroke(i);
        if (stroke->boundRect().intersects(clipRegion))
        {
            QPair<QColor, QVector<float>> lines;
            draw(stroke);
        }
    }
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
        updatePixmap(r);
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


void InkLayerGLWidget::drawSmoothStroke(float pen_width, const QPointF& previous, const QPointF& point,
    const QPointF& next, QVector<float>& tiangle_points) 
{
    QPointF c1 = (previous + point) / 2;
    QPointF c2 = (next + point) / 2;
    QPointF cc = (c1 + c2) / 2;
    QPointF adjust = (point + cc) / 2;

    // Not smooth enough! Do more process.
    if ((adjust - cc).manhattanLength() >1 )    
    {
        getTriangles(pen_width, c1, (c1 + adjust) / 2, tiangle_points);
        drawSmoothStroke(pen_width, c1, adjust, c2, tiangle_points);
        getTriangles(pen_width, (c2 + adjust) / 2, c2, tiangle_points);
    }
    else
    {
        getTriangles(pen_width, c1, adjust, tiangle_points);
        getTriangles(pen_width, adjust, c2, tiangle_points);
    }
}

QVector2D InkLayerGLWidget::normalize(QVector2D& vec3)
{
    float mat_width = width();
    float mat_height = height();

    vec3.setX( (vec3.x() - mat_width / 2.0) / mat_width);
    //vec3.setY((mat_height - (mat_height / 2.0 - vec3.y())) / mat_height);
    vec3.setY( (mat_height / 2.0 - vec3.y()) / mat_height);

    return vec3;
}

void InkLayerGLWidget::getTriangles(float width, const QPointF& start, const QPointF& end, QVector<float>& points)
{
    width /= 2.0;

    auto start1 = start;
    auto end1 = end;

    //auto start1 = QPointF(100, 100);
    //auto end1 = QPointF(200, 200);

    //float angle = atan2(end.y() - start.y(), end.x() - start.x());
    //float t2sina1 = width * sin(angle);
    //float t2cosa1 = width * cos(angle);
    //float t2sina2 = width * sin(angle);
    //float t2cosa2 = width * cos(angle);

    //points << start.x() + t2sina1 << start.y() - t2cosa1 << end.x() + t2sina2 << end.y() - t2cosa2 <<
    //    end.x() - t2sina2 << end.y() + t2cosa2 << end.x() - t2sina2 << end.y() + t2cosa2 <<
    //    start.x() - t2sina1 << start.y() + t2cosa1 << start.x() + t2sina1 << start.y() - t2cosa1;

    //https://stackoverflow.com/questions/101718/drawing-a-variable-width-line-in-opengl-no-gllinewidth
    // find line between p1 and p2

    QVector2D p1(start1.x(), start1.y());
    QVector2D p2(end1.x(), end1.y());

    QVector2D lineP1P2 = p2 - p1;
    //if we define dx = x2 - x1 and dy = y2 - y1, then the normals are(-dy, dx) and (dy, -dx).

    QVector2D vNormal = QVector2D(lineP1P2.y(), -1.0*lineP1P2.x());
    vNormal.normalize();

    //ABCD conter clockwise
    auto A = p1 + vNormal*(width);
    auto B = p1 - vNormal*(width);

    auto C = p2 - vNormal*(width);
    auto D = p2 + vNormal*(width);

    //if(A.y() < B.y())
    //    A.setY(B.y());
    //else
    //    B.setY(A.y());

    //if (C.y() < D.y())
    //    C.setY(D.y());
    //else
    //    D.setY(C.y());   

    normalize(A);
    normalize(B);
    normalize(C);
    normalize(D);

    points << A.x() << A.y() << B.x() << B.y() << C.x() << C.y() << D.x() << D.y();
    //points << A.x() << A.y() << A.z() << D.x() << D.y() << D.z() << C.x() << C.y() << C.z() << B.x() << B.y() << B.z() ;

}

void InkLayerGLWidget::draw(QSharedPointer<InkStroke> stroke, bool mono, double scale )
{
    int ptCount = stroke->pointCount();
    if (ptCount < 2) return;

    //QPair<QColor, QVector<float>> lines; //float[6] = start_x, start_y, end_x, end_y

    QColor color(Qt::black);
    if (!mono)
    {
        color = stroke->color();
    }

    //lines.first = color;

    float pen_width;

    for (int i = 1; i < ptCount; i++)
    {
        pen_width = stroke->getPoint(i).second*scale;

        auto ptStart = QPointF(stroke->getPoint(i - 1).first.x()*scale, stroke->getPoint(i - 1).first.y()*scale);
        auto ptEnd = QPointF(stroke->getPoint(i).first.x()*scale, stroke->getPoint(i).first.y()*scale);

        //QPointF vector = ptEnd - ptStart;
        //if (vector.manhattanLength() > pen_width/2.0)
        {
            QVector<QPointF> smoothPts = plot_line(ptStart, ptEnd, pen_width / 2.0);//interpolation

            for (auto smooth_point : smoothPts)
            {
                int previousSize = m_vertex_index;
                draw_circle(smooth_point.x(), smooth_point.y(), pen_width / 2.0);
                m_polygonCounts << (m_vertex_index - previousSize) / 2;
            }
        }
    }
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

void draw_solid_circle(float x, float y, float radius)
{
    int count;
    int sections = 200;

    GLfloat TWOPI = 2.0f * 3.14159f;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);

    for (count = 0; count <= sections; count++)
    {
        glVertex2f(x + radius*cos(count*TWOPI / sections), y + radius*sin(count*TWOPI / sections));
    }
    glEnd();
}

void InkLayerGLWidget::draw_circle(float x, float y, float radius)
{
    plot_circle(x, y, radius);
    return;

    static const float angle = 2.0f * 3.1416f / (CIRCLE_POINTS_NUM-1);

    double angle1=0.0;
    QVector2D point(x+radius * cos(0.0), y+radius * sin(0.0));
    normalize(point);
    QVector<float> polygon;
    polygon << point.x() << point.y();
    for (int i=0; i<CIRCLE_POINTS_NUM-1; i++)
    {
        point = QVector2D(x+radius * cos(angle1), y+radius *sin(angle1));
        normalize(point);
        polygon << point.x() << point.y();
        angle1 += angle;
    }
}



void InkLayerGLWidget::plot_circle(int xm, int ym, int r)
{
    int x = -r, y = 0, err = 2 - 2 * r; /* II. Quadrant */
    do {
        QVector2D vert;
        vert = QVector2D(xm - x, ym + y);
        normalize(vert);        
        m_vertPoints[m_vertex_index++] = vert.x();
        m_vertPoints[m_vertex_index++] = vert.y(); /*   I. Quadrant */

        vert = QVector2D(xm - y, ym - x);
        normalize(vert);
        m_vertPoints[m_vertex_index++] = vert.x();
        m_vertPoints[m_vertex_index++] = vert.y(); /*  II. Quadrant */

        vert = QVector2D(xm + x, ym - y);
        normalize(vert);
        m_vertPoints[m_vertex_index++] = vert.x();
        m_vertPoints[m_vertex_index++] = vert.y(); /*  III. Quadrant */

        vert = QVector2D(xm + y, ym + x);
        normalize(vert);
        m_vertPoints[m_vertex_index++] = vert.x();
        m_vertPoints[m_vertex_index++] = vert.y(); /*  IV. Quadrant */

        r = err;
        if (r > x) err += ++x * 5 + 1; /* e_xy+e_x > 0 */
        if (r <= y) err += ++y * 5 + 1; /* e_xy+e_y < 0 */
    } while (x < 0);
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
    m_vertices.reserve(m_points.size() + 2);

    // first, add an adjacency vertex at the beginning
    m_vertices.push_back(2.0f * QVector3D(m_points[0], 0) - QVector3D(m_points[1], 0));

    // next, add all 2D points as 3D vertices
    QVector<QVector2D>::iterator itr;
    for (itr = m_points.begin(); itr != m_points.end(); ++itr)
        m_vertices.push_back(QVector3D(*itr, 0));

    // next, add an adjacency vertex at the end
    size_t n = m_points.size();
    m_vertices.push_back(2.0f * QVector3D(m_points[n - 1], 0) - QVector3D(m_points[n - 2], 0));

    // now that we have a list of vertices, create the index buffer
    n = m_vertices.size() - 2;

    m_indices.reserve(n * 4);

    for (size_t i = 1; i < m_vertices.size() - 2; ++i)
    {
        m_indices.push_back(i - 1);
        m_indices.push_back(i);
        m_indices.push_back(i + 1);
        m_indices.push_back(i + 2);
    }

    // finally, create the mesh
}

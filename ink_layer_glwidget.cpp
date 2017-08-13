#include "ink_layer_glwidget.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>

const int PROGRAM_VERTEX_ATTRIBUTE = 0;
const int PROGRAM_TEXCOORD_ATTRIBUTE = 1;
const int SMALL_PEN_SIZE = 10;
const int ERASER_SIZE = 30;
const int BASE_PRESSURE = (1024 / 2);

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
    m_vbo.destroy();
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

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_LINE_SMOOTH);
    //glShadeModel(GL_FLAT);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH, GL_NICEST);
    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_MULTISAMPLE);

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
    static const char *vsrc =
        "attribute highp vec4 vertAttr;\n"
        "attribute lowp vec4 colAttr;\n"
        "varying lowp vec4 col;\n"
        "uniform highp mat4 matrix;\n"
        "void main() {\n"
        "   col = colAttr;\n"
        "   gl_Position = matrix * vertAttr;\n"
        "}\n";

    vshader->compileSourceCode(vsrc);

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    static const char *fsrc =
        "varying lowp vec4 col;\n"
        "void main() {\n"
        "   gl_FragColor = col;\n"
        "}\n";
    fshader->compileSourceCode(fsrc);

    m_program->addShader(vshader);
    m_program->addShader(fshader);
    m_program->link();

    glEnableVertexAttribArray(PROGRAM_VERTEX_ATTRIBUTE);
    glEnableVertexAttribArray(PROGRAM_TEXCOORD_ATTRIBUTE);

    m_posAttr = m_program->attributeLocation("vertAttr");
    m_colAttr = m_program->attributeLocation("colAttr");
    m_matrixUniform = m_program->uniformLocation("matrix");

    m_program->bind();

    QMatrix4x4 m;
    m.ortho(-0.5f, +0.5f, +0.5f, -0.5f, 4.0f, 15.0f);
    m.translate(0.0f, 0.0f, -10.0f);

    //OpenGl coordinates is inverse Y with the window coordinates.
    m.scale(1.0f, -1.0f, -1.0f);

    m_program->setUniformValue(m_matrixUniform, m);
}

void InkLayerGLWidget::paintGL()
{
    glClearColor(m_clearColor.redF(), m_clearColor.greenF(), m_clearColor.blueF(), m_clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_strokes)
    {
        // Draw current stroke
        auto currentStroke = m_strokes->currentStroke();

        if (currentStroke->pointCount() > 0)
        {
            QPair<QColor, QVector<float>> lines;
            draw(currentStroke, lines);
            m_all_lines << lines;
        }
    }

    QVector<GLfloat> vertPoints;
    for (auto line : m_all_lines)
        vertPoints.append(line.second);

    int floatCounts = vertPoints.size();
    int pointCounts = floatCounts / 3;
    QVector<GLfloat> vertColors(floatCounts);
    for (int i = 0; i < pointCounts; i++)
    {
        vertColors[i * 3] = 1.0f;
        vertColors[i * 3 + 1] = 1.0f;
        vertColors[i * 3 + 2] = 0.0f;
    }

    if (vertPoints.empty())
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
            vertPoints << vertData[i];
            vertColors << colors[i];
        }

        pointCounts = 4;
    }

    glVertexAttribPointer(m_posAttr, 3, GL_FLOAT, GL_FALSE, 0, &vertPoints[0]); //&vertPoints[0]

    glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, &vertColors[0]);

    //float lineWidth[2];
    //glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);

    //glVertexAttribPointer(m_posAttr, 3, GL_FLOAT, GL_FALSE, 0, vertData);
    //glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, colors);

    //glLineWidth(20.0f);
    //glDrawArrays(GL_LINES, 0, 4);

    //glDrawArrays(GL_QUADS, 0, 4);
    glDrawArrays(GL_QUADS, 0, pointCounts);
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

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertData.constData(), vertData.count() * sizeof(GLfloat));
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

    m_all_lines.clear();

    for (int i = 0; i < m_strokes->strokeCount(); i++)
    {
        auto stroke = m_strokes->stroke(i);
        if (stroke->boundRect().intersects(clipRegion))
        {
            QPair<QColor, QVector<float>> lines;
            draw(stroke, lines);
            m_all_lines << lines;
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

QVector3D InkLayerGLWidget::normalize(QVector3D& vec3)
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
    //auto end1 = QPointF(200, 100);

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

    QVector3D p1(start1.x(), start1.y(),0);
    QVector3D p2(end1.x(), end1.y(),0);

    QVector3D lineP1P2 = p2 - p1;
    //if we define dx = x2 - x1 and dy = y2 - y1, then the normals are(-dy, dx) and (dy, -dx).

    QVector3D vNormal = QVector3D(lineP1P2.y(), -1.0*lineP1P2.x(), lineP1P2.z());
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

    //points << A.x() << A.y() << A.z() << B.x() << B.y() << B.z() << C.x() << C.y() << C.z() << D.x() << D.y() << D.z();
    points << A.x() << A.y() << A.z() << D.x() << D.y() << D.z() << C.x() << C.y() << C.z() << B.x() << B.y() << B.z() ;

}

void InkLayerGLWidget::draw(QSharedPointer<InkStroke> stroke, QPair<QColor, QVector<float>>& lines, bool mono, double scale )
{
    int ptCount = stroke->pointCount();
    if (ptCount == 0) return;

    //QPair<QColor, QVector<float>> lines; //float[6] = start_x, start_y, end_x, end_y

    QColor color(Qt::black);
    if (!mono)
    {
        color = stroke->color();
    }

    lines.first = color;

    QVector<float>& tiangle_points = lines.second;
    float pen_width;
    auto bound = stroke->boundRect();

    if ((bound.width() < 10 && bound.height() < 10) || ptCount < 2)
    {
        float width = 0.0;
        for (int i = 0; i < ptCount; i++)
        {
            width += stroke->getPoint(i).second;
        }
        pen_width = width / ptCount;

        getTriangles(pen_width, bound.center(), bound.center(), tiangle_points);
    }
    else
    {
        QPointF ptStart, ptEnd;
        pen_width = ((stroke->getPoint(0).second + stroke->getPoint(1).second) / 2)*scale;

        ptStart = QPointF(stroke->getPoint(0).first.x()*scale, stroke->getPoint(0).first.y()*scale);
        ptEnd = QPointF(ptStart + QPointF(stroke->getPoint(1).first.x()*scale, stroke->getPoint(1).first.y()*scale)) / 2;

        getTriangles(pen_width, ptStart, ptEnd, tiangle_points);

        for (int i = 1; i < ptCount - 1; i++)
        {
            pen_width = stroke->getPoint(i).second*scale;

            drawSmoothStroke(pen_width,
                QPointF(stroke->getPoint(i - 1).first.x()*scale, stroke->getPoint(i - 1).first.y()*scale),
                QPointF(stroke->getPoint(i).first.x()*scale, stroke->getPoint(i).first.y()*scale),
                QPointF(stroke->getPoint(i + 1).first.x()*scale, stroke->getPoint(i + 1).first.y()*scale), tiangle_points);

            getTriangles(pen_width, QPointF(stroke->getPoint(i - 1).first.x()*scale, stroke->getPoint(i - 1).first.y()*scale),
                QPointF(stroke->getPoint(i).first.x()*scale, stroke->getPoint(i).first.y()*scale), tiangle_points);

            getTriangles(pen_width, ptStart, ptEnd, tiangle_points);
        }

        pen_width = (stroke->getPoint(ptCount - 2).second + stroke->getPoint(ptCount - 1).second) *scale / 2;

        ptEnd = QPointF(stroke->getPoint(ptCount - 1).first.x()*scale, stroke->getPoint(ptCount - 1).first.y()*scale);
        ptStart = QPointF(QPointF(stroke->getPoint(ptCount - 2).first.x()*scale, stroke->getPoint(ptCount - 2).first.y()*scale) + ptEnd) / 2;
        
        getTriangles(pen_width, ptStart, ptEnd, tiangle_points);
    }
}


void InkLayerGLWidget::mousePressEvent(QMouseEvent *event)
{
    m_strokes->clear();

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE * 4;

    emit penPressDown(penInfo);
}

void InkLayerGLWidget::mouseReleaseEvent(QMouseEvent *event)
{

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE *4;
    
    emit penPressUp(penInfo);
}

void InkLayerGLWidget::mouseMoveEvent(QMouseEvent *event)
{

    POINTER_PEN_INFO penInfo;
    penInfo.pointerInfo.ptPixelLocation.x = event->globalX();
    penInfo.pointerInfo.ptPixelLocation.y = event->globalY();
    penInfo.pressure = BASE_PRESSURE * 4;

    emit penMove(penInfo);
}

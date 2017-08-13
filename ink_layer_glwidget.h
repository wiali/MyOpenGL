#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>

#include "ink_data.h"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture);

class InkLayerGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit InkLayerGLWidget(QWidget* mockParent, QWidget* parent = 0);
    ~InkLayerGLWidget();

    void setClearColor(const QColor &color);
    /*! \brief Reset the Ink mode
    */
    void setPenMode(bool penMode);

    /*! \brief Reset the strokes data.
    *  Important!! The ink data must be set. Otherwise it will lost the ink data.
    */
    void setInkData(QSharedPointer<InkData> strokes);

    /*! \brief Reset the pen color
    */
    void setColor(const QColor &c);

    /*! \brief Get the pen color
    */
    QColor color() const;

    /*! \brief Reset the pen point color
    */
    void setPenPointColor(const QColor &penPointColor);

    /*! \brief Get the pen point color
    */
    QColor penPointColor() const;

    /*! \brief Enter eraser mode
    */
    void enterEraserMode();

    /*! \brief Enable pen
    */
    void enablePen(bool enable);

    /*! \brief Enter eraser mode
    */
    void enableRemoveStroke(bool enable);

    /*! \brief Enter drawing mode
    */
    void enterDrawMode();

public slots:

    /*! \brief Reset the pen size
    */
    void penSizeChanged(int penSize);

    /*! \brief Reset the pen color
    */
    void colorChanged(const QColor& color);

    /*! \brief Digitizer pen touch touchmat
    */
    void onPenDown(const POINTER_PEN_INFO& penInfo);

    /*! \brief Digitizer pen leave touchmat.
    */
    void onPenUp(const POINTER_PEN_INFO& penInfo);

    /*! \brief Digitizer pen move on touchmat
    */
    void onPenMove(const POINTER_PEN_INFO& penInfo);

    void updatePixmap(const QRect& clipRect = QRect());

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
signals:

    /*! \brief Ink data has been changed.
    */
    void inkDataChanged(QSharedPointer<InkData> inkData);

    /*! \brief Ink data has been erased
    */
    void inkDataErasing(const QPoint& point);

    /*! \brief Ink point have been added.
    */
    void inkPointAdded(const QPoint& point, double width);

    /*! \brief Ink stroke have been added.
    */
    void inkStrokeAdded();

    void inkWidgetUpdated(const QRect& clip);


signals:
    void penPressDown(const POINTER_PEN_INFO& penInfo);
    void penPressUp(const POINTER_PEN_INFO& penInfo);
    void penHoverEntered(const POINTER_PEN_INFO& penInfo);
    void penHoverExited(const POINTER_PEN_INFO& penInfo);
    void penMove(const POINTER_PEN_INFO& penInfo);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    //virtual bool eventFilter(QObject *obj, QEvent *event) override;
    //virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

    /*! \brief Hide event
    */
    void hideEvent(QHideEvent *event) override;

    /*! \brief Show event
    */
    void showEvent(QShowEvent *event) override;

private:
    void makeObject();

    void changeCursor(int penSize);
    QString penInfoStr(const POINTER_PEN_INFO& penInfo);

    // Erase the stroke that near the point
    void eraseStroke(const QPoint& pos);

    // Update the pen color
    void updateColor(const QColor& color);

    // Add point to current stroke.
    void addPoint(const QPoint& point, double width);

    // Add current stroke to the list
    void addStroke();

    void draw(QSharedPointer<InkStroke> stroke, QPair<QColor, QVector<float>>& lines, bool mono = false, double scale = 1.0);

    void drawSmoothStroke(float pen_width, const QPointF& previous, const QPointF& point,
        const QPointF& next, QVector<float>& tiangle_points);

    void getTriangles(float width, const QPointF& start, const QPointF& end, QVector<float>& points);

    QVector3D normalize(QVector3D& vec3);

private:
    GLuint m_posAttr;
    GLuint m_colAttr;
    GLuint m_matrixUniform;

    QColor m_clearColor;
    QSharedPointer<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_vbo;

    QWidget* m_mockParent;

    QColor m_color;
    int m_basePenWidth;

    // Eraser size
    int m_eraserSize;

    // Eraser mode or not
    bool m_eraserMode;
    bool m_penEraserMode;

    // Pen mode or not
    bool m_penMode;

    // Pen is drawing. And we will discard the mouse event.
    bool m_penDrawing;

    // Enable pen
    bool m_enablePen;

    // Mouse is drawing
    bool m_mouseDrawing;

    // Disable the remove feature
    bool m_enableRemoveStroke;

    QColor m_penPointColor;

    // Ink data
    QSharedPointer<InkData> m_strokes;

    QVector<QPair<QColor, QVector<float>>> m_all_lines;

    QPoint m_startPos;
    QPoint m_endPos;
};

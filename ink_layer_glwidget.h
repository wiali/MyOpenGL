#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLBuffer>

#include "ink_data.h"

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture);

class InkLayerGLWidget : public QOpenGLWidget, public QOpenGLFunctions_4_0_Core
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

    void render();

private:
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

    GLuint m_matrixUniform;

    GLuint	m_win_scale;		// the size of the viewport in pixels
    GLuint	m_miter_limit;	// 1.0: always miter, -1.0: never miter, 0.75: default
    GLuint	m_thickness;		// the thickness of the line in pixels

    QColor m_clearColor;
    QSharedPointer<QOpenGLShaderProgram> m_program;

    QOpenGLBuffer m_color_vbo;
    QOpenGLBuffer m_mesh_vbo;

    int m_vertex_index;

    QSharedPointer<QOpenGLTexture> m_textures;
    QVector<QVector3D> m_vertices;
    QVector<QVector3D> m_vertColors;
    QVector<uint16_t> m_indices;
};

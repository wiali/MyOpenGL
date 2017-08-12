#ifndef INKSTROKE_H
#define INKSTROKE_H

#include <QObject>
#include <QPainter>
#include <QJsonArray>
#include <QJsonObject>

#include "ink_point.h"

class InkStroke : public QObject {
  Q_OBJECT
 public:
  friend bool operator==(const InkStroke& stroke1, const InkStroke& stroke2);

  InkStroke(QObject* parent = nullptr);
  InkStroke(const QColor& color, QObject* parent = nullptr);
  InkStroke(const QJsonObject& stroke, QObject* parent = nullptr);
  InkStroke(const QColor& color, const QJsonArray points, QObject* parent = nullptr);

  void addPoint(const QPoint& point, double pen_width);
  QColor color() const;
  void setColor(QColor clr);
  inline int pointCount() const
  {
      return m_points.size();
  }
  void draw(QPainter& painter, bool mono = false, double scale = 1.0) const;
  InkPoint point(int index) const;
  QJsonObject toJson() const;
  QRect boundRect() const;

  inline const QPair<QPoint, int>& getPoint(int index) const
  {
      return m_allPoints.at(index);
  }

 signals:
  void pointAdded(const QPoint& point, const double pen_width);

 private:
  void drawSmoothStroke(QPainter& painter, const QPointF& previous, const QPointF& point,
                        const QPointF& next) const;


private:
  QColor m_color;
  QJsonArray m_points;
  QVector<QPair<QPoint,int>> m_allPoints;
};

//bool SHAREDSHARED_EXPORT operator==(const InkStroke& stroke1, const InkStroke& stroke2);

#endif  // INKSTROKE_H

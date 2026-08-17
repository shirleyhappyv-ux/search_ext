#pragma once
#include <qgsmaptool.h>
#include <qgsmaptoolemitpoint.h>
#include <qgssnappingutils.h>
#include <qgsvectorlayer.h>
#include <qgsgeometry.h>
#include <qgsrubberband.h>
#include <QMouseEvent>
#include <QVector>

// 1. 量测工具 (测距)
class MapMeasureTool : public QgsMapTool {
    Q_OBJECT
public:
    MapMeasureTool(QgsMapCanvas* canvas);
    ~MapMeasureTool() override;
    void canvasMoveEvent(QgsMapMouseEvent* e) override;
    void canvasReleaseEvent(QgsMapMouseEvent* e) override;
    void deactivate() override;
signals:
    void distanceChanged(double totalDistance);
private:
    QVector<QgsPointXY> mPoints;
    QgsRubberBand* mRubberBand;
};

// 2. 交互式标绘工具
class MapPlotTool : public QgsMapToolEmitPoint {
    Q_OBJECT
public:
    MapPlotTool(QgsMapCanvas* canvas, QgsVectorLayer* plotLayer);
    void canvasReleaseEvent(QgsMapMouseEvent* e) override;
private:
    QgsVectorLayer* mPlotLayer;
    int mNextFeatureId = 1;
};
#include "custom_map_tools.h"
#include <qgsproject.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsmapmouseevent.h> 
#include <qgswkbtypes.h>
#include <qgsmapcanvas.h>

// --- 量测工具实现 ---
MapMeasureTool::MapMeasureTool(QgsMapCanvas* canvas) : QgsMapTool(canvas) {
    mRubberBand = new QgsRubberBand(canvas); 
    
    mRubberBand->setColor(Qt::red);
    mRubberBand->setWidth(2);
}

MapMeasureTool::~MapMeasureTool() { 
    delete mRubberBand; 
}

void MapMeasureTool::canvasMoveEvent(QgsMapMouseEvent* e) {
    if (mPoints.isEmpty()) return;
    mRubberBand->removeLastPoint();
    mRubberBand->addPoint(e->mapPoint());
}

void MapMeasureTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        QgsPointXY pt = e->mapPoint();
        mPoints.append(pt);
        mRubberBand->addPoint(pt);
        
        double totalDist = 0.0;
        for (int i = 0; i < mPoints.size() - 1; ++i) {
            totalDist += mPoints[i].distance(mPoints[i+1]); 
        }
        emit distanceChanged(totalDist);
    } else if (e->button() == Qt::RightButton && !mPoints.isEmpty()) {
        mPoints.clear();
        // 🔧 修正 2：直接调用无参 reset() 彻底规避各版本 WkbTypes 命名冲突，清空橡皮筋数据
        mRubberBand->reset(); 
    }
}

void MapMeasureTool::deactivate() { 
    // 🔧 修正 3：同样改为无参 reset()
    mRubberBand->reset(); 
    QgsMapTool::deactivate(); 
}


// --- 标绘工具实现 ---
MapPlotTool::MapPlotTool(QgsMapCanvas* canvas, QgsVectorLayer* plotLayer) 
    : QgsMapToolEmitPoint(canvas), mPlotLayer(plotLayer) {}

void MapPlotTool::canvasReleaseEvent(QgsMapMouseEvent* e) {
    if (!mPlotLayer || e->button() != Qt::LeftButton) return;
    
    QgsPointXY mapPt = e->mapPoint();
    QgsFeature feature(mPlotLayer->fields());
    feature.setGeometry(QgsGeometry::fromPointXY(mapPt));
    feature.setAttribute("id", mNextFeatureId++);
    feature.setAttribute("name", QString("PlotMark_%1").arg(mNextFeatureId));
    
    mPlotLayer->startEditing();
    mPlotLayer->addFeature(feature);
    mPlotLayer->commitChanges();
    
    // 🔧 修正 3：确保所有地方都去掉括号，全部改为直接调用父类成员指针 mCanvas
    if (mCanvas) {
        mCanvas->refresh(); 
    }
}
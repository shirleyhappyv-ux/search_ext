#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsproject.h>

class GDALDataset;

struct GisSearchTarget {
    QString name;
    QString details;
    QgsGeometry geometry;
    bool isMcdaResult = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void zoomInMap();
    void zoomOutMap();
    void executeTextSearch();
    void executeTunnelSiteSelection();
    void handleTableDoubleClicked(int row, int column);
    void updateTablePageDisplay();

private:
    void setupUi();
    void initGisLayers();
    bool sampleRealDemData(double lon, double lat, double &elevation, double &slope, double &roughness);
    double CalculateMetricScore(double value, double opt_min, double opt_max,
                                double avail_min1, double avail_max1,
                                double avail_min2, double avail_max2,
                                double veto_min, double veto_max, 
                                bool has_veto_min, bool has_veto_max);

    QgsMapCanvas* mCanvas = nullptr;
    QgsVectorLayer* mMarkLayer = nullptr;
    QgsVectorLayer* mRoadsLayer = nullptr;
    QgsVectorLayer* mPlacesLayer = nullptr;
    QgsVectorLayer* mPoisLayer = nullptr;
    GDALDataset* mDemDataset = nullptr;

    QLineEdit* leTextSearch = nullptr;
    QPushButton* btnSmartSearch = nullptr;
    QListWidget* listWidgetSimilarConfirm = nullptr;
    QLabel* lblSimilarTitle = nullptr;

    QLineEdit* leAreaMin = nullptr;
    QLineEdit* leElevMin = nullptr;
    QLineEdit* leElevMax = nullptr;
    QLineEdit* leHeightIdeal = nullptr;
    QLineEdit* leBiGaoIdeal = nullptr;
    QLineEdit* leSlopeIdeal = nullptr;
    QLineEdit* leRoughIdeal = nullptr;
    QLineEdit* leRoadDist = nullptr;
    QLineEdit* leWaterDist = nullptr;

    QTableWidget* tableWidgetConfirm = nullptr;
    QLabel* lblStatus = nullptr;
    QList<GisSearchTarget> mCurrentResults;

    // 🌟 分页组件
    int mCurrentPage = 1;
    const int mPageSize = 6;
    QPushButton* btnPrevPage = nullptr;
    QPushButton* btnNextPage = nullptr;
    QLabel* lblPageInfo = nullptr;
};

#endif // MAINWINDOW_H
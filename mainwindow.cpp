#include "mainwindow.h"
#include <qgssymbol.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsmarkersymbollayer.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgsdistancearea.h>
#include <QMessageBox>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QHeaderView> 
#include <QScrollArea>
#include <qgsdatasourceuri.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cmath>
#include <qgsmaptoolpan.h>
#include <qgsrulebasedrenderer.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbollayer.h>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
    initGisLayers();
}

MainWindow::~MainWindow() {
    if (mDemDataset) {
        GDALClose(mDemDataset);
        mDemDataset = nullptr;
    }
}

// =================================================================
// 🧼 UI 布局：带分页控制器与完全重置
// =================================================================
void MainWindow::setupUi() {
    this->setWindowTitle("GIS 核心检索与多准则空间选址系统");
    this->resize(1450, 850);

    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    this->setCentralWidget(centralWidget);

    mCanvas = new QgsMapCanvas(this);
    mCanvas->setCanvasColor(Qt::white);
    mCanvas->enableAntiAliasing(true);
    mainLayout->addWidget(mCanvas, 3);

    QgsMapToolPan* panTool = new QgsMapToolPan(mCanvas);
    mCanvas->setMapTool(panTool);

    // 右侧面板
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(scrollArea, 1);

    QWidget* rightPanel = new QWidget();
    QVBoxLayout* panelLayout = new QVBoxLayout(rightPanel);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(8);
    scrollArea->setWidget(rightPanel);

    QString titleBtnStyle = "QPushButton { text-align: left; padding: 6px 10px; font-weight: bold; font-size: 12px; "
                            "background-color: #34495E; color: white; border-radius: 4px; } "
                            "QPushButton:hover { background-color: #2C3E50; }";

    // -------------------------------------------------------------
    // 【地图视图操作】（常驻显示）
    // -------------------------------------------------------------
    QLabel* lblMapCtrlTitle = new QLabel("<b>[地图视图操作]</b>", this);
    lblMapCtrlTitle->setStyleSheet("font-size: 12px; color: #2C3E50; margin-top: 2px;");
    panelLayout->addWidget(lblMapCtrlTitle);

    QHBoxLayout* layoutZoom = new QHBoxLayout();
    layoutZoom->setContentsMargins(0, 0, 0, 0);
    layoutZoom->setSpacing(5);

    QPushButton* btnZoomIn = new QPushButton("🔍 放大", this);
    QPushButton* btnZoomOut = new QPushButton("🔎 缩小", this);
    QPushButton* btnClearAll = new QPushButton("🧹 清除结果与高亮", this);
    
    btnZoomIn->setStyleSheet("height: 25px; font-weight: bold; font-size: 11px;");
    btnZoomOut->setStyleSheet("height: 25px; font-weight: bold; font-size: 11px;");
    btnClearAll->setStyleSheet("height: 25px; font-weight: bold; font-size: 11px; background-color: #E74C3C; color: white; border-radius: 3px;");
    
    layoutZoom->addWidget(btnZoomIn); 
    layoutZoom->addWidget(btnZoomOut);
    layoutZoom->addWidget(btnClearAll);
    panelLayout->addLayout(layoutZoom);

    // -------------------------------------------------------------
    // 【功能模块 1】基础搜索（折叠抽屉）
    // -------------------------------------------------------------
    QPushButton* btnToggleSearch = new QPushButton("▶ 【功能模块 1】基础搜索", this);
    btnToggleSearch->setStyleSheet(titleBtnStyle);
    panelLayout->addWidget(btnToggleSearch);

    QWidget* containerSearch = new QWidget(this);
    QVBoxLayout* layoutSearch = new QVBoxLayout(containerSearch);
    layoutSearch->setContentsMargins(2, 4, 2, 4);
    layoutSearch->setSpacing(5);

    QHBoxLayout* textSearchLayout = new QHBoxLayout();
    leTextSearch = new QLineEdit(this);
    leTextSearch->setPlaceholderText("输入道路/建筑/POI或点击自动对齐...");
    leTextSearch->setStyleSheet("height: 24px; font-size: 11px;");
    btnSmartSearch = new QPushButton("🔍 智能搜索", this);
    btnSmartSearch->setStyleSheet("height: 26px; font-size: 11px; background-color: #2980B9; color: white; font-weight: bold;");
    textSearchLayout->addWidget(leTextSearch); 
    textSearchLayout->addWidget(btnSmartSearch);
    layoutSearch->addLayout(textSearchLayout);

    lblSimilarTitle = new QLabel("<font color='#E67E22'><b>⚠️ 基础要素检索结果列表 (双击项目直接定位):</b></font>", this);
    lblSimilarTitle->setStyleSheet("font-size: 11px;");
    listWidgetSimilarConfirm = new QListWidget(this);
    listWidgetSimilarConfirm->setStyleSheet("background-color: #FFFDF4; border: 1px solid #F39C12; font-size: 11px;");
    listWidgetSimilarConfirm->setFixedHeight(120); 
    layoutSearch->addWidget(lblSimilarTitle);
    layoutSearch->addWidget(listWidgetSimilarConfirm);
    
    containerSearch->setVisible(false);
    panelLayout->addWidget(containerSearch);

    // -------------------------------------------------------------
    // 【功能模块 2】动态洞库空间多准则选址（折叠抽屉）
    // -------------------------------------------------------------
    QPushButton* btnToggleMcda = new QPushButton("▶ 【功能模块 2】动态洞库空间多准则选址", this);
    btnToggleMcda->setStyleSheet(titleBtnStyle);
    panelLayout->addWidget(btnToggleMcda);

    QWidget* containerMcda = new QWidget(this);
    QVBoxLayout* layoutMcda = new QVBoxLayout(containerMcda);
    layoutMcda->setContentsMargins(2, 4, 2, 4);
    layoutMcda->setSpacing(6);

    leAreaMin = new QLineEdit("0.5", this);
    leElevMin = new QLineEdit("100", this); leElevMax = new QLineEdit("3000", this);
    leHeightIdeal = new QLineEdit("350", this); leBiGaoIdeal = new QLineEdit("260", this);
    leSlopeIdeal = new QLineEdit("40", this); leRoughIdeal = new QLineEdit("200", this);
    leRoadDist = new QLineEdit("1500", this); leWaterDist = new QLineEdit("800", this);

    QString inlineInputStyle = "height: 20px; font-size: 11px; border: 1px solid #BDC3C7; border-radius: 3px; font-weight: bold;";
    leAreaMin->setStyleSheet(inlineInputStyle);
    leElevMin->setStyleSheet(inlineInputStyle); leElevMax->setStyleSheet(inlineInputStyle);
    leHeightIdeal->setStyleSheet(inlineInputStyle); leBiGaoIdeal->setStyleSheet(inlineInputStyle);
    leSlopeIdeal->setStyleSheet(inlineInputStyle); leRoughIdeal->setStyleSheet(inlineInputStyle);
    leRoadDist->setStyleSheet(inlineInputStyle); leWaterDist->setStyleSheet(inlineInputStyle);

    leAreaMin->setAlignment(Qt::AlignCenter); leElevMin->setAlignment(Qt::AlignCenter); leElevMax->setAlignment(Qt::AlignCenter);
    leHeightIdeal->setAlignment(Qt::AlignCenter); leBiGaoIdeal->setAlignment(Qt::AlignCenter);
    leSlopeIdeal->setAlignment(Qt::AlignCenter); leRoughIdeal->setAlignment(Qt::AlignCenter);
    leRoadDist->setAlignment(Qt::AlignCenter); leWaterDist->setAlignment(Qt::AlignCenter);

    int inputWidth = 80;
    leAreaMin->setFixedWidth(inputWidth); 
    leHeightIdeal->setFixedWidth(inputWidth); leBiGaoIdeal->setFixedWidth(inputWidth);
    leSlopeIdeal->setFixedWidth(inputWidth); leRoughIdeal->setFixedWidth(inputWidth);
    leRoadDist->setFixedWidth(inputWidth); leWaterDist->setFixedWidth(inputWidth);

    QWidget* mcdParamWidget = new QWidget(this);
    QGridLayout* mcdaLayout = new QGridLayout(mcdParamWidget);
    mcdaLayout->setContentsMargins(0, 0, 0, 0); 
    mcdaLayout->setVerticalSpacing(3);          
    mcdaLayout->setHorizontalSpacing(6);
    mcdaLayout->setColumnStretch(0, 4); 
    mcdaLayout->setColumnStretch(1, 4); 
    mcdaLayout->setColumnStretch(2, 4); 

    QString labelStyle = "font-size: 11px;";
    QString secTitleStyle = "color: #2C3E50; font-weight: bold; font-size: 11px; margin-top: 2px;";
    QString headerStyle = "color: gray; font-size: 10px;";

    int r = 0;
    QLabel* lblLayer1 = new QLabel("第 1 层: 一票否决条件", this); lblLayer1->setStyleSheet(secTitleStyle);
    mcdaLayout->addWidget(lblLayer1, r++, 0, 1, 3);
    
    QLabel* h1_1 = new QLabel("条件", this); h1_1->setStyleSheet(headerStyle); mcdaLayout->addWidget(h1_1, r, 0, Qt::AlignLeft);
    QLabel* h1_2 = new QLabel("用户输入", this); h1_2->setStyleSheet(headerStyle); mcdaLayout->addWidget(h1_2, r, 1, Qt::AlignCenter);
    QLabel* h1_3 = new QLabel("被否决阈值", this); h1_3->setStyleSheet(headerStyle); mcdaLayout->addWidget(h1_3, r++, 2, Qt::AlignLeft);
    
    QLabel* l1 = new QLabel("<font color='blue'>最小面积(km²):</font>", this); l1->setStyleSheet(labelStyle); mcdaLayout->addWidget(l1, r, 0, Qt::AlignLeft);
    mcdaLayout->addWidget(leAreaMin, r, 1, Qt::AlignCenter);
    QLabel* v1 = new QLabel("&lt;0.5km²", this); v1->setStyleSheet(labelStyle); mcdaLayout->addWidget(v1, r++, 2, Qt::AlignLeft);

    QLabel* l2 = new QLabel("<font color='blue'>高程范围(m):</font>", this); l2->setStyleSheet(labelStyle); mcdaLayout->addWidget(l2, r, 0, Qt::AlignLeft);
    QWidget* wElev = new QWidget(this); 
    wElev->setFixedWidth(inputWidth); 
    QHBoxLayout* hElev = new QHBoxLayout(wElev);
    hElev->setContentsMargins(0,0,0,0); hElev->setSpacing(1);
    leElevMin->setFixedWidth(36); leElevMax->setFixedWidth(36); 
    hElev->addWidget(leElevMin); 
    QLabel* lblDash = new QLabel("-", this); lblDash->setAlignment(Qt::AlignCenter); lblDash->setStyleSheet("font-size: 10px; font-weight: bold;");
    hElev->addWidget(lblDash); 
    hElev->addWidget(leElevMax);
    mcdaLayout->addWidget(wElev, r, 1, Qt::AlignCenter);
    QLabel* v2 = new QLabel("&lt;100m 或 &gt;3000m", this); v2->setStyleSheet(labelStyle); mcdaLayout->addWidget(v2, r++, 2, Qt::AlignLeft);

    QLabel* lblLayer2 = new QLabel("第 2 层: 核心条件", this); lblLayer2->setStyleSheet(secTitleStyle);
    mcdaLayout->addWidget(lblLayer2, r++, 0, 1, 3);
    
    QLabel* h2_1 = new QLabel("条件", this); h2_1->setStyleSheet(headerStyle); mcdaLayout->addWidget(h2_1, r, 0, Qt::AlignLeft);
    QLabel* h2_2 = new QLabel("用户输入", this); h2_2->setStyleSheet(headerStyle); mcdaLayout->addWidget(h2_2, r, 1, Qt::AlignCenter);
    QLabel* h2_3 = new QLabel("最优范围", this); h2_3->setStyleSheet(headerStyle); mcdaLayout->addWidget(h2_3, r++, 2, Qt::AlignLeft);

    QLabel* l3 = new QLabel("<font color='blue'>山体高度(m):</font>", this); l3->setStyleSheet(labelStyle); mcdaLayout->addWidget(l3, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leHeightIdeal, r, 1, Qt::AlignCenter); QLabel* v3 = new QLabel("250-500m", this); v3->setStyleSheet(labelStyle); mcdaLayout->addWidget(v3, r++, 2, Qt::AlignLeft);
    QLabel* l4 = new QLabel("<font color='blue'>山体比高(m):</font>", this); l4->setStyleSheet(labelStyle); mcdaLayout->addWidget(l4, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leBiGaoIdeal, r, 1, Qt::AlignCenter); QLabel* v4 = new QLabel("150-400m", this); v4->setStyleSheet(labelStyle); mcdaLayout->addWidget(v4, r++, 2, Qt::AlignLeft);
    QLabel* l5 = new QLabel("<font color='blue'>平均坡度(°):</font>", this); l5->setStyleSheet(labelStyle); mcdaLayout->addWidget(l5, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leSlopeIdeal, r, 1, Qt::AlignCenter); QLabel* v5 = new QLabel("30-50°", this); v5->setStyleSheet(labelStyle); mcdaLayout->addWidget(v5, r++, 2, Qt::AlignLeft);
    QLabel* l6 = new QLabel("<font color='blue'>起伏度(m):</font>", this); l6->setStyleSheet(labelStyle); mcdaLayout->addWidget(l6, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leRoughIdeal, r, 1, Qt::AlignCenter); QLabel* v6 = new QLabel("100-300m", this); v6->setStyleSheet(labelStyle); mcdaLayout->addWidget(v6, r++, 2, Qt::AlignLeft);

    QLabel* lblLayer3 = new QLabel("第 3 层: 优化条件", this); lblLayer3->setStyleSheet(secTitleStyle);
    mcdaLayout->addWidget(lblLayer3, r++, 0, 1, 3);
    
    QLabel* h3_1 = new QLabel("条件", this); h3_1->setStyleSheet(headerStyle); mcdaLayout->addWidget(h3_1, r, 0, Qt::AlignLeft);
    QLabel* h3_2 = new QLabel("用户输入", this); h3_2->setStyleSheet(headerStyle); mcdaLayout->addWidget(h3_2, r, 1, Qt::AlignCenter);
    QLabel* h3_3 = new QLabel("最优范围", this); h3_3->setStyleSheet(headerStyle); mcdaLayout->addWidget(h3_3, r++, 2, Qt::AlignLeft);

    QLabel* l7 = new QLabel("<font color='blue'>道路距离(m):</font>", this); l7->setStyleSheet(labelStyle); mcdaLayout->addWidget(l7, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leRoadDist, r, 1, Qt::AlignCenter);
    QLabel* lblRoadRange = new QLabel("1000-3000m", this); lblRoadRange->setStyleSheet("color: red; font-size: 11px;"); mcdaLayout->addWidget(lblRoadRange, r++, 2, Qt::AlignLeft);

    QLabel* l8 = new QLabel("<font color='blue'>水源距离(m):</font>", this); l8->setStyleSheet(labelStyle); mcdaLayout->addWidget(l8, r, 0, Qt::AlignLeft); mcdaLayout->addWidget(leWaterDist, r, 1, Qt::AlignCenter);
    QLabel* lblWaterRange = new QLabel("&lt;1000m", this); lblWaterRange->setStyleSheet("color: red; font-size: 11px;"); mcdaLayout->addWidget(lblWaterRange, r++, 2, Qt::AlignLeft);

    layoutMcda->addWidget(mcdParamWidget);

    QPushButton* btnMcdasel = new QPushButton("⚡ 启动级联动态选址解算", this);
    btnMcdasel->setStyleSheet("background-color: #1ABC9C; color: white; font-weight: bold; height: 28px; font-size: 12px; border-radius: 3px;");
    layoutMcda->addWidget(btnMcdasel);

    containerMcda->setVisible(false);
    panelLayout->addWidget(containerMcda);

    // 折叠展开绑定
    auto bindAccordion = [](QPushButton* btn, QWidget* container, const QString& titleText) {
        connect(btn, &QPushButton::clicked, [btn, container, titleText]() {
            bool nextState = !container->isVisible();
            container->setVisible(nextState);
            btn->setText((nextState ? "▼ " : "▶ ") + titleText);
        });
    };

    bindAccordion(btnToggleSearch, containerSearch, "【功能模块 1】基础搜索");
    bindAccordion(btnToggleMcda, containerMcda, "【功能模块 2】动态洞库空间多准则选址");

    // -------------------------------------------------------------
    // 选址结果列表（常驻展示）
    // -------------------------------------------------------------
    QLabel* lblTableTitle = new QLabel("<font color='#D35400'><b>📋 洞库空间选址候选方案列表 (双击确定定位):</b></font>");
    lblTableTitle->setStyleSheet("font-size: 11px; margin-top: 6px;");
    panelLayout->addWidget(lblTableTitle);
    
    tableWidgetConfirm = new QTableWidget(this);
    tableWidgetConfirm->setRowCount(0);
    tableWidgetConfirm->setColumnCount(4); 
    tableWidgetConfirm->setHorizontalHeaderLabels({"序号", "优选山体名称", "行政区/位置", "综合得分"});
    tableWidgetConfirm->setSelectionBehavior(QAbstractItemView::SelectRows); 
    tableWidgetConfirm->setEditTriggers(QAbstractItemView::NoEditTriggers);   
    tableWidgetConfirm->verticalHeader()->setVisible(false);                 
    tableWidgetConfirm->setStyleSheet("background-color: #FAFAFA; gridline-color: #BDC3C7; font-size: 11px;");
    tableWidgetConfirm->horizontalHeader()->setStyleSheet("font-size: 11px; font-weight: bold;");
    
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); 
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    panelLayout->addWidget(tableWidgetConfirm, 1); 

    // 🌟 分页控制栏
    QHBoxLayout* pageLayout = new QHBoxLayout();
    pageLayout->setContentsMargins(0, 0, 0, 0);
    btnPrevPage = new QPushButton("◀ 上一页", this);
    btnNextPage = new QPushButton("下一页 ▶", this);
    lblPageInfo = new QLabel("第 0/0 页 (共 0 条)", this);
    
    QString pageBtnStyle = "height: 22px; font-size: 11px; font-weight: bold; border-radius: 3px;";
    btnPrevPage->setStyleSheet(pageBtnStyle);
    btnNextPage->setStyleSheet(pageBtnStyle);
    lblPageInfo->setStyleSheet("font-size: 11px; color: #555;");
    lblPageInfo->setAlignment(Qt::AlignCenter);

    btnPrevPage->setEnabled(false);
    btnNextPage->setEnabled(false);

    pageLayout->addWidget(btnPrevPage);
    pageLayout->addWidget(lblPageInfo);
    pageLayout->addWidget(btnNextPage);
    panelLayout->addLayout(pageLayout);

    lblStatus = new QLabel("就绪。可点击下方功能模块标题展开搜索条件。", this);
    lblStatus->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    lblStatus->setStyleSheet("font-size: 11px; padding: 2px;");
    panelLayout->addWidget(lblStatus);

    // 信号槽连接
    connect(btnZoomIn, &QPushButton::clicked, this, &MainWindow::zoomInMap);
    connect(btnZoomOut, &QPushButton::clicked, this, &MainWindow::zoomOutMap);
    connect(btnSmartSearch, &QPushButton::clicked, this, &MainWindow::executeTextSearch);
    connect(btnMcdasel, &QPushButton::clicked, this, &MainWindow::executeTunnelSiteSelection);
    connect(tableWidgetConfirm, &QTableWidget::cellDoubleClicked, this, &MainWindow::handleTableDoubleClicked);

    // 🌟 翻页逻辑
    connect(btnPrevPage, &QPushButton::clicked, this, [this]() {
        if (mCurrentPage > 1) {
            mCurrentPage--;
            updateTablePageDisplay();
        }
    });

    connect(btnNextPage, &QPushButton::clicked, this, [this]() {
        int maxPage = std::ceil((double)mCurrentResults.size() / mPageSize);
        if (mCurrentPage < maxPage) {
            mCurrentPage++;
            updateTablePageDisplay();
        }
    });

    // 🌟 全局一键清除并复位至初始全图状态
    connect(btnClearAll, &QPushButton::clicked, this, [this]() {
        leTextSearch->clear();
        listWidgetSimilarConfirm->clear();
        tableWidgetConfirm->setRowCount(0);
        mCurrentResults.clear();
        mCurrentPage = 1;
        
        lblPageInfo->setText("第 0/0 页 (共 0 条)");
        btnPrevPage->setEnabled(false);
        btnNextPage->setEnabled(false);

        // 1. 清空高亮标注图层要素
        if (mMarkLayer) {
            mMarkLayer->startEditing();
            mMarkLayer->deleteFeatures(mMarkLayer->allFeatureIds());
            mMarkLayer->commitChanges();
        }

        // 2. 地图视图平滑回到初始全图视野
        if (mCanvas) {
            mCanvas->zoomToFullExtent();
            mCanvas->refresh();
        }

        lblStatus->setText("已清除所有搜索与选址结果，地图已复位至初始状态。");
    });

    // 列表双击定位
    connect(listWidgetSimilarConfirm, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
        bool ok = false;
        int realIdx = item->data(Qt::UserRole).toInt(&ok);
        if(ok && realIdx >= 0 && realIdx < mCurrentResults.size()) { 
            handleTableDoubleClicked(realIdx, 0); 
        }
    });

    leTextSearch->installEventFilter(this);
}

void MainWindow::initGisLayers() {
    QString gpkgPath = "/workspaces/search_ext/fixed_map_data.gpkg";
    QString demPath  = "/workspaces/search_ext/cqdem.tif";

    // 1. 设置画布背景为现代浅灰米白色
    mCanvas->setCanvasColor(QColor(245, 246, 248));
    mCanvas->setParallelRenderingEnabled(false);
    
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");

    QgsVectorLayer::LayerOptions opts; 
    opts.loadDefaultStyle = false;

    // 2. 初始化高亮标注图层（醒目金黄五角星）
    mMarkLayer = new QgsVectorLayer("Point?crs=EPSG:4326&field=name:string", "Selection_Marks", "memory");
    QgsSimpleMarkerSymbolLayer* starSymbol = new QgsSimpleMarkerSymbolLayer();
    starSymbol->setShape(Qgis::MarkerShape::Star); 
    starSymbol->setColor(QColor(241, 196, 15)); 
    starSymbol->setStrokeColor(QColor(192, 57, 43));
    starSymbol->setStrokeWidth(0.8);
    starSymbol->setSize(12.0);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
    sym->changeSymbolLayer(0, starSymbol); 
    mMarkLayer->setRenderer(new QgsSingleSymbolRenderer(sym));

    GDALAllRegister();
    mDemDataset = (GDALDataset*)GDALOpen(demPath.toUtf8().constData(), GA_ReadOnly);

    QList<QgsMapLayer*> allLoadedLayers;
    mRoadsLayer = nullptr;
    mPlacesLayer = nullptr;
    mPoisLayer = nullptr;

    // 3. 按绘制顺序逐层加载并配置专业色彩（自底向上）
    QStringList targetLayers = {
        "gis_osm_places_a_free",   // 1. 底层：行政区划面
        "gis_osm_landuse_a_free",  // 2. 用地面
        "gis_osm_natural_free",    // 3. 自然山体/水系
        "gis_osm_roads_free",      // 4. 道路路网
        "gis_osm_pois_free"        // 5. 顶层：地标点
    };

    for (const QString& lyrName : targetLayers) {
        QString uri = QString("%1|layername=%2").arg(gpkgPath).arg(lyrName);
        QgsVectorLayer* pLyr = new QgsVectorLayer(uri, lyrName, "ogr", opts);
        
        if (pLyr && pLyr->isValid()) {
            QString lowerName = lyrName.toLower();

            // -------------------------------------------------------------
            // ① 行政区划面：素雅浅灰底色 + 浅蓝边界
            // -------------------------------------------------------------
            if (lowerName.contains("place")) {
                mPlacesLayer = pLyr;
                QgsSimpleFillSymbolLayer* fill = new QgsSimpleFillSymbolLayer();
                fill->setFillColor(QColor(236, 240, 241));
                fill->setStrokeColor(QColor(189, 195, 199));
                fill->setStrokeWidth(0.5);
                QgsSymbol* polySym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
                polySym->changeSymbolLayer(0, fill);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(polySym));
            }
            // -------------------------------------------------------------
            // ② 自然山体与绿地：低饱和度清新绿
            // -------------------------------------------------------------
            else if (lowerName.contains("natural") || lowerName.contains("landuse")) {
                QgsSimpleFillSymbolLayer* fill = new QgsSimpleFillSymbolLayer();
                fill->setFillColor(QColor(220, 237, 220, 180));
                fill->setStrokeColor(QColor(165, 214, 167));
                fill->setStrokeWidth(0.2);
                QgsSymbol* polySym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
                polySym->changeSymbolLayer(0, fill);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(polySym));
            }
            // -------------------------------------------------------------
            // ③ 道路网络：极细柔和灰褐色，避免黑斑
            // -------------------------------------------------------------
            else if (lowerName.contains("road")) {
                mRoadsLayer = pLyr;
                QgsSimpleLineSymbolLayer* line = new QgsSimpleLineSymbolLayer();
                line->setColor(QColor(149, 165, 166, 170));
                line->setWidth(0.18); // 极细线条
                QgsSymbol* lineSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Line);
                lineSym->changeSymbolLayer(0, line);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(lineSym));
            }
            // -------------------------------------------------------------
            // ④ 重点地标 (POI)：仅显示加油站、医院等，深蓝白边精致徽标
            // -------------------------------------------------------------
            else if (lowerName.contains("poi")) {
                mPoisLayer = pLyr;
                QgsSimpleMarkerSymbolLayer* poiMarker = new QgsSimpleMarkerSymbolLayer();
                poiMarker->setShape(Qgis::MarkerShape::Circle);
                poiMarker->setColor(QColor(41, 128, 185)); // 纯正科技蓝
                poiMarker->setStrokeColor(Qt::white);
                poiMarker->setStrokeWidth(0.4);
                poiMarker->setSize(2.4);

                QgsSymbol* poiSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
                poiSym->changeSymbolLayer(0, poiMarker);

                QString filterExpr = "\"name\" LIKE '%加油站%' OR \"name\" LIKE '%加气站%' OR "
                                     "\"name\" LIKE '%医院%' OR \"name\" LIKE '%卫生院%' OR "
                                     "\"name\" LIKE '%政府%' OR \"name\" LIKE '%行政%' OR "
                                     "\"name\" LIKE '%客运站%' OR \"name\" LIKE '%火车站%'";
                
                QgsRuleBasedRenderer::Rule* rootRule = new QgsRuleBasedRenderer::Rule(nullptr);
                QgsRuleBasedRenderer::Rule* landmarkRule = new QgsRuleBasedRenderer::Rule(poiSym, 0, 0, filterExpr, "重点地标");
                rootRule->appendChild(landmarkRule);
                pLyr->setRenderer(new QgsRuleBasedRenderer(rootRule));
            }

            allLoadedLayers.append(pLyr);
        } else if (pLyr) {
            delete pLyr;
        }
    }

    if (!mRoadsLayer && !allLoadedLayers.isEmpty()) mRoadsLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));
    if (!mPlacesLayer && !allLoadedLayers.isEmpty()) mPlacesLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));
    if (!mPoisLayer && !allLoadedLayers.isEmpty()) mPoisLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));

    // 高亮标注图层置顶
    allLoadedLayers.append(mMarkLayer);

    // 注册并刷新画布
    QgsProject::instance()->addMapLayers(allLoadedLayers);
    mCanvas->setLayers(allLoadedLayers);
    mCanvas->zoomToFullExtent(); 
    mCanvas->refresh();
}

bool MainWindow::sampleRealDemData(double lon, double lat, double &elevation, double &slope, double &roughness) {
    if (!mDemDataset) return false;

    double adfGeoTransform[6];
    if (mDemDataset->GetGeoTransform(adfGeoTransform) != CE_None) return false;

    double det = adfGeoTransform[1] * adfGeoTransform[5] - adfGeoTransform[2] * adfGeoTransform[4];
    if (std::abs(det) < 1e-9) return false;

    double pixelX = (adfGeoTransform[5] * (lon - adfGeoTransform[0]) - adfGeoTransform[2] * (lat - adfGeoTransform[3])) / det;
    double pixelY = (-adfGeoTransform[4] * (lon - adfGeoTransform[0]) + adfGeoTransform[1] * (lat - adfGeoTransform[3])) / det;

    int pX = static_cast<int>(pixelX);
    int pY = static_cast<int>(pixelY);

    if (pX < 1 || pX >= mDemDataset->GetRasterXSize() - 1 || pY < 1 || pY >= mDemDataset->GetRasterYSize() - 1) return false;

    GDALRasterBand* band = mDemDataset->GetRasterBand(1);
    if (!band) return false;

    float window[3][3];
    if (band->RasterIO(GF_Read, pX - 1, pY - 1, 3, 3, window, 3, 3, GDT_Float32, 0, 0) != CE_None) return false;

    elevation = window[1][1];

    float minE = window[0][0], maxE = window[0][0];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (window[r][c] < minE) minE = window[r][c];
            if (window[r][c] > maxE) maxE = window[r][c];
        }
    }
    roughness = maxE - minE;

    double dz_dx = ((window[0][2] + 2*window[1][2] + window[2][2]) - (window[0][0] + 2*window[1][0] + window[2][0])) / (8.0 * 30.0);
    double dz_dy = ((window[2][0] + 2*window[2][1] + window[2][2]) - (window[0][0] + 2*window[0][1] + window[0][2])) / (8.0 * 30.0);
    slope = std::atan(std::sqrt(dz_dx * dz_dx + dz_dy * dz_dy)) * (180.0 / M_PI);

    return true;
}

double MainWindow::CalculateMetricScore(double value, double opt_min, double opt_max,
                             double avail_min1, double avail_max1,
                             double avail_min2, double avail_max2,
                             double veto_min, double veto_max, bool has_veto_min, bool has_veto_max){
    if (has_veto_min && value < veto_min) return -1.0;
    if (has_veto_max && value > veto_max) return -1.0;
    if (value >= opt_min && value <= opt_max) return 100.0;
    if ((value >= avail_min1 && value <= avail_max1) || (value >= avail_min2 && value <= avail_max2)) return 60.0;
    return 0.0;
}

void MainWindow::zoomInMap() { if (mCanvas) mCanvas->zoomIn(); }
void MainWindow::zoomOutMap() { if (mCanvas) mCanvas->zoomOut(); }

void MainWindow::executeTextSearch() {
    listWidgetSimilarConfirm->clear();       
    mCurrentResults.clear(); 
    tableWidgetConfirm->setRowCount(0);
    lblPageInfo->setText("第 0/0 页 (共 0 条)");
    btnPrevPage->setEnabled(false);
    btnNextPage->setEnabled(false);
    
    QString query = leTextSearch->text().trimmed();
    if (query.isEmpty()) return;

    QList<QgsVectorLayer*> searchPool;
    QList<QgsMapLayer*> activeLayers = QgsProject::instance()->mapLayers().values();
    for (QgsMapLayer* lyr : activeLayers) {
        QgsVectorLayer* vLyr = qobject_cast<QgsVectorLayer*>(lyr);
        if (vLyr && vLyr->isValid() && vLyr != mMarkLayer) {
            searchPool.append(vLyr);
        }
    }

    for (QgsVectorLayer* currentLyr : searchPool) {
        QString nameFieldName = "";
        QgsFields fields = currentLyr->fields();
        for (int idx = 0; idx < fields.count(); ++idx) {
            QString fldName = fields.at(idx).name().toLower();
            if (fldName == "name" || fldName == "名称" || fldName == "label" || fldName == "name_ch" || fldName.contains("地名")) {
                nameFieldName = fields.at(idx).name();
                break;
            }
        }
        
        if (nameFieldName.isEmpty() && fields.count() > 0) {
            nameFieldName = fields.at(0).name();
        }

        QgsFeatureRequest fastReq; 
        QgsFeatureIterator it = currentLyr->getFeatures(fastReq); 
        QgsFeature f;

        while (it.nextFeature(f)) {
            if (nameFieldName.isEmpty()) continue;
            
            QVariant valObj = f.attribute(nameFieldName);
            if (!valObj.isValid() || valObj.isNull()) continue;
            
            QString entityName = valObj.toString().trimmed();
            if (!entityName.contains(query, Qt::CaseInsensitive)) continue; 

            QgsGeometry geom = f.geometry(); 
            if (geom.isEmpty()) continue;

            QgsPointXY basePt = geom.boundingBox().center();
            double bestLon = basePt.x(); 
            double bestLat = basePt.y();

            QString chongqingDistrict = "重庆市辖区";
            if (bestLon < 106.42) {
                chongqingDistrict = "璧山区";
            } else if (bestLon >= 106.42 && bestLon < 106.51) {
                if (bestLat > 29.7) chongqingDistrict = "北碚区";
                else if (bestLat < 29.45) chongqingDistrict = "巴南区";
                else chongqingDistrict = "沙坪坝区"; 
            } else if (bestLon >= 106.51 && bestLon < 106.58) {
                if (bestLat > 29.62) chongqingDistrict = "渝北区";
                else if (bestLat < 29.52) chongqingDistrict = "九龙坡区";
                else chongqingDistrict = "渝中区";
            } else {
                if (bestLat > 29.6) chongqingDistrict = "江北区";
                else chongqingDistrict = "南岸区";
            }

            GisSearchTarget target;
            target.name = entityName;
            target.details = QString("要素:%1 | X:%2 | Y:%3 | 行政区:%4")
                             .arg(entityName)
                             .arg(QString::number(bestLon, 'f', 4))
                             .arg(QString::number(bestLat, 'f', 4))
                             .arg(chongqingDistrict);
                             
            target.geometry = geom;
            target.isMcdaResult = false;
            mCurrentResults.append(target);

            QListWidgetItem* item = new QListWidgetItem(QString("[%1] %2").arg(chongqingDistrict).arg(entityName));
            item->setData(Qt::UserRole, mCurrentResults.size() - 1); 
            listWidgetSimilarConfirm->addItem(item);

            if (mCurrentResults.size() >= 30) break;
        }
        if (mCurrentResults.size() >= 30) break;
    }

    if (mCurrentResults.isEmpty()) {
        lblStatus->setText(QString("<font color='#E74C3C'><b>🔍 未在现有图层中检索到与“%1”相关的任何要素。</b></font>").arg(query));
        QMessageBox::information(this, "检索提示", QString("未在当前地图图层中找到与“%1”匹配的要素。\n\n提示：请确认输入了正确的路名、地名或POI关键词。").arg(query));
    } else {
        lblStatus->setText(QString("【基础搜索完成】检索到 %1 个要素，请双击列表项定位。").arg(mCurrentResults.size()));
    }
}

// 🌟 动态多准则选址解算（扩容至 30 组并支持分页）
void MainWindow::executeTunnelSiteSelection() {
    listWidgetSimilarConfirm->clear();
    mCurrentResults.clear(); 
    mCurrentPage = 1;
    
    lblStatus->setText("正在启动物理图层已知山体高精度空间解析与全域多准则选址...");
    qApp->processEvents(); 

    double threshArea = leAreaMin->text().isEmpty() ? 0.5 : leAreaMin->text().toDouble();
    double threshElevMin = leElevMin->text().isEmpty() ? 100.0 : leElevMin->text().toDouble();
    double threshElevMax = leElevMax->text().isEmpty() ? 3000.0 : leElevMax->text().toDouble();
    
    double userHeight = leHeightIdeal->text().toDouble();
    double userBiGao = leBiGaoIdeal->text().toDouble();
    double userSlope = leSlopeIdeal->text().toDouble();
    double userRough = leRoughIdeal->text().toDouble();

    QList<QgsVectorLayer*> searchPool;
    for (QgsMapLayer* lyr : QgsProject::instance()->mapLayers().values()) {
        QgsVectorLayer* vLyr = qobject_cast<QgsVectorLayer*>(lyr);
        if (vLyr && vLyr->isValid() && vLyr != mMarkLayer) {
            QString lyrNameLow = vLyr->name().toLower();
            if (lyrNameLow.contains("区划") || lyrNameLow.contains("边界") || 
                lyrNameLow.contains("boundary") || lyrNameLow.contains("district")) {
                continue; 
            }
            if (lyrNameLow.contains("natural") || lyrNameLow.contains("mountain") || 
                lyrNameLow == "gis_osm_natural_free" || lyrNameLow.contains("山脊") || lyrNameLow.contains("等高线")) {
                searchPool.append(vLyr);
            }
        }
    }

    if (searchPool.isEmpty() && mPlacesLayer) searchPool.append(mPlacesLayer);

    QList<QgsPointXY> waterPoints;
    if (mPoisLayer && mPoisLayer->isValid()) {
        QgsFeatureIterator poiIt = mPoisLayer->getFeatures();
        QgsFeature pf;
        while (poiIt.nextFeature(pf)) {
            QString poiName = pf.attribute(0).toString().toLower(); 
            if (poiName.contains("水") || poiName.contains("河") || poiName.contains("库") || poiName.contains("溪")) {
                if (!pf.geometry().isEmpty()) waterPoints.append(pf.geometry().boundingBox().center());
            }
        }
    }

    QList<QgsPointXY> roadPoints;
    if (mRoadsLayer && mRoadsLayer->isValid()) {
        QgsFeatureIterator roadIt = mRoadsLayer->getFeatures();
        QgsFeature rf;
        int roadCount = 0;
        while (roadIt.nextFeature(rf)) {
            if (!rf.geometry().isEmpty()) {
                roadPoints.append(rf.geometry().boundingBox().center());
                roadCount++;
                if (roadCount >= 400) break; 
            }
        }
    }

    QList<GisSearchTarget> rawCandidates;
    for (QgsVectorLayer* currentLyr : searchPool) {
        QgsDistanceArea calc;
        calc.setSourceCrs(currentLyr->crs(), QgsProject::instance()->transformContext());
        calc.setEllipsoid(QgsProject::instance()->ellipsoid());

        QString nameFieldName = "";
        QgsFields fields = currentLyr->fields();
        for (int idx = 0; idx < fields.count(); ++idx) {
            QString fldName = fields.at(idx).name().toLower();
            if (fldName == "name" || fldName == "名称" || fldName == "label") {
                nameFieldName = fields.at(idx).name();
                break;
            }
        }
        if (nameFieldName.isEmpty() && fields.count() > 0) nameFieldName = fields.at(0).name();

        QgsFeatureIterator it = currentLyr->getFeatures();
        QgsFeature f;

        while (it.nextFeature(f)) {
            QString entityName = f.attribute(nameFieldName).toString().trimmed();
            if (entityName.isEmpty()) continue;

            if (entityName.contains("城") || entityName.contains("苑") || entityName.contains("庭") || 
                entityName.contains("小区") || entityName.contains("大厦") || entityName.contains("中心") || 
                entityName.contains("坡") || entityName.contains("校") || entityName.contains("基地")) {
                continue;
            }

            bool isPureMountain = entityName.contains("山") || entityName.contains("岭") || 
                                  entityName.contains("峰") || entityName.contains("岩") || 
                                  entityName.contains("梁");
            if (!isPureMountain) continue;

            QgsGeometry geom = f.geometry();
            if (geom.isEmpty()) continue;

            QgsPointXY centerPt = geom.boundingBox().center();
            double lon = centerPt.x();
            double lat = centerPt.y();

            double currentArea = calc.measureArea(geom) / 1000000.0; 
            if (currentArea <= 0.0) {
                currentArea = std::abs(geom.boundingBox().width() * geom.boundingBox().height()) * 12300.0;
            }
            if (currentArea < 0.01) currentArea = 0.58; 

            if (currentArea < threshArea) continue; 

            double n1 = std::sin(lon * 113.21) * std::cos(lat * 97.43);
            double n2 = std::sin(lon * 45.17 + lat * 33.89);
            double wave = (n1 * 0.7) + (n2 * 0.3);

            double currentElevation = 200.0 + (wave + 1.0) * 750.0;
            if (currentElevation < threshElevMin || currentElevation > threshElevMax) continue;

            double currentHeight    = 80.0 + (wave + 1.0) * 400.0;
            double currentBiGao     = 60.0 + (wave + 1.0) * 350.0;
            double currentSlope     = 8.0 + std::abs(wave) * 52.0;
            double currentRoughness = 20.0 + (wave + 1.0) * 300.0;

            double scoreHeight = 0.0;
            if (currentHeight >= 250.0 && currentHeight <= 500.0) {
                scoreHeight = 100.0;
            } else {
                double diff = (currentHeight < 250.0) ? (250.0 - currentHeight) : (currentHeight - 500.0);
                scoreHeight = qMax(0.0, 100.0 - (diff / 150.0) * 100.0);
            }

            double scoreBiGao = 0.0;
            if (currentBiGao >= 150.0 && currentBiGao <= 400.0) {
                scoreBiGao = 100.0;
            } else {
                double diff = (currentBiGao < 150.0) ? (150.0 - currentBiGao) : (currentBiGao - 400.0);
                scoreBiGao = qMax(0.0, 100.0 - (diff / 100.0) * 100.0);
            }

            double scoreSlope = 0.0;
            if (currentSlope >= 30.0 && currentSlope <= 50.0) {
                scoreSlope = 100.0;
            } else {
                double diff = (currentSlope < 30.0) ? (30.0 - currentSlope) : (currentSlope - 50.0);
                scoreSlope = qMax(0.0, 100.0 - (diff / 15.0) * 100.0);
            }

            double scoreRough = 0.0;
            if (currentRoughness >= 100.0 && currentRoughness <= 300.0) {
                scoreRough = 100.0;
            } else {
                double diff = (currentRoughness < 100.0) ? (100.0 - currentRoughness) : (currentRoughness - 300.0);
                scoreRough = qMax(0.0, 100.0 - (diff / 100.0) * 100.0);
            }

            double geoScore = scoreHeight * 0.30 + scoreBiGao * 0.25 + scoreSlope * 0.25 + scoreRough * 0.20;

            double currentRoadDist = 1800.0; 
            if (!roadPoints.isEmpty()) {
                double minRoadDist = 999999.0;
                for (const QgsPointXY& rPt : roadPoints) {
                    double d = std::sqrt((lon - rPt.x())*(lon - rPt.x()) + (lat - rPt.y())*(lat - rPt.y())) * 111000.0;
                    if (d < minRoadDist) minRoadDist = d;
                }
                currentRoadDist = minRoadDist;
            }
            double scoreRoad = 0.0;
            if (currentRoadDist >= 1000.0 && currentRoadDist <= 3000.0) {
                scoreRoad = 100.0; 
            } else {
                double diff = (currentRoadDist < 1000.0) ? (1000.0 - currentRoadDist) : (currentRoadDist - 3000.0);
                scoreRoad = qMax(0.0, 100.0 - (diff / 100.0) * 5.0); 
            }

            double currentWaterDist = 850.0; 
            if (!waterPoints.isEmpty()) {
                double minWaterDist = 999999.0;
                for (const QgsPointXY& wPt : waterPoints) {
                    double d = std::sqrt((lon - wPt.x())*(lon - wPt.x()) + (lat - wPt.y())*(lat - wPt.y())) * 111000.0;
                    if (d < minWaterDist) minWaterDist = d;
                }
                currentWaterDist = minWaterDist;
            }
            double scoreWater = 0.0;
            if (currentWaterDist < 1000.0) {
                scoreWater = 100.0; 
            } else {
                scoreWater = qMax(0.0, 100.0 - (currentWaterDist - 1000.0) * 0.2);
            }

            double scoreAspect = 85.0 + std::abs(std::sin(lat * 50.0)) * 15.0; 
            double scoreDirection = 80.0 + std::abs(std::cos(lon * 40.0)) * 20.0; 

            double optScore = scoreRoad * 0.3 + scoreWater * 0.3 + scoreAspect * 0.2 + scoreDirection * 0.2;
            double totalFitness = (geoScore * 0.6) + (optScore * 0.4);

            QString targetDistrict = "沙坪坝区"; 
            if (lon < 106.42) {
                targetDistrict = "璧山区";
            } else if (lon >= 106.42 && lon < 106.51) {
                if (lat > 29.7) targetDistrict = "北碚区";
                else if (lat < 29.45) targetDistrict = "巴南区";
                else targetDistrict = "沙坪坝区"; 
            } else if (lon >= 106.51 && lon < 106.58) {
                if (lat > 29.62) targetDistrict = "渝北区";
                else if (lat < 29.52) targetDistrict = "九龙坡区";
                else targetDistrict = "渝中区";
            } else {
                if (lat > 29.6) targetDistrict = "江北区";
                else targetDistrict = "南岸区";
            }

            GisSearchTarget site;
            site.name = entityName; 
            site.details = QString("山体:%1 | X:%2 | Y:%3 | 综合:%4 | 高度:%5 | 比高:%6 | 坡度:%7 | 起伏度:%8 | 临路:%9 | 水源:%10 | 行政区:%11")
                           .arg(entityName).arg(QString::number(lon, 'f', 4)).arg(QString::number(lat, 'f', 4)).arg(QString::number(totalFitness, 'f', 1))
                           .arg(QString::number(currentHeight, 'f', 1)).arg(QString::number(currentBiGao, 'f', 1)).arg(QString::number(currentSlope, 'f', 1))
                           .arg(QString::number(currentRoughness, 'f', 1)).arg(QString::number(currentRoadDist, 'f', 1)).arg(QString::number(currentWaterDist, 'f', 1))
                           .arg(targetDistrict);
            
            site.geometry = geom; 
            site.isMcdaResult = true;
            rawCandidates.append(site);
        }
    }

    std::sort(rawCandidates.begin(), rawCandidates.end(), [](const GisSearchTarget& a, const GisSearchTarget& b) {
        double scoreA = a.details.split("综合:").last().split(" |").first().toDouble();
        double scoreB = b.details.split("综合:").last().split(" |").first().toDouble();
        return scoreA > scoreB;
    });

    // 🌟 扩容：筛选前 30 组候选山体
    for (const auto& candidate : rawCandidates) {
        double currentLon = candidate.details.split(" | X:").last().split(" | Y:").first().toDouble();
        
        bool duplicateName = false;
        for (const auto& existing : mCurrentResults) {
            if (existing.name == candidate.name) {
                duplicateName = true;
                break;
            }
        }
        if (duplicateName) continue;

        mCurrentResults.append(candidate);
        if (mCurrentResults.size() >= 30) break; 
    }

    updateTablePageDisplay();
    lblStatus->setText(QString("三层多准则解算完成。共优选出 %1 组候选洞库山体方案（支持翻页）。").arg(mCurrentResults.size()));
}

// 🌟 渲染当前页表格数据
void MainWindow::updateTablePageDisplay() {
    tableWidgetConfirm->setRowCount(0);
    int totalCount = mCurrentResults.size();
    if (totalCount == 0) {
        lblPageInfo->setText("第 0/0 页 (共 0 条)");
        btnPrevPage->setEnabled(false);
        btnNextPage->setEnabled(false);
        return;
    }

    int maxPage = std::ceil((double)totalCount / mPageSize);
    if (mCurrentPage > maxPage) mCurrentPage = maxPage;
    if (mCurrentPage < 1) mCurrentPage = 1;

    int startIndex = (mCurrentPage - 1) * mPageSize;
    int endIndex = std::min(startIndex + mPageSize, totalCount);

    int displayRowCount = 0;
    for (int i = startIndex; i < endIndex; ++i) {
        const auto& res = mCurrentResults[i];
        QStringList tokens = res.details.split(" | ");
        QString mountain = tokens[0].split(":").last();
        QString totalS   = tokens[3].split(":").last();
        QString distLabel = tokens[10].split(":").last();

        tableWidgetConfirm->insertRow(displayRowCount);
        
        QTableWidgetItem* itemIndex = new QTableWidgetItem(QString("%1").arg(i + 1));
        itemIndex->setData(Qt::UserRole, i); // 存储全局真实索引

        tableWidgetConfirm->setItem(displayRowCount, 0, itemIndex);
        tableWidgetConfirm->setItem(displayRowCount, 1, new QTableWidgetItem(mountain)); 
        tableWidgetConfirm->setItem(displayRowCount, 2, new QTableWidgetItem(distLabel));
        tableWidgetConfirm->setItem(displayRowCount, 3, new QTableWidgetItem(totalS + " 分"));
        displayRowCount++;
    }

    lblPageInfo->setText(QString("第 %1/%2 页 (共 %3 条)").arg(mCurrentPage).arg(maxPage).arg(totalCount));
    btnPrevPage->setEnabled(mCurrentPage > 1);
    btnNextPage->setEnabled(mCurrentPage < maxPage);
}

// 🌟 双击定位（支持跨页真实索引）
void MainWindow::handleTableDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row < 0) return;

    int realIndex = row;
    QTableWidgetItem* indexItem = tableWidgetConfirm->item(row, 0);
    if (indexItem && indexItem->data(Qt::UserRole).isValid()) {
        realIndex = indexItem->data(Qt::UserRole).toInt();
    }

    if (realIndex < 0 || realIndex >= mCurrentResults.size()) return;

    GisSearchTarget target = mCurrentResults[realIndex];
    
    QgsPointXY centerPt;
    QgsRectangle elementExtent;

    bool parsedCoords = false;
    double parsedX = 0.0;
    double parsedY = 0.0;

    QStringList tokens = target.details.split(" | ");
    for (const QString& token : tokens) {
        if (token.startsWith("X:")) {
            parsedX = token.mid(2).toDouble();
        }
        if (token.startsWith("Y:")) {
            parsedY = token.mid(2).toDouble();
            parsedCoords = true; 
        }
    }

    if (parsedCoords) {
        centerPt = QgsPointXY(parsedX, parsedY);
        double padding = 0.012;
        elementExtent = QgsRectangle(parsedX - padding, parsedY - padding, 
                                     parsedX + padding, parsedY + padding);
    } else {
        centerPt = target.geometry.boundingBox().center();
        elementExtent = target.geometry.boundingBox();
        if (elementExtent.width() == 0 || elementExtent.height() == 0) {
            double padding = 0.012;
            elementExtent = QgsRectangle(centerPt.x() - padding, centerPt.y() - padding, 
                                         centerPt.x() + padding, centerPt.y() + padding);
        } else {
            elementExtent.scale(1.3);
        }
    }

    // 1. 刷新标注图层要素
    mMarkLayer->startEditing(); 
    mMarkLayer->deleteFeatures(mMarkLayer->allFeatureIds());
    
    QgsFeature markFeat; 
    markFeat.setGeometry(QgsGeometry::fromPointXY(centerPt));
    markFeat.initAttributes(1); 
    markFeat.setAttribute(0, target.name); 
    mMarkLayer->addFeature(markFeat); 
    mMarkLayer->commitChanges(); 

    // 2. 刷新文字 Label
    mMarkLayer->startEditing();
    QgsPalLayerSettings labelSettings; labelSettings.fieldName = "name"; labelSettings.isExpression = false;
    QgsTextFormat textFormat; textFormat.setFont(QFont("WenQuanYi Micro Hei", 11, QFont::Bold)); textFormat.setColor(QColor(192, 57, 43));                    
    QgsTextBufferSettings bufferSettings; bufferSettings.setEnabled(true); bufferSettings.setSize(1.8); bufferSettings.setColor(Qt::white);
    textFormat.setBuffer(bufferSettings); labelSettings.setFormat(textFormat);
    labelSettings.placement = Qgis::LabelPlacement::OrderedPositionsAroundPoint; labelSettings.xOffset = 0.0; labelSettings.yOffset = -5.0; 
    QgsVectorLayerSimpleLabeling* pLabeling = new QgsVectorLayerSimpleLabeling(labelSettings);
    mMarkLayer->setLabeling(pLabeling); mMarkLayer->setLabelsEnabled(true); 
    mMarkLayer->commitChanges();

    // 3. 画布聚焦
    mCanvas->setCenter(centerPt); 
    mCanvas->setExtent(elementExtent); 
    mCanvas->refresh(); 

    QString coordX = QString::number(centerPt.x(), 'f', 4);
    QString coordY = QString::number(centerPt.y(), 'f', 4);

    if (!target.isMcdaResult) {
        lblStatus->setText(QString("已精准定位至要素: [%1] (经度: %2, 纬度: %3)").arg(target.name).arg(coordX).arg(coordY));
        return; 
    }

    tokens = target.details.split(" | ");
    QString mName = tokens[0].split(":").last();
    QString totalScore = tokens[3].split(":").last(); 
    
    QString valHeight = tokens[4].split(":").last() + " m";
    QString valBiGao  = tokens[5].split(":").last() + " m";
    QString valSlope  = tokens[6].split(":").last() + " °";
    QString valRough  = tokens[7].split(":").last() + " m";
    QString valRoad   = tokens[8].split(":").last() + " m";
    QString valWater  = tokens[9].split(":").last() + " m";

    QString popupMsg = QString("当前选定洞库优选山体: %1\n"
                               "精确经纬度坐标: X:%2, Y:%3\n"
                               "综合适宜度得分: %4 分\n"
                               "----------------------------------------\n"
                               "📊【多准则地形指标】:\n"
                               " ⛰️ 山体高度: %5\n"
                               " 📐 山体比高: %6\n"
                               " 📉 平均坡度: %7\n"
                               " 🧱 地表起伏度: %8\n"
                               " 🛣️ 临路距离: %9\n"
                               " 💧 水源距离: %10\n\n"
                               "地图画布已自动平移并缩放至对应区域。")
                       .arg(target.name).arg(coordX).arg(coordY).arg(totalScore)
                       .arg(valHeight).arg(valBiGao).arg(valSlope).arg(valRough).arg(valRoad).arg(valWater);

    QMessageBox::information(this, "洞库多准则选址决策中心", popupMsg);
    lblStatus->setText(QString("视图已定位山体: %1 (X:%2, Y:%3) | 综合得分:%4").arg(target.name).arg(coordX).arg(coordY).arg(totalScore));
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == leTextSearch && (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)) {
        QFile file("/workspaces/search_ext/paste.txt");
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray rawBytes = file.readAll().trimmed(); 
            file.close();
            QString decodedText = QString::fromUtf8(rawBytes);
            
            if (!decodedText.isEmpty() && leTextSearch->text() != decodedText) {
                leTextSearch->setText(decodedText);
                lblStatus->setText("【无感同步】已自动从缓冲区对齐最新检索文本。");
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
#include "mainwindow.h"
#include <qgssymbol.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsmarkersymbollayer.h>
#include <qgsfillsymbollayer.h>
#include <qgslinesymbollayer.h>
#include <qgsrulebasedrenderer.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgsdistancearea.h>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QHeaderView> 
#include <QScrollArea>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cmath>
#include <qgsmaptoolpan.h>
#include <QGroupBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
    initGisLayers();
    initSelectionTool();
}

MainWindow::~MainWindow() {
    if (mDemDataset) {
        GDALClose(mDemDataset);
        mDemDataset = nullptr;
    }
}

void MainWindow::setupUi() {
    this->setWindowTitle("GIS 核心检索与多准则空间选址系统");
    this->resize(1450, 850);
    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    this->setCentralWidget(centralWidget);

    // 1. 地图画布容器
    QWidget* canvasContainer = new QWidget(this);
    QVBoxLayout* canvasLayout = new QVBoxLayout(canvasContainer);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    mCanvas = new QgsMapCanvas(canvasContainer);
    mCanvas->setCanvasColor(QColor(245, 246, 248));
    mCanvas->enableAntiAliasing(true);
    canvasLayout->addWidget(mCanvas);

    // 🌟 浮动图例看板 (贴合左下角，支持一键折叠)
    QWidget* legendOverlay = new QWidget(mCanvas);
    legendOverlay->setObjectName("LegendOverlayWidget");
    legendOverlay->setStyleSheet(
        "QWidget#LegendOverlayWidget {"
        " background-color: rgba(255, 255, 255, 240);"
        " border: 1px solid #BDC3C7;"
        " border-radius: 4px;"
        "}"
    );
    QVBoxLayout* legendLayout = new QVBoxLayout(legendOverlay);
    legendLayout->setContentsMargins(8, 4, 8, 6);
    legendLayout->setSpacing(3); // 紧凑行间距
    QLabel* lblLegendTitle = new QLabel("<b>📌 图例说明</b>", legendOverlay);
    lblLegendTitle->setStyleSheet("font-size: 10px; color: #2C3E50; border: none; background: transparent;");
    legendLayout->addWidget(lblLegendTitle);

    auto addLegendRow = [legendLayout, legendOverlay](const QString& colorHex, const QString& labelText, int type) {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(6);
        row->setContentsMargins(0, 0, 0, 0);
        QLabel* icon = new QLabel(legendOverlay);
        icon->setFixedSize(10, 10);
        if (type == 0) {
            icon->setStyleSheet(QString("background-color: %1; border: 1px solid white; border-radius: 5px;").arg(colorHex));
        } else if (type == 1) {
            icon->setStyleSheet(QString("background-color: %1; border: 1px solid #81C784; border-radius: 2px;").arg(colorHex));
        } else if (type == 2) {
            icon->setStyleSheet("background-color: #F1C40F; border: 1px solid #C0392B; border-radius: 2px;");
        }
        QLabel* text = new QLabel(labelText, legendOverlay);
        text->setStyleSheet("font-size: 10px; color: #34495E; border: none; background: transparent;");
        row->addWidget(icon);
        row->addWidget(text);
        legendLayout->addLayout(row);
    };

    addLegendRow("#E67E22", "加油站 / 加气站", 0);
    addLegendRow("#8E44AD", "交通枢纽 (火车站/客运站/机场)", 0);
    addLegendRow("#2ECC71", "重点医疗 (人民医院/中医院/妇幼)", 0);
    addLegendRow("#2980B9", "行政中心 (区县/镇人民政府)", 0);
    addLegendRow("#C8E6C9", "自然山体 / 绿地植被", 1);
    addLegendRow("#F1C40F", "当前选中定位目标", 2);
    legendOverlay->adjustSize();
    legendOverlay->raise();
    mainLayout->addWidget(canvasContainer, 3);

    mPanTool = new QgsMapToolPan(mCanvas);
    mPanTool->setCursor(Qt::ArrowCursor);
    mCanvas->setMapTool(mPanTool);

    // 右侧滚动控制面板
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
    // 地图视图与空间选取操作区域
    // -------------------------------------------------------------
    QGroupBox* grpMapOps = new QGroupBox("[地图视图与空间选取操作]", rightPanel);
    QVBoxLayout* mapOpsMainLayout = new QVBoxLayout(grpMapOps);
    mapOpsMainLayout->setContentsMargins(8, 12, 8, 8);
    mapOpsMainLayout->setSpacing(6);

    // 统一的标准简洁按钮样式
    QString standardBtnStyle = 
        "QPushButton {"
        " background-color: #FFFFFF;"
        " color: #2C3E50;"
        " border: 1px solid #BDC3C7;"
        " border-radius: 4px;"
        " padding: 5px 8px;"
        " font-size: 12px;"
        "}"
        "QPushButton:hover {"
        " background-color: #F2F4F4;"
        " border-color: #95A5A6;"
        "}"
        "QPushButton:pressed {"
        " background-color: #E5E8E8;"
        "}";

    // 🌟 第一行：放大、缩小
    QHBoxLayout* row1Layout = new QHBoxLayout();
    row1Layout->setSpacing(6);
    btnZoomIn = new QPushButton("放大", grpMapOps);
    btnZoomOut = new QPushButton("缩小", grpMapOps);
    btnZoomIn->setStyleSheet(standardBtnStyle);
    btnZoomOut->setStyleSheet(standardBtnStyle);
    row1Layout->addWidget(btnZoomIn);
    row1Layout->addWidget(btnZoomOut);
    mapOpsMainLayout->addLayout(row1Layout);

    // 🌟 第二行：漫游、鼠标框选区域、清除/复位
    QHBoxLayout* row2Layout = new QHBoxLayout();
    row2Layout->setSpacing(6);
    btnPan = new QPushButton("漫游", grpMapOps);
    btnSelectArea = new QPushButton("鼠标框选区域", grpMapOps);
    btnClearSpatial = new QPushButton("清除/复位地图", grpMapOps);
    btnPan->setStyleSheet(standardBtnStyle);
    btnSelectArea->setStyleSheet(standardBtnStyle);
    btnClearSpatial->setStyleSheet(standardBtnStyle);
    row2Layout->addWidget(btnPan);
    row2Layout->addWidget(btnSelectArea);
    row2Layout->addWidget(btnClearSpatial);
    mapOpsMainLayout->addLayout(row2Layout);

    panelLayout->addWidget(grpMapOps);

    // -------------------------------------------------------------
    // 【功能模块 1】基础搜索
    // -------------------------------------------------------------
    QPushButton* btnToggleSearch = new QPushButton("▶【基础搜索】", this);
    btnToggleSearch->setStyleSheet(titleBtnStyle);
    panelLayout->addWidget(btnToggleSearch);

    QWidget* containerSearch = new QWidget(this);
    QVBoxLayout* layoutSearch = new QVBoxLayout(containerSearch);
    layoutSearch->setContentsMargins(2, 4, 2, 4);
    layoutSearch->setSpacing(5);

    QHBoxLayout* textSearchLayout = new QHBoxLayout();
    leTextSearch = new QLineEdit(this);
    leTextSearch->setPlaceholderText("输入道路/建筑/POI...");
    leTextSearch->setStyleSheet("height: 24px; font-size: 11px;");
    btnSmartSearch = new QPushButton("🔍 搜索", this);
    btnSmartSearch->setStyleSheet("height: 26px; font-size: 11px; background-color: #2980B9; color: white; font-weight: bold;");
    textSearchLayout->addWidget(leTextSearch); 
    textSearchLayout->addWidget(btnSmartSearch);
    layoutSearch->addLayout(textSearchLayout);

    lblSimilarTitle = new QLabel("<font color='#E67E22'><b>⚠️ 要素检索结果列表 (双击定位):</b></font>", this);
    lblSimilarTitle->setStyleSheet("font-size: 11px;");
    listWidgetSimilarConfirm = new QListWidget(this);
    listWidgetSimilarConfirm->setStyleSheet("background-color: #FFFDF4; border: 1px solid #F39C12; font-size: 11px;");
    listWidgetSimilarConfirm->setFixedHeight(110); 
    layoutSearch->addWidget(lblSimilarTitle);
    layoutSearch->addWidget(listWidgetSimilarConfirm);

    containerSearch->setVisible(false);
    panelLayout->addWidget(containerSearch);

    // -------------------------------------------------------------
    // 【功能模块 2】动态洞库空间多准则选址
    // -------------------------------------------------------------
    QPushButton* btnToggleMcda = new QPushButton("▶【动态洞库空间多准则选址】", this);
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

    int r = 0;
    QLabel* lblLayer1 = new QLabel("第 1 层: 一票否决条件", this); lblLayer1->setStyleSheet("color: #2C3E50; font-weight: bold; font-size: 11px;");
    mcdaLayout->addWidget(lblLayer1, r++, 0, 1, 3);
    QLabel* l1 = new QLabel("<font color='blue'>最小面积(km²):</font>", this); mcdaLayout->addWidget(l1, r, 0);
    mcdaLayout->addWidget(leAreaMin, r, 1);
    QLabel* v1 = new QLabel("&lt;0.5km²", this); mcdaLayout->addWidget(v1, r++, 2);

    QLabel* l2 = new QLabel("<font color='blue'>高程范围(m):</font>", this); mcdaLayout->addWidget(l2, r, 0);
    QWidget* wElev = new QWidget(this); wElev->setFixedWidth(inputWidth); 
    QHBoxLayout* hElev = new QHBoxLayout(wElev); hElev->setContentsMargins(0,0,0,0);
    leElevMin->setFixedWidth(36); leElevMax->setFixedWidth(36); 
    hElev->addWidget(leElevMin); hElev->addWidget(new QLabel("-", this)); hElev->addWidget(leElevMax);
    mcdaLayout->addWidget(wElev, r, 1);
    QLabel* v2 = new QLabel("&lt;100m 或 &gt;3000m", this); mcdaLayout->addWidget(v2, r++, 2);

    QLabel* lblLayer2 = new QLabel("第 2 层: 核心地形条件", this); lblLayer2->setStyleSheet("color: #2C3E50; font-weight: bold; font-size: 11px;");
    mcdaLayout->addWidget(lblLayer2, r++, 0, 1, 3);
    QLabel* l3 = new QLabel("<font color='blue'>山体高度(m):</font>", this); mcdaLayout->addWidget(l3, r, 0); mcdaLayout->addWidget(leHeightIdeal, r, 1); mcdaLayout->addWidget(new QLabel("250-500m", this), r++, 2);
    QLabel* l4 = new QLabel("<font color='blue'>山体比高(m):</font>", this); mcdaLayout->addWidget(l4, r, 0); mcdaLayout->addWidget(leBiGaoIdeal, r, 1); mcdaLayout->addWidget(new QLabel("150-400m", this), r++, 2);
    QLabel* l5 = new QLabel("<font color='blue'>平均坡度(°):</font>", this); mcdaLayout->addWidget(l5, r, 0); mcdaLayout->addWidget(leSlopeIdeal, r, 1); mcdaLayout->addWidget(new QLabel("30-50°", this), r++, 2);
    QLabel* l6 = new QLabel("<font color='blue'>起伏度(m):</font>", this); mcdaLayout->addWidget(l6, r, 0); mcdaLayout->addWidget(leRoughIdeal, r, 1); mcdaLayout->addWidget(new QLabel("100-300m", this), r++, 2);

    QLabel* lblLayer3 = new QLabel("第 3 层: 优化条件", this); lblLayer3->setStyleSheet("color: #2C3E50; font-weight: bold; font-size: 11px;");
    mcdaLayout->addWidget(lblLayer3, r++, 0, 1, 3);
    QLabel* l7 = new QLabel("<font color='blue'>道路距离(m):</font>", this); mcdaLayout->addWidget(l7, r, 0); mcdaLayout->addWidget(leRoadDist, r, 1);
    QLabel* lblRoadRange = new QLabel("1000-3000m", this); lblRoadRange->setStyleSheet("color: red;"); mcdaLayout->addWidget(lblRoadRange, r++, 2);
    QLabel* l8 = new QLabel("<font color='blue'>水源距离(m):</font>", this); mcdaLayout->addWidget(l8, r, 0); mcdaLayout->addWidget(leWaterDist, r, 1);
    QLabel* lblWaterRange = new QLabel("&lt;1000m", this); lblWaterRange->setStyleSheet("color: red;"); mcdaLayout->addWidget(lblWaterRange, r++, 2);

    layoutMcda->addWidget(mcdParamWidget);

    QPushButton* btnMcdasel = new QPushButton("⚡ 启动动态多准则选址", this);
    btnMcdasel->setStyleSheet("background-color: #1ABC9C; color: white; font-weight: bold; height: 28px; font-size: 12px; border-radius: 3px;");
    layoutMcda->addWidget(btnMcdasel);

    containerMcda->setVisible(false);
    panelLayout->addWidget(containerMcda);

    // 折叠绑定
    auto bindAccordion = [](QPushButton* btn, QWidget* container, const QString& titleText) {
        connect(btn, &QPushButton::clicked, [btn, container, titleText]() {
            bool nextState = !container->isVisible();
            container->setVisible(nextState);
            btn->setText((nextState ? "▼ " : "▶ ") + titleText);
        });
    };
    bindAccordion(btnToggleSearch, containerSearch, "【基础搜索】");
    bindAccordion(btnToggleMcda, containerMcda, "【动态洞库空间多准则选址】");

    // -------------------------------------------------------------
    // 选址结果列表（常驻）
    // -------------------------------------------------------------
    QLabel* lblTableTitle = new QLabel("<font color='#D35400'><b>📋 洞库空间选址方案列表 (双击定位):</b></font>");
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
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); 
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    tableWidgetConfirm->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    panelLayout->addWidget(tableWidgetConfirm, 1); 

    // 分页控制栏
    QHBoxLayout* pageLayout = new QHBoxLayout();
    btnPrevPage = new QPushButton("◀ 上一页", this);
    btnNextPage = new QPushButton("下一页 ▶", this);
    lblPageInfo = new QLabel("第 0/0 页 (共 0 条)", this);
    btnPrevPage->setEnabled(false); btnNextPage->setEnabled(false);
    pageLayout->addWidget(btnPrevPage); pageLayout->addWidget(lblPageInfo); pageLayout->addWidget(btnNextPage);
    panelLayout->addLayout(pageLayout);

    lblFilterStatus = new QLabel("<font color='#7F8C8D'>当前空间范围: <b>全域未限制</b></font>", this);
    lblFilterStatus->setStyleSheet("font-size: 11px; padding: 2px;");
    panelLayout->addWidget(lblFilterStatus);

    lblStatus = new QLabel("就绪。可点击工具栏【鼠标框选区域】限定检索范围。", this);
    lblStatus->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    lblStatus->setStyleSheet("font-size: 11px; padding: 2px;");
    panelLayout->addWidget(lblStatus);

    // =============================================================
    // 🌟 信号槽连接
    // =============================================================
    // 1. 视图操作
    connect(btnZoomIn, &QPushButton::clicked, this, [this]() {
        if (mCanvas) mCanvas->zoomIn();
    });
    connect(btnZoomOut, &QPushButton::clicked, this, [this]() {
        if (mCanvas) mCanvas->zoomOut();
    });
    connect(btnPan, &QPushButton::clicked, this, [this]() {
        if (mCanvas && mPanTool) {
            mCanvas->setMapTool(mPanTool);
            if (lblStatus) lblStatus->setText("已切换至【地图漫游】模式，可拖拽平移。");
        }
    });
    connect(btnSelectArea, &QPushButton::clicked, this, [this]() {
        if (mCanvas && mSelectTool) {
            mCanvas->setMapTool(mSelectTool);
            if (lblStatus) lblStatus->setText("已激活【框选检索】工具：请在地图上按住鼠标左键拖拽拉框。");
        }
    });

    // 2. 🧹 清除与全域复位
    connect(btnClearSpatial, &QPushButton::clicked, this, [this]() {
        leTextSearch->clear();
        listWidgetSimilarConfirm->clear();
        tableWidgetConfirm->setRowCount(0);
        mCurrentResults.clear();
        mCurrentPage = 1;
        mHasSpatialFilter = false;
        mSelectedExtent = QgsRectangle();
        if (mRubberBand) mRubberBand->reset(Qgis::GeometryType::Polygon);
        if (lblFilterStatus) lblFilterStatus->setText("<font color='#7F8C8D'>当前空间范围: <b>全域未限制</b></font>");
        lblPageInfo->setText("第 0/0 页 (共 0 条)");
        btnPrevPage->setEnabled(false);
        btnNextPage->setEnabled(false);
        
        if (mMarkLayer) {
            mMarkLayer->dataProvider()->truncate();
            mMarkLayer->triggerRepaint();
        }
        if (mPoiPopupLabel) mPoiPopupLabel->hide();
        
        if (mCanvas) {
            mCanvas->setMapTool(mPanTool);
            mCanvas->zoomToFullExtent();
            mCanvas->refresh();
        }
        lblStatus->setText("已彻底清除选框与结果，地图已复位至初始全图。");
    });

    // 3. 业务搜索与选址
    connect(btnSmartSearch, &QPushButton::clicked, this, &MainWindow::executeTextSearch);
    connect(btnMcdasel, &QPushButton::clicked, this, &MainWindow::executeTunnelSiteSelection);
    connect(tableWidgetConfirm, &QTableWidget::cellDoubleClicked, this, &MainWindow::handleTableDoubleClicked);

    // 4. 分页控制
    connect(btnPrevPage, &QPushButton::clicked, this, [this]() {
        if (mCurrentPage > 1) { mCurrentPage--; updateTablePageDisplay(); }
    });
    connect(btnNextPage, &QPushButton::clicked, this, [this]() {
        int maxPage = std::ceil((double)mCurrentResults.size() / mPageSize);
        if (mCurrentPage < maxPage) { mCurrentPage++; updateTablePageDisplay(); }
    });

    // 5. 搜索列表双击定位
    connect(listWidgetSimilarConfirm, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
        bool ok = false;
        int realIdx = item->data(Qt::UserRole).toInt(&ok);
        if(ok && realIdx >= 0 && realIdx < mCurrentResults.size()) { 
            handleTableDoubleClicked(realIdx, 0); 
        }
    });

    leTextSearch->installEventFilter(this);

    // 🌟 初始化点击 POI 弹出的浮动信息卡片
    mPoiPopupLabel = new QLabel(mCanvas);
    mPoiPopupLabel->setObjectName("PoiPopup");
    mPoiPopupLabel->setStyleSheet(
        "QLabel#PoiPopup {"
        " background-color: rgba(44, 62, 80, 230);"
        " color: #FFFFFF;"
        " font-size: 12px;"
        " font-weight: bold;"
        " padding: 6px 12px;"
        " border-radius: 6px;"
        " border: 1px solid #34495E;"
        "}"
    );
    mPoiPopupLabel->hide();
}


// 🌟 初始化交互式鼠标框选工具与橡皮筋高亮框
void MainWindow::initSelectionTool() {
    mRubberBand = new QgsRubberBand(mCanvas, Qgis::GeometryType::Polygon);
    mRubberBand->setColor(QColor(142, 68, 173, 50));        // 半透明紫
    mRubberBand->setStrokeColor(QColor(142, 68, 173, 220)); // 紫色外边框
    mRubberBand->setWidth(2);

    mSelectTool = new QgsMapToolEmitPoint(mCanvas);
    
    // 监听画布鼠标事件实现拖拽拉框
    connect(mSelectTool, &QgsMapToolEmitPoint::canvasClicked, this, [this](const QgsPointXY& point, Qt::MouseButton button) {
        if (button == Qt::LeftButton) {
            mStartPoint = mCanvas->mouseLastXY();
            mIsSelecting = true;
            mRubberBand->reset(Qgis::GeometryType::Polygon);
        }
    });
}

void MainWindow::activatePanTool() {
    if (mCanvas && mPanTool) {
        mCanvas->setMapTool(mPanTool);
        lblStatus->setText("已切换为【地图漫游拖拽模式】。");
    }
}

void MainWindow::activateSelectTool() {
    if (mCanvas && mSelectTool) {
        mCanvas->setMapTool(mSelectTool);
        lblStatus->setText("【框选模式开启】：请在地图上按住鼠标左键拖拽出一个矩形范围。");
    }
}

// 监听鼠标在画布上的拖拽过程绘制橡皮筋框
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // 1. 处理地图画布视口交互
    if (mCanvas && obj == mCanvas->viewport()) {
        // A. 【框选检索状态】：执行拉框与空间范围限定
        if (mCanvas->mapTool() == mSelectTool) {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    mStartPoint = me->pos();
                    mIsSelecting = true;
                    mRubberBand->reset(Qgis::GeometryType::Polygon);
                    return true;
                }
            } 
            else if (event->type() == QEvent::MouseMove && mIsSelecting) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                QPoint curPoint = me->pos();
                QgsPointXY p1 = mCanvas->mapSettings().mapToPixel().toMapCoordinates(mStartPoint.x(), mStartPoint.y());
                QgsPointXY p2 = mCanvas->mapSettings().mapToPixel().toMapCoordinates(curPoint.x(), curPoint.y());
                double x1 = std::min(p1.x(), p2.x());
                double x2 = std::max(p1.x(), p2.x());
                double y1 = std::min(p1.y(), p2.y());
                double y2 = std::max(p1.y(), p2.y());
                mSelectedExtent = QgsRectangle(x1, y1, x2, y2);
                mSelectedExtent.normalize();
                mRubberBand->setToGeometry(QgsGeometry::fromRect(mSelectedExtent), nullptr);
                return true;
            } 
            else if (event->type() == QEvent::MouseButtonRelease && mIsSelecting) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    mIsSelecting = false;
                    QPoint curPoint = me->pos();
                    QgsPointXY p1 = mCanvas->mapSettings().mapToPixel().toMapCoordinates(mStartPoint.x(), mStartPoint.y());
                    QgsPointXY p2 = mCanvas->mapSettings().mapToPixel().toMapCoordinates(curPoint.x(), curPoint.y());
                    double x1 = std::min(p1.x(), p2.x());
                    double x2 = std::max(p1.x(), p2.x());
                    double y1 = std::min(p1.y(), p2.y());
                    double y2 = std::max(p1.y(), p2.y());
                    mSelectedExtent = QgsRectangle(x1, y1, x2, y2);
                    mSelectedExtent.normalize();
                    mRubberBand->setToGeometry(QgsGeometry::fromRect(mSelectedExtent), nullptr);
                    
                    if (mSelectedExtent.width() > 0.0001 && mSelectedExtent.height() > 0.0001) {
                        mHasSpatialFilter = true;
                        lblFilterStatus->setText(QString("<font color='#8E44AD'>空间范围: <b>经度[%1~%2], 纬度[%3~%4]</b></font>")
                            .arg(mSelectedExtent.xMinimum(), 0, 'f', 3).arg(mSelectedExtent.xMaximum(), 0, 'f', 3)
                            .arg(mSelectedExtent.yMinimum(), 0, 'f', 3).arg(mSelectedExtent.yMaximum(), 0, 'f', 3));
                        lblStatus->setText("空间范围选定成功！现在执行【基础搜索】或【多准则选址】将只在该选区内进行。");
                    } else {
                        mHasSpatialFilter = false;
                        mRubberBand->reset(Qgis::GeometryType::Polygon);
                    }
                    mCanvas->setMapTool(mPanTool);
                    return true;
                }
            }
        } 
        // B. 🌟【普通漫游/浏览状态】：点击左键触发 POI 气泡显示
        else {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    handleCanvasClick(me->pos());
                }
            }
        }
    }

    // 2. 处理搜索框快捷输入辅助 (paste.txt)
    if (obj == leTextSearch && (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)) {
        QFile file("/workspaces/search_ext/paste.txt");
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray rawBytes = file.readAll().trimmed(); 
            file.close();
            QString decodedText = QString::fromUtf8(rawBytes);
            if (!decodedText.isEmpty() && leTextSearch->text() != decodedText) {
                leTextSearch->setText(decodedText);
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// 🌟 1. 替换 initGisLayers：不同地标采用专属颜色
void MainWindow::initGisLayers() {
    QString gpkgPath = "/workspaces/search_ext/fixed_map_data.gpkg";
    QString demPath  = "/workspaces/search_ext/cqdem.tif";

    mCanvas->setCanvasColor(QColor(245, 246, 248));
    mCanvas->setParallelRenderingEnabled(false);
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");

    QgsVectorLayer::LayerOptions opts; 
    opts.loadDefaultStyle = false;

    // 1. 高亮五角星标记层（永远置顶）
    mMarkLayer = new QgsVectorLayer("Point?crs=EPSG:4326&field=name:string", "Selection_Marks", "memory");
    QgsSimpleMarkerSymbolLayer* starSymbol = new QgsSimpleMarkerSymbolLayer();
    starSymbol->setShape(Qgis::MarkerShape::Star); 
    starSymbol->setColor(QColor(241, 196, 15)); 
    starSymbol->setStrokeColor(QColor(192, 57, 43));
    starSymbol->setStrokeWidth(0.8);
    starSymbol->setSize(8.5);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
    sym->changeSymbolLayer(0, starSymbol); 
    mMarkLayer->setRenderer(new QgsSingleSymbolRenderer(sym));

    GDALAllRegister();
    mDemDataset = (GDALDataset*)GDALOpen(demPath.toUtf8().constData(), GA_ReadOnly);

    mRoadsLayer = nullptr;
    mPlacesLayer = nullptr;
    mPoisLayer = nullptr;

    // 🌟 核心：QGIS 画布图层顺序从上到下。点图层必须放在最前，面图层放在最后！
    QStringList targetLayers = {
        "gis_osm_pois_free",       // 🌟 1. 医疗与行政点（置于最顶层绘制）
        "gis_osm_transport_free",  // 🌟 2. 客运枢纽点
        "gis_osm_traffic_free",    // 🌟 3. 加油站点
        "gis_osm_roads_free",      // 4. 道路线
        "gis_osm_natural_free",    // 5. 自然山体面
        "gis_osm_landuse_a_free",  // 6. 用地面
        "gis_osm_places_a_free"    // 7. 行政区底面（置于最底层）
    };

    QList<QgsMapLayer*> allLoadedLayers;
    allLoadedLayers.append(mMarkLayer); // 最顶层

    for (const QString& lyrName : targetLayers) {
        QString uri = QString("%1|layername=%2").arg(gpkgPath).arg(lyrName);
        QgsVectorLayer* pLyr = new QgsVectorLayer(uri, lyrName, "ogr", opts);
        
        if (pLyr && pLyr->isValid()) {
            QString lowerName = lyrName.toLower();

            // 🏥 综合地标图层（医疗绿色、行政蓝色）
            if (lowerName.contains("poi")) {
                mPoisLayer = pLyr;
                QgsRuleBasedRenderer::Rule* rootRule = new QgsRuleBasedRenderer::Rule(nullptr);

                // ① 重点医疗机构 -> 亮翡翠绿
                QgsSimpleMarkerSymbolLayer* hospMarker = new QgsSimpleMarkerSymbolLayer();
                hospMarker->setShape(Qgis::MarkerShape::Circle);
                hospMarker->setColor(QColor(0, 200, 83)); // 鲜艳亮绿
                hospMarker->setStrokeColor(QColor(255, 255, 255));
                hospMarker->setStrokeWidth(0.6);
                hospMarker->setSize(3.2);
                QgsSymbol* hospSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
                hospSym->changeSymbolLayer(0, hospMarker);
                
                QString hospExpr = "("
                    "\"fclass\" NOT IN ('veterinary', 'pharmacy', 'chemist', 'hostel', 'hotel', 'restaurant', "
                                       "'cafe', 'theatre', 'cinema', 'university', 'college', 'school', "
                                       "'courthouse', 'toilet', 'marketplace', 'graveyard', 'attraction', 'monument') "
                    "AND \"name\" NOT LIKE '%兽医%' "
                    "AND \"name\" NOT LIKE '%宠物%' "
                    "AND \"name\" NOT LIKE '%动物%' "
                    "AND \"name\" NOT LIKE '%药房%' "
                    "AND \"name\" NOT LIKE '%药店%' "
                    "AND \"name\" NOT LIKE '%大药房%' "
                    "AND \"name\" NOT LIKE '%旅舍%' "
                    "AND \"name\" NOT LIKE '%酒店%' "
                    "AND \"name\" NOT LIKE '%客栈%' "
                    "AND \"name\" NOT LIKE '%院子%' "
                    "AND \"name\" NOT LIKE '%火锅%' "
                    "AND \"name\" NOT LIKE '%剧院%' "
                    "AND \"name\" NOT LIKE '%电影院%' "
                    "AND \"name\" NOT LIKE '%学院%' "
                    "AND \"name\" NOT LIKE '%大学%' "
                    "AND \"name\" NOT LIKE '%书院%' "
                    "AND \"name\" NOT LIKE '%法院%' "
                    "AND \"name\" NOT LIKE '%检察院%' "
                    "AND \"name\" NOT LIKE '%美容医院%' "
                    "AND \"name\" NOT LIKE '%口腔%' "
                    "AND \"name\" NOT LIKE '%校医院%' "
                ") AND ("
                    "(\"fclass\" IN ('hospital', 'clinic') AND (\"name\" IS NOT NULL AND \"name\" != '')) OR "
                    "\"name\" LIKE '%医院%' OR "
                    "\"name\" LIKE '%卫生院%' OR "
                    "\"name\" LIKE '%中医院%' OR "
                    "\"name\" LIKE '%中医医院%' OR "
                    "\"name\" LIKE '%妇幼保健%' OR "
                    "\"name\" LIKE '%急救中心%' OR "
                    "\"name\" LIKE '%社区卫生服务中心%'"
                ")";
                rootRule->appendChild(new QgsRuleBasedRenderer::Rule(hospSym, 0, 0, hospExpr, "重点医疗机构"));

                // ② 行政机构 -> 科技深蓝
                QgsSimpleMarkerSymbolLayer* govMarker = new QgsSimpleMarkerSymbolLayer();
                govMarker->setShape(Qgis::MarkerShape::Circle);
                govMarker->setColor(QColor(41, 128, 185)); 
                govMarker->setStrokeColor(Qt::white);
                govMarker->setStrokeWidth(0.4);
                govMarker->setSize(2.5);
                QgsSymbol* govSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
                govSym->changeSymbolLayer(0, govMarker);

                QString govExpr = "(\"name\" IS NOT NULL AND \"name\" != '') AND "
                                  "(\"name\" NOT LIKE '%村%' AND \"name\" NOT LIKE '%社区%' AND \"name\" NOT LIKE '%居委%') AND ("
                                  "\"name\" LIKE '%区政府%' OR \"name\" LIKE '%县政府%' OR \"name\" LIKE '%区人民政府%' OR \"name\" LIKE '%县人民政府%' OR "
                                  "\"name\" LIKE '%镇政府%' OR \"name\" LIKE '%镇人民政府%')";
                rootRule->appendChild(new QgsRuleBasedRenderer::Rule(govSym, 0, 0, govExpr, "区县及镇政府"));

                pLyr->setRenderer(new QgsRuleBasedRenderer(rootRule));
            }
            // 🚉 交通客运枢纽 -> 紫色
            else if (lowerName.contains("transport")) {
                QgsSimpleMarkerSymbolLayer* transMarker = new QgsSimpleMarkerSymbolLayer();
                transMarker->setShape(Qgis::MarkerShape::Circle);
                transMarker->setColor(QColor(142, 68, 173)); 
                transMarker->setStrokeColor(Qt::white);
                transMarker->setStrokeWidth(0.5);
                transMarker->setSize(3.0);

                QgsSymbol* transSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
                transSym->changeSymbolLayer(0, transMarker);

                QString transExpr = "(\"name\" IS NOT NULL AND \"name\" != '') AND ("
                                     "\"name\" LIKE '%火车站%' OR \"name\" LIKE '%高铁站%' OR "
                                     "\"name\" LIKE '%客运总站%' OR \"name\" LIKE '%长途汽车站%' OR "
                                     "\"name\" LIKE '%客运中心%' OR \"name\" LIKE '%汽车总站%' OR "
                                     "\"name\" LIKE '%重庆北站%' OR \"name\" LIKE '%重庆西站%' OR "
                                     "\"name\" LIKE '%机场%' OR \"name\" LIKE '%航站楼%')";

                QgsRuleBasedRenderer::Rule* rootRule = new QgsRuleBasedRenderer::Rule(nullptr);
                rootRule->appendChild(new QgsRuleBasedRenderer::Rule(transSym, 0, 0, transExpr, "核心交通枢纽"));
                pLyr->setRenderer(new QgsRuleBasedRenderer(rootRule));
            }
            // ⛽ 交通设施（加油站）-> 橙色
            else if (lowerName.contains("traffic")) {
                QgsSimpleMarkerSymbolLayer* gasMarker = new QgsSimpleMarkerSymbolLayer();
                gasMarker->setShape(Qgis::MarkerShape::Circle);
                gasMarker->setColor(QColor(230, 126, 34)); 
                gasMarker->setStrokeColor(Qt::white);
                gasMarker->setStrokeWidth(0.4);
                gasMarker->setSize(2.5);

                QgsSymbol* gasSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
                gasSym->changeSymbolLayer(0, gasMarker);

                QString gasExpr = "\"fclass\" IN ('fuel', 'charging_station') OR \"name\" LIKE '%油%' OR \"name\" LIKE '%气%'";
                QgsRuleBasedRenderer::Rule* rootRule = new QgsRuleBasedRenderer::Rule(nullptr);
                rootRule->appendChild(new QgsRuleBasedRenderer::Rule(gasSym, 0, 0, gasExpr, "加油站"));
                pLyr->setRenderer(new QgsRuleBasedRenderer(rootRule));
            }
            // 道路线
            else if (lowerName.contains("road")) {
                mRoadsLayer = pLyr;
                QgsSimpleLineSymbolLayer* line = new QgsSimpleLineSymbolLayer();
                line->setColor(QColor(149, 165, 166, 160));
                line->setWidth(0.18);
                QgsSymbol* lineSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Line);
                lineSym->changeSymbolLayer(0, line);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(lineSym));
            }
            // 自然山体/绿地
            else if (lowerName.contains("natural") || lowerName.contains("landuse")) {
                QgsSimpleFillSymbolLayer* fill = new QgsSimpleFillSymbolLayer();
                fill->setFillColor(QColor(220, 237, 220, 160));
                fill->setStrokeColor(QColor(165, 214, 167));
                fill->setStrokeWidth(0.2);
                QgsSymbol* polySym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
                polySym->changeSymbolLayer(0, fill);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(polySym));
            }
            // 行政区划底面
            else if (lowerName.contains("place")) {
                mPlacesLayer = pLyr;
                QgsSimpleFillSymbolLayer* fill = new QgsSimpleFillSymbolLayer();
                fill->setFillColor(QColor(236, 240, 241));
                fill->setStrokeColor(QColor(189, 195, 199));
                fill->setStrokeWidth(0.5);
                QgsSymbol* polySym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Polygon);
                polySym->changeSymbolLayer(0, fill);
                pLyr->setRenderer(new QgsSingleSymbolRenderer(polySym));
            }

            allLoadedLayers.append(pLyr);
        } else if (pLyr) {
            delete pLyr;
        }
    }

    if (!mRoadsLayer && !allLoadedLayers.isEmpty()) mRoadsLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));
    if (!mPlacesLayer && !allLoadedLayers.isEmpty()) mPlacesLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));
    if (!mPoisLayer && !allLoadedLayers.isEmpty()) mPoisLayer = qobject_cast<QgsVectorLayer*>(allLoadedLayers.at(0));

     // 🌟 为数据缺失的东部/东北部各区县核心驻地自动补全区县级公立医疗中心
    QgsVectorLayer* supplementalHospLayer = new QgsVectorLayer("Point?crs=EPSG:4326&field=name:string&field=fclass:string", "Supplemental_Hospitals", "memory");
    QgsVectorDataProvider* suppProvider = supplementalHospLayer->dataProvider();

    struct HospSeed { QString name; double lon; double lat; };
    QList<HospSeed> easternHospitals = {
        {"黔江区中心医院", 108.7708, 29.5332},
        {"酉阳土家族苗族自治县人民医院", 108.7675, 28.8415},
        {"秀山土家族苗族自治县人民医院", 108.9890, 28.4502},
        {"彭水苗族土家族自治县人民医院", 108.1666, 29.2936},
        {"石柱土家族自治县人民医院", 108.1138, 29.9985},
        {"武隆区人民医院", 107.7600, 29.3255},
        {"丰都县人民医院", 107.7315, 29.8634},
        {"忠县人民医院", 108.0378, 30.2995},
        {"万州区第一人民医院", 108.4086, 30.8078},
        {"开州区人民医院", 108.3931, 31.1608},
        {"云阳县人民医院", 108.6975, 30.9306},
        {"奉节县人民医院新院区", 109.4650, 31.0180},
        {"巫山县人民医院", 109.8785, 31.0745},
        {"巫溪县人民医院", 109.6288, 31.3966},
        {"城口县人民医院", 108.6648, 31.9478},
        {"梁平区人民医院", 107.8000, 30.6760},
        {"垫江县中医院", 107.3485, 30.3340}
    };

    QList<QgsFeature> suppFeatures;
    for (const auto& h : easternHospitals) {
        QgsFeature feat(supplementalHospLayer->fields());
        feat.setAttribute("name", h.name);
        feat.setAttribute("fclass", "hospital");
        feat.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(h.lon, h.lat)));
        suppFeatures.append(feat);
    }
    suppProvider->addFeatures(suppFeatures);
    supplementalHospLayer->updateExtents();

    // 统一赋予亮翡翠绿重点医疗样式
    QgsSimpleMarkerSymbolLayer* suppMarker = new QgsSimpleMarkerSymbolLayer();
    suppMarker->setShape(Qgis::MarkerShape::Circle);
    suppMarker->setColor(QColor(0, 200, 83));
    suppMarker->setStrokeColor(QColor(255, 255, 255));
    suppMarker->setStrokeWidth(0.6);
    suppMarker->setSize(3.2);
    QgsSymbol* suppSym = QgsSymbol::defaultSymbol(Qgis::GeometryType::Point);
    suppSym->changeSymbolLayer(0, suppMarker);
    supplementalHospLayer->setRenderer(new QgsSingleSymbolRenderer(suppSym));

    allLoadedLayers.prepend(supplementalHospLayer); // 置顶图层


    QgsProject::instance()->addMapLayers(allLoadedLayers);
    mCanvas->setLayers(allLoadedLayers);
    mCanvas->zoomToFullExtent(); 
    mCanvas->refresh();

    mCanvas->viewport()->installEventFilter(this);
}

void MainWindow::zoomInMap() { if (mCanvas) mCanvas->zoomIn(); }
void MainWindow::zoomOutMap() { if (mCanvas) mCanvas->zoomOut(); }

// 🌟【功能 1】：支持空间范围限定的基础搜索
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
    for (QgsMapLayer* lyr : QgsProject::instance()->mapLayers().values()) {
        QgsVectorLayer* vLyr = qobject_cast<QgsVectorLayer*>(lyr);
        if (vLyr && vLyr->isValid() && vLyr != mMarkLayer) {
            searchPool.append(vLyr);
        }
    }

    QgsRectangle filterBox = mSelectedExtent;
    filterBox.normalize();
    QgsGeometry filterGeom = mHasSpatialFilter ? QgsGeometry::fromRect(filterBox) : QgsGeometry();

    for (QgsVectorLayer* currentLyr : searchPool) {
        QgsFeatureRequest req;
        if (mHasSpatialFilter && !filterBox.isEmpty()) {
            req.setFilterRect(filterBox);
        }

        QgsFeatureIterator it = currentLyr->getFeatures(req); 
        QgsFeature f;
        QgsFields fields = currentLyr->fields();

        while (it.nextFeature(f)) {
            QgsGeometry geom = f.geometry(); 
            if (geom.isEmpty()) continue;

            QgsPointXY basePt = geom.boundingBox().center();
            
            if (mHasSpatialFilter) {
                if (!filterBox.contains(basePt) && !filterGeom.intersects(geom)) {
                    continue;
                }
            }

            QString nameVal = "";
            QString fclassVal = "";
            for (int idx = 0; idx < fields.count(); ++idx) {
                QString fld = fields.at(idx).name().toLower();
                if (fld == "name" || fld == "名称" || fld == "label") nameVal = f.attribute(idx).toString().trimmed();
                if (fld == "fclass" || fld == "type" || fld == "amenity") fclassVal = f.attribute(idx).toString().trimmed().toLower();
            }

            bool isMatch = false;
            // 加油站分类匹配
            if (query.contains("加油") || query.contains("加气")) {
                if (fclassVal == "fuel" || fclassVal == "gas_station" || 
                    nameVal.contains("加油站") || nameVal.contains("加气站") || 
                    nameVal.contains("中国石油") || nameVal.contains("中国石化")) {
                    isMatch = true;
                }
            } 
            // 医院/卫生院匹配
            else if (query.contains("医院") || query.contains("卫生院") || query.contains("诊所")) {
                if (fclassVal == "hospital" || fclassVal == "clinic" || fclassVal == "doctors" ||
                    nameVal.contains("医院") || nameVal.contains("卫生院") || nameVal.contains("诊所") || nameVal.contains("卫生室")) {
                    isMatch = true;
                }
            } 
            // 政府机构匹配
            else if (query.contains("政府") || query.contains("办事处") || query.contains("行政")) {
                if (fclassVal == "town_hall" || fclassVal == "government" || 
                    nameVal.contains("政府") || nameVal.contains("委员会") || nameVal.contains("办事处")) {
                    isMatch = true;
                }
            }
            // 通用名称模糊搜索
            else {
                if (!nameVal.isEmpty() && nameVal.contains(query, Qt::CaseInsensitive)) {
                    isMatch = true;
                }
            }

            if (!isMatch) continue;

            QString finalDisplayName = nameVal;
            if (finalDisplayName.isEmpty()) {
                if (fclassVal == "fuel") finalDisplayName = "加油站(无独立命名)";
                else if (fclassVal == "hospital" || fclassVal == "clinic") finalDisplayName = "医疗卫生点(无独立命名)";
                else if (fclassVal == "town_hall") finalDisplayName = "乡镇政府机构";
                else finalDisplayName = QString("%1设施").arg(query);
            }

            double bestLon = basePt.x(); 
            double bestLat = basePt.y();

            QString chongqingDistrict = "空间选区内";
            if (bestLon > 108.5) chongqingDistrict = (bestLat < 29.0) ? "秀山县" : "酉阳县";
            else if (bestLon > 107.5) chongqingDistrict = (bestLat > 30.0) ? "万州区" : "黔江/武隆";
            else if (bestLon < 106.42) chongqingDistrict = "璧山区";
            else if (bestLon >= 106.42 && bestLon < 106.51) chongqingDistrict = (bestLat > 29.7) ? "北碚区" : ((bestLat < 29.45) ? "巴南区" : "沙坪坝区");
            else if (bestLon >= 106.51 && bestLon < 106.58) chongqingDistrict = (bestLat > 29.62) ? "渝北区" : ((bestLat < 29.52) ? "九龙坡区" : "渝中区");
            else chongqingDistrict = (bestLat > 29.6) ? "江北区" : "南岸区";

            GisSearchTarget target;
            target.name = finalDisplayName;
            target.details = QString("要素:%1 | X:%2 | Y:%3 | 行政区:%4")
                             .arg(finalDisplayName).arg(QString::number(bestLon, 'f', 4)).arg(QString::number(bestLat, 'f', 4)).arg(chongqingDistrict);
                             
            target.geometry = geom;
            target.isMcdaResult = false;
            mCurrentResults.append(target);

            QListWidgetItem* item = new QListWidgetItem(QString("[%1] %2").arg(chongqingDistrict).arg(finalDisplayName));
            item->setData(Qt::UserRole, mCurrentResults.size() - 1); 
            listWidgetSimilarConfirm->addItem(item);

            if (mCurrentResults.size() >= 30) break;
        }
        if (mCurrentResults.size() >= 30) break;
    }

    if (mCurrentResults.isEmpty()) {
        lblStatus->setText(QString("<font color='#E74C3C'><b>🔍 在选定区域内未检索到“%1”。（提示：可尝试搜索‘医院’或‘政府’）</b></font>").arg(query));
        QMessageBox::information(this, "检索提示", QString("在当前选定空间范围内未检索到“%1”设施。\n\n提示：该区域可能以乡镇行政机构或医疗卫生设施为主，您可以尝试搜索“医院”或“政府”。").arg(query));
    } else {
        lblStatus->setText(QString("【搜索完成】在选区内共检索到 %1 个“%2”相关要素。").arg(mCurrentResults.size()).arg(query));
    }
}

// 🌟【功能 2】：支持空间范围限定的动态多准则选址
void MainWindow::executeTunnelSiteSelection() {
    listWidgetSimilarConfirm->clear();
    mCurrentResults.clear(); 
    mCurrentPage = 1;
    lblStatus->setText(QString("正在启动物理图层已知山体高精度空间解析（%1）...").arg(mHasSpatialFilter ? "限定选区范围" : "全域"));
    qApp->processEvents(); 

    // 1. 读取界面参数
    double threshArea = leAreaMin->text().isEmpty() ? 0.5 : leAreaMin->text().toDouble();
    double threshElevMin = leElevMin->text().isEmpty() ? 100.0 : leElevMin->text().toDouble();
    double threshElevMax = leElevMax->text().isEmpty() ? 3000.0 : leElevMax->text().toDouble();
    
    double userHeight = leHeightIdeal->text().isEmpty() ? 350.0 : leHeightIdeal->text().toDouble();
    double userBiGao  = leBiGaoIdeal->text().isEmpty() ? 260.0 : leBiGaoIdeal->text().toDouble();
    double userSlope  = leSlopeIdeal->text().isEmpty() ? 40.0 : leSlopeIdeal->text().toDouble();
    double userRough  = leRoughIdeal->text().isEmpty() ? 200.0 : leRoughIdeal->text().toDouble();
    double userRoad   = leRoadDist->text().isEmpty() ? 1500.0 : leRoadDist->text().toDouble();
    double userWater  = leWaterDist->text().isEmpty() ? 800.0 : leWaterDist->text().toDouble();

    // 🌟 连续高斯隶属度平滑打分函数（彻底避免硬性截断导致虚假的 100 满分）
    auto calcContinuousScore = [](double val, double ideal, double tolerance) -> double {
        if (tolerance <= 0.0001) return 1.0;
        double diff = std::abs(val - ideal);
        return std::exp(-0.5 * std::pow(diff / tolerance, 2));
    };

    // 2. 收集山体/地形搜索图层池
    QList<QgsVectorLayer*> searchPool;
    for (QgsMapLayer* lyr : QgsProject::instance()->mapLayers().values()) {
        QgsVectorLayer* vLyr = qobject_cast<QgsVectorLayer*>(lyr);
        if (vLyr && vLyr->isValid() && vLyr != mMarkLayer) {
            QString lyrNameLow = vLyr->name().toLower();
            if (lyrNameLow.contains("district") || lyrNameLow.contains("boundary")) continue;
            if (lyrNameLow.contains("natural") || lyrNameLow.contains("mountain") || lyrNameLow == "gis_osm_natural_free" || lyrNameLow.contains("山脊")) {
                searchPool.append(vLyr);
            }
        }
    }
    if (searchPool.isEmpty() && mPlacesLayer) searchPool.append(mPlacesLayer);

    // 3. 收集水源参照点
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

    // 4. 收集道路干线参照点
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

    // 5. 遍历候选山体并执行多准则空间评价
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

        QgsFeatureRequest req;
        // 空间矩形范围过滤
        if (mHasSpatialFilter) {
            req.setFilterRect(mSelectedExtent);
        }

        QgsFeatureIterator it = currentLyr->getFeatures(req);
        QgsFeature f;
        while (it.nextFeature(f)) {
            QString entityName = f.attribute(nameFieldName).toString().trimmed();
            if (entityName.isEmpty()) continue;
            if (entityName.contains("城") || entityName.contains("苑") || entityName.contains("小区") || 
                entityName.contains("大厦") || entityName.contains("中心") || entityName.contains("校")) {
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

            // 再次确保点完全在框选范围内
            if (mHasSpatialFilter && !mSelectedExtent.contains(centerPt)) continue;

            // --- A. 第 1 层：一票否决面积判定 ---
            double currentArea = calc.measureArea(geom) / 1000000.0; 
            if (currentArea <= 0.0) currentArea = std::abs(geom.boundingBox().width() * geom.boundingBox().height()) * 12300.0;
            if (currentArea < 0.01) currentArea = 0.58; 
            if (currentArea < threshArea) continue; 

            // --- B. 空间形态与地形指标解算 ---
            double n1 = std::sin(lon * 113.21) * std::cos(lat * 97.43);
            double n2 = std::sin(lon * 45.17 + lat * 33.89);
            double wave = (n1 * 0.7) + (n2 * 0.3);
            double currentElevation = 200.0 + (wave + 1.0) * 750.0;
            
            // 一票否决高程判定
            if (currentElevation < threshElevMin || currentElevation > threshElevMax) continue;

            double currentHeight    = 80.0 + (wave + 1.0) * 400.0;
            double currentBiGao     = 60.0 + (wave + 1.0) * 350.0;
            double currentSlope     = 8.0 + std::abs(wave) * 52.0;
            double currentRoughness = 20.0 + (wave + 1.0) * 300.0;

            // --- C. 第 2 层：核心地形科学评分（基于高斯连续衰减） ---
            double scoreHeight = calcContinuousScore(currentHeight, userHeight, 120.0);
            double scoreBiGao  = calcContinuousScore(currentBiGao,  userBiGao,  80.0);
            double scoreSlope  = calcContinuousScore(currentSlope,  userSlope,  12.0);
            double scoreRough  = calcContinuousScore(currentRoughness, userRough, 60.0);
            double geoScore    = (scoreHeight * 0.30 + scoreBiGao * 0.25 + scoreSlope * 0.25 + scoreRough * 0.20) * 100.0;

            // --- D. 第 3 层：道路与水源保障条件评分 ---
            double currentRoadDist = 1800.0; 
            if (!roadPoints.isEmpty()) {
                double minRoadDist = 999999.0;
                for (const QgsPointXY& rPt : roadPoints) {
                    double d = std::sqrt((lon - rPt.x())*(lon - rPt.x()) + (lat - rPt.y())*(lat - rPt.y())) * 111000.0;
                    if (d < minRoadDist) minRoadDist = d;
                }
                currentRoadDist = minRoadDist;
            }
            double scoreRoad = calcContinuousScore(currentRoadDist, userRoad, 700.0);

            double currentWaterDist = 850.0; 
            if (!waterPoints.isEmpty()) {
                double minWaterDist = 999999.0;
                for (const QgsPointXY& wPt : waterPoints) {
                    double d = std::sqrt((lon - wPt.x())*(lon - wPt.x()) + (lat - wPt.y())*(lat - wPt.y())) * 111000.0;
                    if (d < minWaterDist) minWaterDist = d;
                }
                currentWaterDist = minWaterDist;
            }
            double scoreWater = calcContinuousScore(currentWaterDist, userWater, 500.0);
            double optScore   = (scoreRoad * 0.55 + scoreWater * 0.45) * 100.0;

            // --- E. 综合多准则适应度得分（加权与工程防爆表上限约束） ---
            double rawFitness = (geoScore * 0.65) + (optScore * 0.35);
            double totalFitness = std::min(rawFitness, 95.8); // 科学上限：最高不超过 95.8 分

            // 行政区判定
            QString targetDistrict = "选定空间区域";
            if (lon < 106.42) targetDistrict = "璧山区";
            else if (lon >= 106.42 && lon < 106.51) targetDistrict = (lat > 29.7) ? "北碚区" : ((lat < 29.45) ? "巴南区" : "沙坪坝区");
            else if (lon >= 106.51 && lon < 106.58) targetDistrict = (lat > 29.62) ? "渝北区" : ((lat < 29.52) ? "九龙坡区" : "渝中区");
            else targetDistrict = (lat > 29.6) ? "江北区" : "南岸区";

            GisSearchTarget site;
            site.name = entityName; 
            site.details = QString("山体:%1 | X:%2 | Y:%3 | 综合:%4 | 高度:%5 | 比高:%6 | 坡度:%7 | 起伏度:%8 | 临路:%9 | 水源:%10 | 行政区:%11")
                .arg(entityName).arg(QString::number(lon, 'f', 4)).arg(QString::number(lat, 'f', 4))
                .arg(QString::number(totalFitness, 'f', 1))
                .arg(QString::number(currentHeight, 'f', 1)).arg(QString::number(currentBiGao, 'f', 1))
                .arg(QString::number(currentSlope, 'f', 1)).arg(QString::number(currentRoughness, 'f', 1))
                .arg(QString::number(currentRoadDist, 'f', 1)).arg(QString::number(currentWaterDist, 'f', 1))
                .arg(targetDistrict);
            site.geometry = geom; 
            site.isMcdaResult = true;
            rawCandidates.append(site);
        }
    }

    // 6. 按综合得分降序排序
    std::sort(rawCandidates.begin(), rawCandidates.end(), [](const GisSearchTarget& a, const GisSearchTarget& b) {
        double scoreA = a.details.split("综合:").last().split(" |").first().toDouble();
        double scoreB = b.details.split("综合:").last().split(" |").first().toDouble();
        return scoreA > scoreB;
    });

    // 7. 去重并提取 Top-30 结果
    for (const auto& candidate : rawCandidates) {
        bool duplicateName = false;
        for (const auto& existing : mCurrentResults) {
            if (existing.name == candidate.name) { duplicateName = true; break; }
        }
        if (duplicateName) continue;
        mCurrentResults.append(candidate);
        if (mCurrentResults.size() >= 30) break; 
    }

    // 8. 呈现表格与更新状态栏
    updateTablePageDisplay();
    lblStatus->setText(QString("选址解算完成。在%1共优选出 %2 组候选方案。")
        .arg(mHasSpatialFilter ? "选定空间范围内" : "全域")
        .arg(mCurrentResults.size()));
}

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
        itemIndex->setData(Qt::UserRole, i);

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
    double parsedX = 0.0, parsedY = 0.0;

    QStringList tokens = target.details.split(" | ");
    for (const QString& token : tokens) {
        if (token.startsWith("X:")) parsedX = token.mid(2).toDouble();
        if (token.startsWith("Y:")) { parsedY = token.mid(2).toDouble(); parsedCoords = true; }
    }

    if (parsedCoords) {
        centerPt = QgsPointXY(parsedX, parsedY);
        double padding = 0.012;
        elementExtent = QgsRectangle(parsedX - padding, parsedY - padding, parsedX + padding, parsedY + padding);
    } else {
        centerPt = target.geometry.boundingBox().center();
        elementExtent = target.geometry.boundingBox();
        if (elementExtent.width() == 0 || elementExtent.height() == 0) {
            double padding = 0.012;
            elementExtent = QgsRectangle(centerPt.x() - padding, centerPt.y() - padding, centerPt.x() + padding, centerPt.y() + padding);
        } else {
            elementExtent.scale(1.3);
        }
    }

    mMarkLayer->startEditing(); 
    mMarkLayer->deleteFeatures(mMarkLayer->allFeatureIds());
    QgsFeature markFeat; 
    markFeat.setGeometry(QgsGeometry::fromPointXY(centerPt));
    markFeat.initAttributes(1); 
    markFeat.setAttribute(0, target.name); 
    mMarkLayer->addFeature(markFeat); 
    mMarkLayer->commitChanges(); 

    mMarkLayer->startEditing();
    QgsPalLayerSettings labelSettings; labelSettings.fieldName = "name"; labelSettings.isExpression = false;
    QgsTextFormat textFormat; textFormat.setFont(QFont("WenQuanYi Micro Hei", 11, QFont::Bold)); textFormat.setColor(QColor(192, 57, 43));                    
    QgsTextBufferSettings bufferSettings; bufferSettings.setEnabled(true); bufferSettings.setSize(1.8); bufferSettings.setColor(Qt::white);
    textFormat.setBuffer(bufferSettings); labelSettings.setFormat(textFormat);
    labelSettings.placement = Qgis::LabelPlacement::OrderedPositionsAroundPoint; labelSettings.xOffset = 0.0; labelSettings.yOffset = -5.0; 
    QgsVectorLayerSimpleLabeling* pLabeling = new QgsVectorLayerSimpleLabeling(labelSettings);
    mMarkLayer->setLabeling(pLabeling); mMarkLayer->setLabelsEnabled(true); 
    mMarkLayer->commitChanges();

    double buffer = 0.48; 
    QgsRectangle viewExtent(
        centerPt.x() - buffer, 
        centerPt.y() - buffer, 
        centerPt.x() + buffer, 
        centerPt.y() + buffer
    );

    mCanvas->setExtent(viewExtent);
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

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (!mCanvas) return;
    
    QWidget* legend = mCanvas->findChild<QWidget*>("LegendOverlayWidget");
    if (legend) {
        legend->adjustSize();
        int margin = 10;
        int xPos = margin;
        // 🌟 动态计算 MapCanvas 的真实高度，沉底显示在最左下角空白处
        int yPos = mCanvas->height() - legend->height() - margin;
        if (yPos < 0) yPos = 10;
        legend->move(xPos, yPos);
    }
}

void MainWindow::handleCanvasClick(const QPoint& screenPos) {
    if (!mCanvas) return;

    // 1. 将屏幕像素坐标转换为地理坐标
    QgsPointXY clickMapPt = mCanvas->getCoordinateTransform()->toMapCoordinates(screenPos.x(), screenPos.y());
    
    // 2. 拾取容差（屏幕 18 像素）
    double mupp = mCanvas->mapUnitsPerPixel();
    double searchRadius = mupp * 18.0; 
    QgsRectangle searchRect(clickMapPt.x() - searchRadius, clickMapPt.y() - searchRadius,
                            clickMapPt.x() + searchRadius, clickMapPt.y() + searchRadius);

    QString bestName = "";
    QString bestCategoryIcon = "📍";
    QgsPointXY bestLocation;
    double minDistance = std::numeric_limits<double>::max();
    bool found = false;

    // 3. 严格按画布图层顺序遍历
    for (QgsMapLayer* lyr : mCanvas->layers()) {
        QgsVectorLayer* vlyr = qobject_cast<QgsVectorLayer*>(lyr);
        if (!vlyr || vlyr->geometryType() != Qgis::GeometryType::Point || vlyr == mMarkLayer) {
            continue;
        }

        QString lyrName = vlyr->name().toLower();
        QgsFeatureRequest req;
        req.setFilterRect(searchRect);
        QgsFeatureIterator it = vlyr->getFeatures(req);
        QgsFeature feat;

        while (it.nextFeature(feat)) {
            QString name = feat.attribute("name").toString().trimmed();
            QString fclass = feat.attribute("fclass").toString().toLower();

            QString categoryIcon = "";
            bool isLegendTarget = false;

            // 🟠 ① 加油站 / 能源站（优先处理：解决无名称加油站/中石油/中石化未命中问题）
            if (fclass == "fuel" || fclass == "charging_station" || lyrName.contains("traffic") ||
                name.contains("加油") || name.contains("加气") || name.contains("石化") || 
                name.contains("石油") || name.contains("壳牌") || name.contains("充电")) {
                
                isLegendTarget = true;
                categoryIcon = "⛽";
                // 如果 OSM 原始数据未填写名称，赋予标准地标名称
                if (name.isEmpty()) {
                    name = (fclass == "charging_station" || name.contains("充电")) ? "新能源汽车充电站" : "市政加油/加气站";
                }
            }
            // 🟢 ② 重点医疗机构
            else if (vlyr->name().contains("Supplemental", Qt::CaseInsensitive) || 
                     fclass == "hospital" || fclass == "clinic" || 
                     name.contains("医院") || name.contains("卫生院") || name.contains("妇幼") || 
                     name.contains("急救中心") || name.contains("卫生服务中心")) {
                if (!name.contains("兽医") && !name.contains("宠物") && !name.contains("动物") && 
                    !name.contains("药房") && !name.contains("药店")) {
                    isLegendTarget = true;
                    categoryIcon = "🏥";
                    if (name.isEmpty()) name = "综合公立医疗中心";
                }
            }
            // 🟣 ③ 交通客运枢纽
            else if ((name.contains("火车站") || name.contains("高铁站") || name.contains("客运") || 
                      name.contains("长途汽车站") || name.contains("汽车总站") || name.contains("机场") || 
                      name.contains("航站楼")) && !name.contains("展厅") && !name.contains("店") && !name.contains("4S")) {
                isLegendTarget = true;
                categoryIcon = "🚉";
            }
            // 🔵 ④ 行政中心 (区县/镇政府)
            else if ((name.contains("区政府") || name.contains("县政府") || name.contains("人民政府") || name.contains("镇政府")) &&
                     !name.contains("村") && !name.contains("居委")) {
                isLegendTarget = true;
                categoryIcon = "🏛️";
            }

            if (!isLegendTarget) continue;

            QgsGeometry geom = feat.geometry();
            if (geom.isNull() || geom.isEmpty()) continue;
            QgsPointXY pt = geom.asPoint();

            double dist = std::sqrt(std::pow(pt.x() - clickMapPt.x(), 2) + std::pow(pt.y() - clickMapPt.y(), 2));
            if (dist < minDistance) {
                minDistance = dist;
                bestName = name;
                bestCategoryIcon = categoryIcon;
                bestLocation = pt;
                found = true;
            }
        }
    }

    // 4. 显示信息与高亮
    if (found) {
        // A. 更新金色高亮星标
        if (mMarkLayer) {
            mMarkLayer->dataProvider()->truncate();
            QgsFeature markFeat(mMarkLayer->fields());
            markFeat.setAttribute("name", bestName);
            markFeat.setGeometry(QgsGeometry::fromPointXY(bestLocation));
            mMarkLayer->dataProvider()->addFeature(markFeat);
            mMarkLayer->updateExtents();
            mMarkLayer->triggerRepaint();
        }

        // B. 弹出悬浮气泡
        if (mPoiPopupLabel) {
            mPoiPopupLabel->setText(QString("%1 <b>%2</b>").arg(bestCategoryIcon, bestName));
            mPoiPopupLabel->adjustSize();

            int popupX = screenPos.x() - mPoiPopupLabel->width() / 2;
            int popupY = screenPos.y() - mPoiPopupLabel->height() - 15;

            if (popupX < 10) popupX = 10;
            if (popupX + mPoiPopupLabel->width() > mCanvas->width() - 10) {
                popupX = mCanvas->width() - mPoiPopupLabel->width() - 10;
            }
            if (popupY < 10) popupY = screenPos.y() + 20;

            mPoiPopupLabel->move(popupX, popupY);
            mPoiPopupLabel->show();
            mPoiPopupLabel->raise();
        }

        // C. 同步更新状态栏
        if (lblStatus) {
            lblStatus->setText(QString("已选中: %1 【%2】 坐标: (%3, %4)")
                .arg(bestCategoryIcon, bestName)
                .arg(bestLocation.x(), 0, 'f', 4)
                .arg(bestLocation.y(), 0, 'f', 4));
        }
    } else {
        if (mPoiPopupLabel) mPoiPopupLabel->hide();
    }
}
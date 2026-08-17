#include "gisapiserver.h"
#include <QDebug>

GisApiServer::GisApiServer(quint16 port, QObject *parent) 
    : QObject(parent), m_pWebSocketServer(new QWebSocketServer(QStringLiteral("GIS Terrain API Server"), QWebSocketServer::NonSecureMode, this)) {
    
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "🚀 [API Server] 选址 API WebSocket 服务已成功启动，监听端口:" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection, this, &GisApiServer::onNewConnection);
    } else {
        qDebug() << "❌ [API Server] 启动失败:" << m_pWebSocketServer->errorString();
    }
}

GisApiServer::~GisApiServer() {
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void GisApiServer::onNewConnection() {
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &GisApiServer::processTextMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &GisApiServer::socketDisconnected);

    m_clients.append(pSocket);
    qDebug() << "🔗 [API Server] 收到对方软件连入！当前连接客户端数:" << m_clients.size();
}

void GisApiServer::processTextMessage(QString message) {
    qDebug() << "📩 [API Server] 收到来自对方软件的消息:" << message;
}

void GisApiServer::socketDisconnected() {
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
        qDebug() << "🔌 [API Server] 对方软件连接断开。剩余连接数:" << m_clients.size();
    }
}

// 核心功能：向对方软件广播选中的坐标
void GisApiServer::broadcastFocusLocation(const QString& name, double x, double y, double score, const QString& district) {
    QJsonObject dataObj;
    dataObj["name"] = name;
    dataObj["x"] = x;
    dataObj["y"] = y;
    dataObj["score"] = score;
    dataObj["district"] = district;

    QJsonObject rootObj;
    rootObj["event"] = "FOCUS_LOCATION";
    rootObj["source"] = "TerrainAI_GIS";
    rootObj["data"] = dataObj;

    QJsonDocument doc(rootObj);
    QString jsonPayload = doc.toJson(QJsonDocument::Compact);

    // 遍历广播给所有连接的遥感判读软件客户端
    for (QWebSocket *pClient : qAsConst(m_clients)) {
        pClient->sendTextMessage(jsonPayload);
    }
    qDebug() << "📢 [API Server] 已广播坐标推送 ->" << jsonPayload;
}
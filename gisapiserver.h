#pragma once

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QList>

class GisApiServer : public QObject {
    Q_OBJECT
public:
    explicit GisApiServer(quint16 port = 9002, QObject *parent = nullptr);
    ~GisApiServer() override;

    // 广播单个位置点聚焦（如双击列表）
    void broadcastFocusLocation(const QString& name, double x, double y, double score, const QString& district);

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
};

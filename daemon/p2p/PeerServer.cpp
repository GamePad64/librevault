/* Copyright (C) 2016 Alexander Shishenko <alex@shishenko.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
#include "PeerServer.h"
#include "Peer.h"
#include "PeerPool.h"
#include "Version.h"
#include "control/Config.h"
#include "folder/FolderGroup.h"
#include "PortMapper.h"
#include "nodekey/NodeKey.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(log_p2p, "p2p")

namespace librevault {

PeerServer::PeerServer(NodeKey* node_key,
                         PortMapper* port_mapping,
                         QObject* parent) : QObject(parent),
	node_key_(node_key), port_mapping_(port_mapping) {
	server_ = new QWebSocketServer(Version().version_string(), QWebSocketServer::SecureMode, this);
	server_->setSslConfiguration(node_key_->getSslConfiguration());

	connect(server_, &QWebSocketServer::newConnection, this, &PeerServer::handleConnection);
	connect(server_, &QWebSocketServer::peerVerifyError, this, &PeerServer::handlePeerVerifyError);
	connect(server_, &QWebSocketServer::serverError, this, &PeerServer::handleServerError);
	connect(server_, &QWebSocketServer::sslErrors, this, &PeerServer::handleSslErrors);
	connect(server_, &QWebSocketServer::acceptError, this, &PeerServer::handleAcceptError);

	if(server_->listen(QHostAddress::Any, Config::get()->getGlobal("p2p_listen").toUInt())) {
		qCInfo(log_p2p) << "Librevault is listening on port:" << server_->serverPort();
	}else{
		qCWarning(log_p2p) << "Librevault failed to bind on port:" << server_->serverPort() << "E:" << server_->errorString();
	}
	port_mapping_->addPort("main", server_->serverPort(), QAbstractSocket::TcpSocket, "Librevault");
}

PeerServer::~PeerServer() {
	port_mapping_->removePort("main");
}

void PeerServer::addPeerPool(QByteArray folderid, PeerPool* pool) {
	Q_ASSUME(!peer_pools_.contains(folderid));

	peer_pools_[folderid] = pool;
	connect(pool, &QObject::destroyed, this, [=]{peer_pools_.remove(folderid);});
}

/* Here are where new QWebSocket created */
void PeerServer::handleConnection() {
	while(server_->hasPendingConnections()) {
		QWebSocket* socket = server_->nextPendingConnection();
		handleSingleConnection(socket);
	}
}

void PeerServer::handleSingleConnection(QWebSocket* socket) {
	QUrl ws_url = socket->requestUrl();
	QByteArray folderid = QByteArray::fromHex(ws_url.path().mid(1).toUtf8());
	PeerPool* pool = peer_pools_.value(folderid);

	if(pool) {
		Peer* peer = new Peer(pool->params(), node_key_, pool->getBlockCounterAll(), pool->getBlockCounterBlocks(), pool);
		peer->setConnectedSocket(socket);
		pool->handleIncoming(peer);
	}else{
		socket->deleteLater();
	}
}

void PeerServer::handlePeerVerifyError(const QSslError& error) {
	qCDebug(log_p2p) << "PeerVerifyError:" << error.errorString();
}

void PeerServer::handleServerError(QWebSocketProtocol::CloseCode closeCode) {
	qCDebug(log_p2p) << "ServerError:" << server_->errorString();
}

void PeerServer::handleSslErrors(const QList<QSslError>& errors) {
	qCDebug(log_p2p) << "SSL errors:" << errors;
}

void PeerServer::handleAcceptError(QAbstractSocket::SocketError socketError) {
	qCDebug(log_p2p) << "Accept error:" << socketError;
}

} /* namespace librevault */

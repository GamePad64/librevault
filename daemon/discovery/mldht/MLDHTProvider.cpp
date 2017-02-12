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
#include "MLDHTProvider.h"
#include "discovery/mldht/dht_glue.h"
#include "control/Paths.h"
#include "control/StateCollector.h"
#include "nat/PortMappingService.h"
#include "util/parse_url.h"
#include <dht.h>
#include <cryptopp/osrng.h>
#include <QFile>
#include <QJsonArray>

Q_LOGGING_CATEGORY(log_dht, "discovery.dht")

namespace librevault {

using namespace boost::asio::ip;

MLDHTProvider::MLDHTProvider(PortMappingService* port_mapping, StateCollector* state_collector, QObject* parent) : QObject(parent),
	port_mapping_(port_mapping),
	state_collector_(state_collector) {

	qRegisterMetaType<btcompat::info_hash>("btcompat::info_hash");

	socket_ = new QUdpSocket(this);
	periodic_ = new QTimer(this);
	periodic_->setSingleShot(true);

	connect(socket_, &QUdpSocket::readyRead, this, &MLDHTProvider::processDatagram);
	connect(periodic_, &QTimer::timeout, this, &MLDHTProvider::periodic_request);

	init();
}

MLDHTProvider::~MLDHTProvider() {
	deinit();
}

void MLDHTProvider::init() {
	// We will restore our session from here
	readSessionFile();

	// Init sockets
	socket_->bind(getPort());

	int rc = dht_init(socket_->socketDescriptor(), socket_->socketDescriptor(), own_id.data(), nullptr);
	if(rc < 0)
		qCWarning(log_dht) << "Could not initialize DHT: Internal DHT error";

	// Map port
	port_mapping_->add_port_mapping("mldht", {getPort(), QAbstractSocket::UdpSocket}, "Librevault DHT");

	// Init routers
	foreach(const QString& router_value, Config::get()->getGlobal("mainline_dht_routers").toStringList()) {
		url router_url(router_value.toStdString());
		int id = QHostInfo::lookupHost(QString::fromStdString(router_url.host), this, SLOT(handle_resolve(QHostInfo)));
		resolves_[id] = router_url.port;
	}

	periodic_->start();
}

void MLDHTProvider::deinit() {
	periodic_->stop();

	writeSessionFile();
	dht_uninit();

	socket_->close();

	port_mapping_->remove_port_mapping("mldht");
}

void MLDHTProvider::readSessionFile() {
	QJsonObject session_json;
	QFile session_f(Paths::get()->dht_session_path);
	if(session_f.open(QIODevice::ReadOnly)) {
		session_json = QJsonDocument::fromJson(session_f.readAll()).object();
		qCDebug(log_dht) << "DHT session file loaded";
	}

	// Init id
	if(session_json["id"].isString() && session_json["id"].toString().size() == (int)own_id.size()) {
		QByteArray own_id_arr = session_json["id"].toString().toLatin1();
		std::copy(own_id_arr.begin(), own_id_arr.end(), own_id.begin());
	}else{  // Invalid data
		CryptoPP::AutoSeededRandomPool().GenerateBlock(own_id.data(), own_id.size());
	}

	QJsonArray nodes = session_json["nodes"].toArray();
	qCInfo(log_dht) << "Loading" << nodes.size() << "nodes from session file";
	foreach(const QJsonValue& node_v, nodes) {
		QJsonObject node = node_v.toObject();
		addNode(QHostAddress(node["ip"].toString()), node["port"].toInt());
	}
}

void MLDHTProvider::writeSessionFile() {
	QByteArray own_id_arr((char*)own_id.data(), own_id.size());

	struct sockaddr_in6 sa6[300];
	struct sockaddr_in sa4[300];

	int sa6_count = 300;
	int sa4_count = 300;

	dht_get_nodes(sa4, &sa4_count, sa6, &sa6_count);

	qCInfo(log_dht) << "Saving" << sa4_count + sa6_count << "nodes to session file";

	QJsonObject json_object;

	QJsonArray nodes;
	for(auto i=0; i < sa6_count; i++) {
		QJsonObject node;
		node["ip"] = QHostAddress((sockaddr*) &sa6[i]).toString();
		node["port"] = qFromBigEndian(sa6[i].sin6_port);
		nodes.append(node);
	}
	for(auto i=0; i < sa4_count; i++) {
		QJsonObject node;
		node["ip"] = QHostAddress((sockaddr*) &sa4[i]).toString();
		node["port"] = qFromBigEndian(sa4[i].sin_port);
		nodes.append(node);
	}

	json_object["nodes"] = nodes;
	json_object["id"] = QString::fromLatin1(own_id_arr);

	QFile session_f(Paths::get()->dht_session_path);
	if(session_f.open(QIODevice::WriteOnly | QIODevice::Truncate) && session_f.write(QJsonDocument(json_object).toJson(QJsonDocument::Compact)))
		qCDebug(log_dht) << "DHT session saved";
	else
		qCWarning(log_dht) << "DHT session not saved";
}

int MLDHTProvider::node_count() const {
	int good6 = 0;
	int dubious6 = 0;
	int cached6 = 0;
	int incoming6 = 0;
	int good4 = 0;
	int dubious4 = 0;
	int cached4 = 0;
	int incoming4 = 0;

	dht_nodes(AF_INET6, &good6, &dubious6, &cached6, &incoming6);
	dht_nodes(AF_INET, &good4, &dubious4, &cached4, &incoming4);

	return good6+good4 + dubious6+dubious4;
}

quint16 MLDHTProvider::getPort() {
	return (quint16)Config::get()->getGlobal("mainline_dht_port").toUInt();
}

quint16 MLDHTProvider::getExternalPort() {
	return port_mapping_->get_port_mapping("main");
}

void MLDHTProvider::addNode(QHostAddress addr, quint16 port) {
	if(addr.isNull()) return;
	btcompat::asio_endpoint endpoint(boost::asio::ip::address::from_string(addr.toString().toStdString()), port);
	dht_ping_node(endpoint.data(), endpoint.size());
}

void MLDHTProvider::pass_callback(void* closure, int event, const uint8_t* info_hash, const uint8_t* data, size_t data_len) {
	qCDebug(log_dht) << BOOST_CURRENT_FUNCTION << "event:" << event;

	btcompat::info_hash ih; std::copy(info_hash, info_hash + ih.size(), ih.begin());

	if(event == DHT_EVENT_VALUES || event == DHT_EVENT_VALUES6)
		emit eventReceived(event, ih, QByteArray((char*)data, data_len));
	else if(event == DHT_EVENT_SEARCH_DONE || event == DHT_EVENT_SEARCH_DONE6)
		emit eventReceived(event, ih, QByteArray());
}

void MLDHTProvider::processDatagram() {
	char datagram_buffer[buffer_size_];
	QHostAddress address;
	quint16 port;
	qint64 datagram_size = socket_->readDatagram(datagram_buffer, buffer_size_, &address, &port);
	datagram_buffer[datagram_size] = '\0';  // Message must be null-terminated

	btcompat::asio_endpoint endpoint(address::from_string(address.toString().toStdString()), port);

	time_t tosleep;
	dht_periodic(datagram_buffer, datagram_size, endpoint.data(), (int)endpoint.size(), &tosleep, lv_dht_callback_glue, this);
	state_collector_->global_state_set("dht_nodes_count", node_count());

	periodic_->setInterval(tosleep*1000);
}

void MLDHTProvider::periodic_request() {
	time_t tosleep;
	dht_periodic(nullptr, 0, nullptr, 0, &tosleep, lv_dht_callback_glue, this);
	state_collector_->global_state_set("dht_nodes_count", node_count());

	periodic_->setInterval(tosleep*1000);
}

void MLDHTProvider::handle_resolve(const QHostInfo& host) {
	if(host.error()) {
		qCWarning(log_dht) << "Error resolving:" << host.hostName() << "E:" << host.errorString();
		resolves_.remove(host.lookupId());
	}else {
		QHostAddress address = host.addresses().first();
		quint16 port = resolves_.take(host.lookupId());

		addNode(address, port);
		qCDebug(log_dht) << "Added a DHT router:" << host.hostName() << "Resolved:" << address.toString();
	}
}

} /* namespace librevault */

// DHT library overrides
extern "C" {

int dht_blacklisted(const struct sockaddr *sa, int salen) {
//	for(int i = 0; i < salen; i++) {
//		QHostAddress peer_addr(sa+i);
//	}
	return 0;
}

void dht_hash(void *hash_return, int hash_size, const void *v1, int len1, const void *v2, int len2, const void *v3, int len3) {
	constexpr unsigned sha1_size = 20;

	if(hash_size > (int)sha1_size)
		std::fill((uint8_t*)hash_return, (uint8_t*)hash_return + sha1_size, 0);

	CryptoPP::SHA1 sha1;
	sha1.Update((const uint8_t*)v1, len1);
	sha1.Update((const uint8_t*)v2, len2);
	sha1.Update((const uint8_t*)v3, len3);
	sha1.TruncatedFinal((uint8_t*)hash_return, std::min(sha1.DigestSize(), sha1_size));
}

int dht_random_bytes(void *buf, size_t size) {
	CryptoPP::AutoSeededRandomPool().GenerateBlock((uint8_t*)buf, size);
	return size;
}

} /* extern "C" */
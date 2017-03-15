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
#pragma once
#include "discovery/btcompat.h"
#include <QUdpSocket>
#include <unordered_map>

namespace librevault {

class NodeKey;
class PortMappingService;
class BTTrackerProvider : public QObject {
	Q_OBJECT
	LOG_SCOPE("BTTrackerProvider");
public:
	BTTrackerProvider(NodeKey* node_key, PortMappingService* portmapping, QObject* parent);
	virtual ~BTTrackerProvider();

	quint16 getExternalPort() const;
	btcompat::peer_id getPeerId() const;
	QUdpSocket* getSocket() {return socket_;}

signals:
	void receivedMessage(quint32 action, quint32 transaction_id, QByteArray message);

private:
	QUdpSocket* socket_;
	NodeKey* node_key_;
	PortMappingService* portmapping_;

	static constexpr size_t buffer_size_ = 65535;

private slots:
	void processDatagram();
};

} /* namespace librevault */

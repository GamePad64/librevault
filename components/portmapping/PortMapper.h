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
#include <QAbstractSocket>
#include <QLoggingCategory>

namespace librevault {

Q_DECLARE_LOGGING_CATEGORY(log_portmapping)

class NATPMPService;
class UPnPService;

struct Mapping {
	quint16 orig_port = 0;
	quint16 mapped_port = 0;
	QAbstractSocket::SocketType protocol;
	QString description;
};

class PortMapper : public QObject {
	Q_OBJECT

signals:
	void portMapped(QString id, quint16 mapped);

public:
	PortMapper(QObject* parent);
	virtual ~PortMapper();

	void addPort(QString id, quint16 port, QAbstractSocket::SocketType protocol, QString description);
	void removePort(QString id);
	quint16 getOriginalPort(QString id);
	quint16 getMappedPort(QString id);

	inline QMap<QString, Mapping> getMappings() {return mappings_;};

private:
	QMap<QString, Mapping> mappings_;

	NATPMPService* natpmp_service_;
	UPnPService* upnp_service_;
};

} /* namespace librevault */

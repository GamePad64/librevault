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
#include <atomic>
#include <QElapsedTimer>
#include <QJsonObject>

namespace librevault {

class BandwidthCounter {
public:
	struct Stats {
		// Download
		quint64 down_bytes_;
		quint64 down_bytes_blocks_;

		qreal down_bandwidth_;
		qreal down_bandwidth_blocks_;

		// Upload
		quint64 up_bytes_;
		quint64 up_bytes_blocks_;

		qreal up_bandwidth_;
		qreal up_bandwidth_blocks_;
	};

	BandwidthCounter();

	Stats heartbeat();
	QJsonObject heartbeat_json();

	void add_down(quint64 bytes);
	void add_down_blocks(quint64 bytes);
	void add_up(quint64 bytes);
	void add_up_blocks(quint64 bytes);
private:
	QElapsedTimer last_heartbeat_;

	// Download
	std::atomic<quint64> down_bytes_;
	std::atomic<quint64> down_bytes_blocks_;

	std::atomic<quint64> down_bytes_last_;
	std::atomic<quint64> down_bytes_blocks_last_;

	// Upload
	std::atomic<quint64> up_bytes_;
	std::atomic<quint64> up_bytes_blocks_;

	std::atomic<quint64> up_bytes_last_;
	std::atomic<quint64> up_bytes_blocks_last_;
};

} /* namespace librevault */

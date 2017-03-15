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
#include <discovery/DiscoveryInstance.h>
#include "../btcompat.h"
#include <util/log_scope.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>

namespace librevault {

class MLDHTDiscovery;
class PortMappingService;

class MLDHTSearcher : public DiscoveryInstance {
	LOG_SCOPE("MLDHTSearcher");
public:
	MLDHTSearcher(std::weak_ptr<FolderGroup> group, MLDHTDiscovery& service, PortMappingService& port_mapping, io_service& io_service);

	void set_enabled(bool enable);
	void start_search(int af);
	void search_completed(bool start_v4, bool start_v6);

private:
	PortMappingService& port_mapping_;

	btcompat::info_hash info_hash_;
	boost::signals2::scoped_connection attached_connection_;

	bool enabled_ = false;


	boost::asio::steady_timer search_timer6_, search_timer4_;
};

} /* namespace librevault */

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
#include "ControlServer.h"
#include "Client.h"
#include "control/Config.h"
#include "discovery/DiscoveryService.h"
#include "discovery/mldht/MLDHTDiscovery.h"
#include "folder/FolderGroup.h"
#include "folder/FolderService.h"
#include "folder/meta/Index.h"
#include "folder/meta/MetaStorage.h"
#include "p2p/P2PFolder.h"
#include "util/log.h"

namespace librevault {

ControlServer::ControlServer(Client& client) :
		client_(client), ios_("ControlServer"), heartbeat_process_(ios_.ios(), std::bind(&ControlServer::send_heartbeat, this, std::placeholders::_1)) {
	url bind_url = parse_url(Config::get()->globals()["control_listen"].asString());
	local_endpoint_ = tcp_endpoint(address::from_string(bind_url.host), bind_url.port);

	/* WebSockets server initialization */
	// General parameters
	ws_server_.init_asio(&ios_.ios());
	ws_server_.set_reuse_addr(true);
	ws_server_.set_user_agent(Version::current().user_agent());

	// Handlers
	ws_server_.set_validate_handler(std::bind(&ControlServer::on_validate, this, std::placeholders::_1));
	ws_server_.set_open_handler(std::bind(&ControlServer::on_open, this, std::placeholders::_1));
	ws_server_.set_message_handler(std::bind(&ControlServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
	ws_server_.set_fail_handler(std::bind(&ControlServer::on_disconnect, this, std::placeholders::_1));
	ws_server_.set_close_handler(std::bind(&ControlServer::on_disconnect, this, std::placeholders::_1));

	ws_server_.listen(local_endpoint_);
	ws_server_.start_accept();

	// This string is for control client, that launches librevault daemon as its child process.
	// It watches STDOUT for ^\[CONTROL\].*?(wss?:\/\/.*)$ regexp and connects to the first matched address.
	// So, change it carefully, preserving the compatibility.
	std::cout << "[CONTROL] Librevault Control is listening at ws://" << (std::string)bind_url << std::endl;

	heartbeat_process_.invoke_post();
}

ControlServer::~ControlServer() {
	LOGFUNC();
	heartbeat_process_.wait();
	ws_server_assignment_.clear();
	ws_server_.stop_listening();
	ios_.stop();
	LOGFUNCEND();
}

bool ControlServer::on_validate(websocketpp::connection_hdl hdl) {
	LOGFUNC();

	auto connection_ptr = ws_server_.get_con_from_hdl(hdl);
	auto subprotocols = connection_ptr->get_requested_subprotocols();
	auto origin = connection_ptr->get_origin();

	LOGD("Incoming connection from " << connection_ptr->get_remote_endpoint() << " Origin: " << origin);

	// Restrict access by "Origin" header
	if(!origin.empty()) {   // TODO: Add a way to relax this restriction
		url origin_url(origin);
		if(origin_url.host != "127.0.0.1" && origin_url.host != "::1" && origin_url.host != "localhost")
			return false;
	}

	// Detect loopback
	if(std::find(subprotocols.begin(), subprotocols.end(), "librevaultctl1.0") != subprotocols.end())
		connection_ptr->select_subprotocol("librevaultctl1.0");
	return true;
}
//
void ControlServer::on_open(websocketpp::connection_hdl hdl) {
	LOGFUNC();

	auto connection_ptr = ws_server_.get_con_from_hdl(hdl);
	ws_server_assignment_.insert(connection_ptr);

	heartbeat_process_.invoke_post();
}

void ControlServer::on_message(websocketpp::connection_hdl hdl, server::message_ptr message_ptr) {
	LOGFUNC();

	try {
		LOGT(message_ptr->get_payload());
		// Read as control_json;
		Json::Value control_json;
		Json::Reader r; r.parse(message_ptr->get_payload(), control_json);

		dispatch_control_json(control_json);
	}catch(std::exception& e) {
		LOGT("on_message e:" << e.what());
		ws_server_.get_con_from_hdl(hdl)->close(websocketpp::close::status::protocol_error, e.what());
	}
}

void ControlServer::on_disconnect(websocketpp::connection_hdl hdl) {
	LOGFUNC();

	ws_server_assignment_.erase(ws_server_.get_con_from_hdl(hdl));
	heartbeat_process_.invoke_post();
}

std::string ControlServer::make_control_json() {
	Json::Value control_json;

	// ID
	static int control_json_id = 0; // Client-wide message id
	control_json["id"] = control_json_id++;
	control_json["globals"] = Config::get()->globals();
	control_json["folders"] = Config::get()->folders();
	control_json["state"] = make_state_json();

	// result serialization
	std::ostringstream os; os << control_json;
	return os.str();
}

Json::Value ControlServer::make_state_json() const {
	Json::Value state_json;                         // state_json
	for(auto folder : client_.folder_service_->groups()) {
		Json::Value folder_json;                    /// folder_json

		folder_json["path"] = folder->params().path.string();
		folder_json["secret"] = folder->secret().string();

		// Indexer
		folder_json["is_indexing"] = folder->meta_storage_->is_indexing();

		// Index
		auto index_status = folder->meta_storage_->index->get_status();
		folder_json["file_entries"] = (Json::Value::UInt64)index_status.file_entries;
		folder_json["directory_entries"] = (Json::Value::UInt64)index_status.directory_entries;
		folder_json["symlink_entries"] = (Json::Value::UInt64)index_status.symlink_entries;
		folder_json["deleted_entries"] = (Json::Value::UInt64)index_status.deleted_entries;

		// Peers
		folder_json["peers"] = Json::arrayValue;
		for(auto p2p_peer : folder->p2p_dirs()) {
			Json::Value peer_json;                  //// peer_json

			std::ostringstream os; os << p2p_peer->remote_endpoint();
			peer_json["endpoint"] = os.str();
			peer_json["client_name"] = p2p_peer->client_name();
			peer_json["user_agent"] = p2p_peer->user_agent();

			// Bandwidth
			auto bandwidth_stats = p2p_peer->heartbeat_stats();
			peer_json["up_bandwidth"] = bandwidth_stats.up_bandwidth_;
			peer_json["up_bandwidth_blocks"] = bandwidth_stats.up_bandwidth_;
			peer_json["down_bandwidth"] = bandwidth_stats.down_bandwidth_;
			peer_json["down_bandwidth_blocks"] = bandwidth_stats.down_bandwidth_;
			// Transferred
			peer_json["up_bytes"] = (Json::Value::UInt64)bandwidth_stats.up_bytes_;
			peer_json["up_bytes_blocks"] = (Json::Value::UInt64)bandwidth_stats.up_bytes_blocks_;
			peer_json["down_bytes"] = (Json::Value::UInt64)bandwidth_stats.down_bytes_;
			peer_json["down_bytes_blocks"] = (Json::Value::UInt64)bandwidth_stats.down_bytes_blocks_;

			folder_json["peers"].append(peer_json); //// /peer_json
		}

		state_json["folders"].append(folder_json);  /// /folder_json
	}

	state_json["dht_nodes_count"] = client_.discovery_->mldht_->node_count();

	return state_json;                              // /state_json
}

void ControlServer::send_heartbeat(PeriodicProcess& process) {
	//log_->trace() << log_tag() << BOOST_CURRENT_FUNCTION;
	auto control_json = make_control_json();

	for(auto conn_assignment : ws_server_assignment_)
		conn_assignment->send(control_json);
	process.invoke_after(std::chrono::seconds(1));
}

void ControlServer::dispatch_control_json(const Json::Value& control_json) {
	std::string command = control_json["command"].asString();
	if(command == "set_config")
		Config::get()->set_globals(control_json["globals"]);
	else if(command == "add_folder")
		handle_add_folder_json(control_json);
	else if(command == "remove_folder")
		handle_remove_folder_json(control_json);
	else
		LOGD("Could not handle control JSON: Unknown command: " << command);
}

void ControlServer::handle_add_folder_json(const Json::Value& control_json) {
	add_folder_signal(control_json["folder"]);
	heartbeat_process_.invoke_post();
}

void ControlServer::handle_remove_folder_json(const Json::Value& control_json) {
	Secret secret = Secret(control_json["secret"].asString());
	remove_folder_signal(secret);
	heartbeat_process_.invoke_post();
}

} /* namespace librevault */
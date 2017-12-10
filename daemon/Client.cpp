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
#include "Client.h"
#include "control/Config.h"
#include "adapters/DiscoveryAdapter.h"
#include "folder/FolderGroup.h"
#include "PortMapper.h"
#include "nodekey/NodeKey.h"
#include "p2p/PeerServer.h"
#include "p2p/PeerPool.h"

namespace librevault {

Client::Client(int argc, char** argv) : QCoreApplication(argc, argv) {
	setApplicationName("Librevault");
	setOrganizationDomain("librevault.com");

	// Initializing components
	node_key_ = new NodeKey(this);
	portmanager_ = new PortMapper(this);
	discovery_ = new DiscoveryAdapter(portmanager_, this);
	peerserver_ = new PeerServer(node_key_, portmanager_, this);

	/* Connecting signals */
	connect(Config::get(), &Config::folderAdded, this, &Client::initFolder);
	connect(Config::get(), &Config::folderRemoved, this, &Client::deinitFolder);

	QTimer::singleShot(0, this, [this] {
			foreach(QByteArray folderid, Config::get()->listFolders()) {
				initFolder(Config::get()->getFolder(folderid));
			}
	});
}

Client::~Client() {
	delete peerserver_;
	delete discovery_;
	delete portmanager_;
	delete node_key_;
}

int Client::run() {
	return this->exec();
}

void Client::restart() {
	qInfo() << "Restarting...";
	this->exit(EXIT_RESTART);
}

void Client::shutdown(){
	qInfo() << "Exiting...";
	this->exit();
}

void Client::initFolder(const FolderParams& params) {
	auto peer_pool = new PeerPool(params, discovery_, node_key_, &bc_all_, &bc_blocks_, this);
	auto fgroup = new FolderGroup(params, peer_pool, this);
	groups_[params.folderid()] = fgroup;

	peerserver_->addPeerPool(params.folderid(), peer_pool);

	qInfo() << "Folder initialized: " << params.folderid().toHex();
}

void Client::deinitFolder(const QByteArray& folderid) {
	auto fgroup = groups_[folderid];
	groups_.remove(folderid);

	fgroup->deleteLater();
	qInfo() << "Folder deinitialized:" << folderid.toHex();
}

} /* namespace librevault */

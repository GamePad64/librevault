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
#include "PeerPool.h"
#include "p2p/PeerServer.h"
#include "p2p/Peer.h"
//#include <DiscoveryGroup.h>

namespace librevault {

PeerPool::PeerPool(const FolderParams& params, DiscoveryAdapter* discovery, NodeKey* node_key, BandwidthCounter* bc_all, BandwidthCounter* bc_blocks, QObject* parent) :
	QObject(parent),
	params_(params),
	node_key_(node_key),
	bc_all_(bc_all),
	bc_blocks_(bc_blocks) {
//	dgroup_ = discovery->createGroup(params.folderid());
//	connect(dgroup_, &DiscoveryGroup::discovered, this, qOverload<QHostAddress, quint16>(&PeerPool::handleDiscovered));
}

PeerPool::~PeerPool() {}

bool PeerPool::contains(Peer* peer) const {
	return peers_.contains(peer) || digests_.contains(peer->digest()) || endpoints_.contains(peer->endpoint());
}

void PeerPool::handleHandshake(Peer* peer) {
	peers_ready_.insert(peer);
	emit newValidPeer(peer);
}

void PeerPool::handleDiscovered(QPair<QHostAddress, quint16> endpoint) {
	if(endpoints_.contains(endpoint))
		return;

	Peer* folder = new Peer(params_, node_key_, &bc_all_, &bc_blocks_, this);

	QUrl ws_url = Peer::makeUrl(endpoint, params_.folderid());
	folder->open(ws_url);
}

void PeerPool::handleIncoming(Peer* peer) {
	Q_ASSUME(! peers_.contains(peer));

	if(contains(peer)) {
		peer->deleteLater();
		return;
	}

	peer->setParent(this);

	peers_.insert(peer);
	endpoints_.insert(peer->endpoint());
	digests_.insert(peer->digest());

	connect(peer, &Peer::handshakeSuccess, this, [=]{handleHandshake(peer);});
	connect(peer, &Peer::handshakeFailed, this, [=]{handleDisconnected(peer);});
}

void PeerPool::handleDisconnected(Peer* peer) {
	peers_.remove(peer);
	peers_ready_.remove(peer);

	endpoints_.remove(peer->endpoint());
	digests_.remove(peer->digest());
}

} /* namespace librevault */

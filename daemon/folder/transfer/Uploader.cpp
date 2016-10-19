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
#include "Uploader.h"

#include "folder/chunk/ChunkStorage.h"
#include "folder/RemoteFolder.h"

#include "util/log.h"

namespace librevault {

Uploader::Uploader(ChunkStorage& chunk_storage) :
	chunk_storage_(chunk_storage) {
	LOGFUNC();
}

void Uploader::broadcast_chunk(std::set<std::shared_ptr<RemoteFolder>> remotes, const blob& ct_hash) {
	for(auto& remote : remotes) {
		remote->post_have_chunk(ct_hash);
	}
}

void Uploader::handle_interested(std::shared_ptr<RemoteFolder> remote) {
	LOGFUNC();

	// TODO: write good choking algorithm.
	remote->unchoke();
}
void Uploader::handle_not_interested(std::shared_ptr<RemoteFolder> remote) {
	LOGFUNC();

	// TODO: write good choking algorithm.
	remote->choke();
}

void Uploader::handle_block_request(std::shared_ptr<RemoteFolder> origin, const blob& ct_hash, uint32_t offset, uint32_t size) {
	try {
		if(!origin->am_choking() && origin->peer_interested()) {
			origin->post_block(ct_hash, offset, get_block(ct_hash, offset, size));
		}
	}catch(AbstractFolder::no_such_chunk& e){
		LOGW("Requested nonexistent block");
	}
}

blob Uploader::get_block(const blob& ct_hash, uint32_t offset, uint32_t size) {
	auto chunk = chunk_storage_.get_chunk(ct_hash);
	if(offset < chunk.size() && size <= chunk.size()-offset)
		return blob(chunk.begin()+offset, chunk.begin()+offset+size);
	else
		throw AbstractFolder::no_such_chunk();
}

} /* namespace librevault */

/* Copyright (C) 2014-2015 Alexander Shishenko <GamePad64@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "EncStorage.h"
#include "FSDirectory.h"

namespace librevault {

EncStorage::EncStorage(FSDirectory& dir, Session& session) : log_(spdlog::get("Librevault")), dir_(dir), block_path_(dir.block_path()) {
	bool block_path_created = fs::create_directories(block_path_);
	log_->debug() << dir_.log_tag() << "Block directory: " << block_path_ << (block_path_created ? " created" : "");
}
EncStorage::~EncStorage() {}

fs::path EncStorage::make_encblock_name(const blob& encrypted_data_hash) const {
	return crypto::Base32().to_string(encrypted_data_hash);
}

fs::path EncStorage::make_encblock_path(const blob& encrypted_data_hash) const {
	return block_path_ / make_encblock_name(encrypted_data_hash);
}

bool EncStorage::have_encblock(const blob& encrypted_data_hash){
	return fs::exists(make_encblock_path(encrypted_data_hash));
}

blob EncStorage::get_encblock(const blob& encrypted_data_hash){
	if(have_encblock(encrypted_data_hash)){
		auto block_path = make_encblock_path(encrypted_data_hash);
		auto blocksize = fs::file_size(block_path);
		blob return_value(blocksize);

		fs::ifstream block_fstream(block_path, std::ios_base::in | std::ios_base::binary);
		block_fstream.read(reinterpret_cast<char*>(return_value.data()), blocksize);

		return return_value;
	}
	throw no_such_block();
}

void EncStorage::put_encblock(const blob& encrypted_data_hash, const blob& data){
	auto block_path = make_encblock_path(encrypted_data_hash);
	fs::ofstream block_fstream(block_path, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
	block_fstream.write(reinterpret_cast<const char*>(data.data()), data.size());

	log_->debug() << dir_.log_tag() << "Encrypted block " << make_encblock_name(encrypted_data_hash) << " pushed into EncStorage";
}

void EncStorage::remove_encblock(const blob& encrypted_data_hash){
	fs::remove(make_encblock_path(encrypted_data_hash));

	log_->debug() << dir_.log_tag() << "Block " << make_encblock_name(encrypted_data_hash) << " removed from EncStorage";
}

} /* namespace librevault */
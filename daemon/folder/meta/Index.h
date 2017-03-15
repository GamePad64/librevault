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
#include "blob.h"
#include "util/log.h"
#include "util/SQLiteWrapper.h"
#include <librevault/SignedMeta.h>
#include <QObject>

namespace librevault {

class FolderParams;
class StateCollector;

class Index : public QObject {
	Q_OBJECT
	LOG_SCOPE("Index");
signals:
	void metaAdded(SignedMeta meta);
	void metaAddedExternal(SignedMeta meta);

public:
	Index(const FolderParams& params, StateCollector* state_collector, QObject* parent);

	/* Meta manipulators */
	bool haveMeta(const Meta::PathRevision& path_revision) noexcept;
	SignedMeta getMeta(const Meta::PathRevision& path_revision);
	SignedMeta getMeta(const blob& path_id);
	QList<SignedMeta> getMeta();
	QList<SignedMeta> getExistingMeta();
	QList<SignedMeta> getIncompleteMeta();
	void putMeta(const SignedMeta& signed_meta, bool fully_assembled = false);

	bool putAllowed(const Meta::PathRevision& path_revision) noexcept;

	void setAssembled(blob path_id);
	bool isAssembledChunk(blob ct_hash);
	QPair<quint32, QByteArray> getChunkSizeIv(blob ct_hash);

	/* Properties */
	QList<SignedMeta> containingChunk(const blob& ct_hash);

private:
	const FolderParams& params_;
	StateCollector* state_collector_;

	std::unique_ptr<SQLiteDB> db_;	// Better use SOCI library ( https://github.com/SOCI/soci ). My "reinvented wheel" isn't stable enough.

	QList<SignedMeta> getMeta(const std::string& sql, const std::map<std::string, SQLValue>& values = std::map<std::string, SQLValue>());
	void wipe();

	void notifyState();
};

} /* namespace librevault */

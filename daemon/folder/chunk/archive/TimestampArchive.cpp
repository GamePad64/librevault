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
#include "TimestampArchive.h"
#include "control/FolderParams.h"
#include "folder/PathNormalizer.h"
#include "util/conv_fspath.h"
#include "util/regex_escape.h"
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <boost/filesystem.hpp>
#include <regex>

namespace librevault {

TimestampArchive::TimestampArchive(const FolderParams& params, PathNormalizer* path_normalizer, QObject* parent) :
	ArchiveStrategy(parent),
	params_(params),
	path_normalizer_(path_normalizer),
	archive_path_(params_.system_path + "archive") {

	QDir().mkpath(archive_path_);
}

void TimestampArchive::archive(QString denormpath) {
	boost::filesystem::path denormpath_fs = conv_fspath(denormpath);
	// Add a new entry
	QString archived_path = archive_path_ + "/" + path_normalizer_->normalizePath(denormpath);

	qint64 mtime = boost::filesystem::last_write_time(denormpath_fs);
	QString suffix = "~" + QDateTime::fromMSecsSinceEpoch(mtime*1000).toString("yyyyMMdd-HHmmss");

	QFileInfo archived_info(archived_path);
	QString timestamped_path = archived_info.baseName();
	timestamped_path += suffix;
	timestamped_path += archived_info.completeSuffix();
	QFile::rename(denormpath, timestamped_path);

	// Remove
	std::map<std::string, boost::filesystem::path> paths;
	std::regex timestamp_regex(regex_escape(archived_info.baseName().toStdString()) + R"((~\d{8}-\d{6}))" + regex_escape(archived_info.completeSuffix().toStdString()));
	for(auto it = boost::filesystem::directory_iterator(denormpath_fs.parent_path()); it != boost::filesystem::directory_iterator(); it++) {
		std::smatch match;
		std::string generic_path = it->path().generic_string();	// To resolve stackoverflow.com/q/32164501
		std::regex_match(generic_path, match, timestamp_regex);
		if(!match.empty()) {
			paths.insert({match[1].str(), denormpath_fs});
		}
	}
	if(paths.size() > params_.archive_timestamp_count && params_.archive_timestamp_count != 0) {
		boost::filesystem::remove(paths.begin()->second);
	}
}

} /* namespace librevault */

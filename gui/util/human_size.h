/* Copyright (C) 2015-2016 Alexander Shishenko <GamePad64@gmail.com>
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
 */
#pragma once

#include <QtCore>

inline QString human_size(uintmax_t size) {
	float num = size;

	if(num < 1024.0)
		return QCoreApplication::translate("Human Size", "%n bytes", 0, size);
	num /= 1024.0;

	if(num < 1024.0)
		return QCoreApplication::translate("Human Size", "%1 KiB").arg(num, 0, 'f', 0);
	num /= 1024.0;

	if(num < 1024.0)
		return QCoreApplication::translate("Human Size", "%1 MiB").arg(num, 0, 'f', 2);
	num /= 1024.0;

	if(num < 1024.0)
		return QCoreApplication::translate("Human Size", "%1 GiB").arg(num, 0, 'f', 2);
	num /= 1024.0;

	return QCoreApplication::translate("Human Size", "%1 TiB").arg(num, 0, 'f', 2);
}

inline QString human_bandwidth(float bandwidth) {
	if(bandwidth < 1024.0)
		return QCoreApplication::translate("Human Bandwidth", "%1 B/s").arg(bandwidth, 0, 'f', 0);
	bandwidth /= 1024.0;

	if(bandwidth < 1024.0)
		return QCoreApplication::translate("Human Bandwidth", "%1 KiB/s").arg(bandwidth, 0, 'f', 1);
	bandwidth /= 1024.0;

	if(bandwidth < 1024.0)
		return QCoreApplication::translate("Human Bandwidth", "%1 MiB/s").arg(bandwidth, 0, 'f', 1);
	bandwidth /= 1024.0;

	if(bandwidth < 1024.0)
		return QCoreApplication::translate("Human Bandwidth", "%1 GiB/s").arg(bandwidth, 0, 'f', 1);
	bandwidth /= 1024.0;

	return QCoreApplication::translate("Human Bandwidth", "%1 TiB/s").arg(bandwidth, 0, 'f', 1);
}

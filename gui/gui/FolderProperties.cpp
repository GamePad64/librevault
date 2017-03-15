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
#include "FolderProperties.h"
#include <QShowEvent>
#include <QFileIconProvider>
#include <QJsonArray>
#include <QClipboard>
#include <util/human_size.h>

#include "../model/PeerModel.h"

FolderProperties::FolderProperties(const librevault::Secret& secret, QWidget* parent) :
		QDialog(parent) {

	ui.setupUi(this);

	//peer_model_ = std::make_unique<PeerModel>(this);
	ui.peers_treeView->setModel(peer_model_.get());

#ifdef Q_OS_MAC
	ui.tabWidget->setDocumentMode(false);
#endif

	setSecret(secret);

	connect(ui.copy_rw, &QAbstractButton::clicked, [this](){QApplication::clipboard()->setText(ui.secret_rw->text());});
	connect(ui.copy_ro, &QAbstractButton::clicked, [this](){QApplication::clipboard()->setText(ui.secret_ro->text());});
	connect(ui.copy_do, &QAbstractButton::clicked, [this](){QApplication::clipboard()->setText(ui.secret_do->text());});

	this->setWindowFlags(Qt::Tool);
	ui.folder_icon->setPixmap(QFileIconProvider().icon(QFileIconProvider::Folder).pixmap(QSize(32, 32)));
	setAttribute(Qt::WA_MacAlwaysShowToolWindow, true);
}

FolderProperties::~FolderProperties() {}

void FolderProperties::setSecret(const librevault::Secret& secret) {
	hash_.setRawData((const char*)secret.get_Hash().data(), secret.get_Hash().size());

	if(secret.get_type() <= secret.ReadWrite)
		ui.secret_rw->setText(QString::fromStdString(secret.string()));
	else {
		ui.label_rw->setVisible(false);
		ui.secret_rw->setVisible(false);
		ui.copy_rw->setVisible(false);
	}

	if(secret.get_type() <= secret.ReadOnly)
		ui.secret_ro->setText(QString::fromStdString(secret.derive(secret.ReadOnly).string()));
	else {
		ui.label_ro->setVisible(false);
		ui.secret_ro->setVisible(false);
		ui.copy_ro->setVisible(false);
	}

	ui.secret_do->setText(QString::fromStdString(secret.derive(secret.Download).string()));
}

void FolderProperties::update(const QJsonObject& control_json, const QJsonObject& folder_config_json, const QJsonObject& folder_state_json) {
	ui.folder_name->setText(folder_state_json["path"].toString());
	ui.folder_icon->setPixmap(QFileIconProvider().icon(QFileIconProvider::Folder).pixmap(32, 32));

	ui.folder_size->setText(tr("%n file(s)", "", folder_state_json["file_entries"].toInt()) + " " + tr("%n directory(s)", "", folder_state_json["directory_entries"].toInt()));
	ui.connected_counter->setText(tr("%n connected", "", folder_state_json["peers"].toArray().size()));

	peer_model_->refresh();
}

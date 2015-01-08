/* Copyright 2013-2014 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <QFile>
#include <QDir>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonArray>
#include <pathutils.h>

#include "logic/minecraft/MinecraftProfile.h"
#include "logic/minecraft/VersionBuilder.h"
#include "logic/OneSixInstance.h"

MinecraftProfile::MinecraftProfile(OneSixInstance *instance, QObject *parent)
	: QAbstractListModel(parent), m_instance(instance)
{
	clear();
}

void MinecraftProfile::reload()
{
	beginResetModel();
	VersionBuilder::build(this, m_instance);
	reapply();
	endResetModel();
}

void MinecraftProfile::clear()
{
	id.clear();
	m_updateTimeString.clear();
	m_updateTime = QDateTime();
	m_releaseTimeString.clear();
	m_releaseTime = QDateTime();
	type.clear();
	assets.clear();
	processArguments.clear();
	minecraftArguments.clear();
	minimumLauncherVersion = 0xDEADBEAF;
	mainClass.clear();
	appletClass.clear();
	libraries.clear();
	tweakers.clear();
	jarMods.clear();
	traits.clear();
}

bool MinecraftProfile::canRemove(const int index) const
{
	return VersionPatches.at(index)->isMoveable();
}

bool MinecraftProfile::preremove(ProfilePatchPtr patch)
{
	bool ok = true;
	for(auto & jarmod: patch->getJarMods())
	{
		QString fullpath =PathCombine(m_instance->jarModsDir(), jarmod->name);
		QFileInfo finfo (fullpath);
		if(finfo.exists())
			ok &= QFile::remove(fullpath);
	}
	return ok;
}

bool MinecraftProfile::remove(const int index)
{
	if (!canRemove(index))
		return false;
	if(!preremove(VersionPatches[index]))
	{
		return false;
	}
	auto toDelete = VersionPatches.at(index)->getPatchFilename();
	if(!QFile::remove(toDelete))
		return false;
	beginRemoveRows(QModelIndex(), index, index);
	VersionPatches.removeAt(index);
	endRemoveRows();
	reapply();
	saveCurrentOrder();
	return true;
}

bool MinecraftProfile::remove(const QString id)
{
	int i = 0;
	for (auto patch : VersionPatches)
	{
		if (patch->getPatchID() == id)
		{
			return remove(i);
		}
		i++;
	}
	return false;
}

QString MinecraftProfile::versionFileId(const int index) const
{
	if (index < 0 || index >= VersionPatches.size())
	{
		return QString();
	}
	return VersionPatches.at(index)->getPatchID();
}

ProfilePatchPtr MinecraftProfile::versionPatch(const QString &id)
{
	for (auto file : VersionPatches)
	{
		if (file->getPatchID() == id)
		{
			return file;
		}
	}
	return 0;
}

ProfilePatchPtr MinecraftProfile::versionPatch(int index)
{
	if(index < 0 || index >= VersionPatches.size())
		return 0;
	return VersionPatches[index];
}

bool MinecraftProfile::isVanilla()
{
	for(auto patchptr: VersionPatches)
	{
		if(patchptr->isCustom())
			return false;
	}
	return true;
}

bool MinecraftProfile::revertToVanilla()
{
	beginResetModel();
	// remove patches, if present
	auto it = VersionPatches.begin();
	while (it != VersionPatches.end())
	{
		if ((*it)->isMoveable())
		{
			if(!preremove(*it))
			{
				endResetModel();
				saveCurrentOrder();
				return false;
			}
			if(!QFile::remove((*it)->getPatchFilename()))
			{
				endResetModel();
				saveCurrentOrder();
				return false;
			}
			it = VersionPatches.erase(it);
		}
		else
			it++;
	}
	reapply();
	endResetModel();
	saveCurrentOrder();
	return true;
}

QList<std::shared_ptr<OneSixLibrary> > MinecraftProfile::getActiveNormalLibs()
{
	QList<std::shared_ptr<OneSixLibrary> > output;
	for (auto lib : libraries)
	{
		if (lib->isActive() && !lib->isNative())
		{
			for (auto other : output)
			{
				if (other->rawName() == lib->rawName())
				{
					QLOG_WARN() << "Multiple libraries with name" << lib->rawName() << "in library list!";
					continue;
				}
			}
			output.append(lib);
		}
	}
	return output;
}
QList<std::shared_ptr<OneSixLibrary> > MinecraftProfile::getActiveNativeLibs()
{
	QList<std::shared_ptr<OneSixLibrary> > output;
	for (auto lib : libraries)
	{
		if (lib->isActive() && lib->isNative())
		{
			output.append(lib);
		}
	}
	return output;
}

std::shared_ptr<MinecraftProfile> MinecraftProfile::fromJson(const QJsonObject &obj)
{
	std::shared_ptr<MinecraftProfile> version(new MinecraftProfile(0));
	try
	{
		VersionBuilder::readJsonAndApplyToVersion(version.get(), obj);
	}
	catch(MMCError & err)
	{
		return 0;
	}
	return version;
}

QVariant MinecraftProfile::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	int row = index.row();
	int column = index.column();

	if (row < 0 || row >= VersionPatches.size())
		return QVariant();

	if (role == Qt::DisplayRole)
	{
		switch (column)
		{
		case 0:
			return VersionPatches.at(row)->getPatchName();
		case 1:
			return VersionPatches.at(row)->getPatchVersion();
		default:
			return QVariant();
		}
	}
	return QVariant();
}
QVariant MinecraftProfile::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (section)
			{
			case 0:
				return tr("Name");
			case 1:
				return tr("Version");
			default:
				return QVariant();
			}
		}
	}
	return QVariant();
}
Qt::ItemFlags MinecraftProfile::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

int MinecraftProfile::rowCount(const QModelIndex &parent) const
{
	return VersionPatches.size();
}

int MinecraftProfile::columnCount(const QModelIndex &parent) const
{
	return 2;
}

void MinecraftProfile::saveCurrentOrder() const
{
	PatchOrder order;
	for(auto item: VersionPatches)
	{
		if(!item->isMoveable())
			continue;
		order.append(item->getPatchID());
	}
	VersionBuilder::writeOverrideOrders(m_instance, order);
}

void MinecraftProfile::move(const int index, const MoveDirection direction)
{
	int theirIndex;
	if (direction == MoveUp)
	{
		theirIndex = index - 1;
	}
	else
	{
		theirIndex = index + 1;
	}

	if (index < 0 || index >= VersionPatches.size())
		return;
	if (theirIndex >= rowCount())
		theirIndex = rowCount() - 1;
	if (theirIndex == -1)
		theirIndex = rowCount() - 1;
	if (index == theirIndex)
		return;
	int togap = theirIndex > index ? theirIndex + 1 : theirIndex;

	auto from = versionPatch(index);
	auto to = versionPatch(theirIndex);

	if (!from || !to || !to->isMoveable() || !from->isMoveable())
	{
		return;
	}
	beginMoveRows(QModelIndex(), index, index, QModelIndex(), togap);
	VersionPatches.swap(index, theirIndex);
	endMoveRows();
	saveCurrentOrder();
	reapply();
}
void MinecraftProfile::resetOrder()
{
	QDir(m_instance->instanceRoot()).remove("order.json");
	reload();
}

void MinecraftProfile::reapply()
{
	clear();
	for(auto file: VersionPatches)
	{
		file->applyTo(this);
	}
	finalize();
}

void MinecraftProfile::finalize()
{
	// HACK: deny april fools. my head hurts enough already.
	QDate now = QDate::currentDate();
	bool isAprilFools = now.month() == 4 && now.day() == 1;
	if (assets.endsWith("_af") && !isAprilFools)
	{
		assets = assets.left(assets.length() - 3);
	}
	if (assets.isEmpty())
	{
		assets = "legacy";
	}
	auto finalizeArguments = [&]( QString & minecraftArguments, const QString & processArguments ) -> void
	{
		if (!minecraftArguments.isEmpty())
			return;
		QString toCompare = processArguments.toLower();
		if (toCompare == "legacy")
		{
			minecraftArguments = " ${auth_player_name} ${auth_session}";
		}
		else if (toCompare == "username_session")
		{
			minecraftArguments = "--username ${auth_player_name} --session ${auth_session}";
		}
		else if (toCompare == "username_session_version")
		{
			minecraftArguments = "--username ${auth_player_name} "
								"--session ${auth_session} "
								"--version ${profile_name}";
		}
	};
	finalizeArguments(vanillaMinecraftArguments, vanillaProcessArguments);
	finalizeArguments(minecraftArguments, processArguments);
}

void MinecraftProfile::installJarMods(QStringList selectedFiles)
{
	for(auto filename: selectedFiles)
	{
		installJarModByFilename(filename);
	}
}

void MinecraftProfile::installJarModByFilename(QString filepath)
{
	QString patchDir = PathCombine(m_instance->instanceRoot(), "patches");
	if(!ensureFolderPathExists(patchDir))
	{
		// THROW...
		return;
	}

	if (!ensureFolderPathExists(m_instance->jarModsDir()))
	{
		// THROW...
		return;
	}

	QFileInfo sourceInfo(filepath);
	auto uuid = QUuid::createUuid();
	QString id = uuid.toString().remove('{').remove('}');
	QString target_filename = id + ".jar";
	QString target_id = "org.multimc.jarmod." + id;
	QString target_name = sourceInfo.completeBaseName() + " (jar mod)";
	QString finalPath = PathCombine(m_instance->jarModsDir(), target_filename);

	QFileInfo targetInfo(finalPath);
	if(targetInfo.exists())
	{
		// THROW
		return;
	}

	if (!QFile::copy(sourceInfo.absoluteFilePath(),QFileInfo(finalPath).absoluteFilePath()))
	{
		// THROW
		return;
	}

	auto f = std::make_shared<VersionFile>();
	auto jarMod = std::make_shared<Jarmod>();
	jarMod->name = target_filename;
	f->jarMods.append(jarMod);
	f->name = target_name;
	f->fileId = target_id;
	f->order = getFreeOrderNumber();
	QString patchFileName = PathCombine(patchDir, target_id + ".json");
	f->filename = patchFileName;

	QFile file(patchFileName);
	if (!file.open(QFile::WriteOnly))
	{
		QLOG_ERROR() << "Error opening" << file.fileName()
					 << "for reading:" << file.errorString();
		return;
		// THROW
	}
	file.write(f->toJson(true).toJson());
	file.close();
	int index = VersionPatches.size();
	beginInsertRows(QModelIndex(), index, index);
	VersionPatches.append(f);
	endInsertRows();
	saveCurrentOrder();
}

int MinecraftProfile::getFreeOrderNumber()
{
	int largest = 100;
	// yes, I do realize this is dumb. The order thing itself is dumb. and to be removed next.
	for(auto thing: VersionPatches)
	{
		int order = thing->getOrder();
		if(order > largest)
			largest = order;
	}
	return largest + 1;
}

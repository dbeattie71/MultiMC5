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

#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QObject>
#include <QDir>
#include <qresource.h>
#include <modutils.h>
#include <pathutils.h>

#include "MultiMC.h"
#include "logic/minecraft/VersionBuilder.h"
#include "logic/minecraft/MinecraftProfile.h"
#include "logic/minecraft/OneSixRule.h"
#include "logic/minecraft/ProfilePatch.h"
#include "logic/minecraft/VersionFile.h"
#include "VersionBuildError.h"
#include "MinecraftVersionList.h"

#include "logic/OneSixInstance.h"
#include "logic/MMCJson.h"

#include "logger/QsLog.h"

VersionBuilder::VersionBuilder()
{
}

void VersionBuilder::build(MinecraftProfile *version, OneSixInstance *instance)
{
	VersionBuilder builder;
	builder.m_version = version;
	builder.m_instance = instance;
	builder.buildInternal();
}

void VersionBuilder::readJsonAndApplyToVersion(MinecraftProfile *version, const QJsonObject &obj)
{
	VersionBuilder builder;
	builder.m_version = version;
	builder.m_instance = 0;
	builder.readJsonAndApply(obj);
}

void VersionBuilder::readInstancePatches()
{
	PatchOrder userOrder;
	readOverrideOrders(m_instance, userOrder);
	QDir patches(PathCombine(m_instance->instanceRoot(),"patches"));

	// first, load things by sort order.
	for (auto id : userOrder)
	{
		// ignore builtins
		if (id == "net.minecraft")
			continue;
		if (id == "org.lwjgl")
			continue;
		// parse the file
		QString filename = patches.absoluteFilePath(id + ".json");
		QFileInfo finfo(filename);
		if(!finfo.exists())
		{
			QLOG_INFO() << "Patch file " << filename << " was deleted by external means...";
			continue;
		}
		QLOG_INFO() << "Reading" << filename << "by user order";
		auto file = parseJsonFile(finfo, false);
		// sanity check. prevent tampering with files.
		if (file->fileId != id)
		{
			throw VersionBuildError(
				QObject::tr("load id %1 does not match internal id %2").arg(id, file->fileId));
		}
		m_version->VersionPatches.append(file);
	}
	// now load the rest by internal preference.
	QMap<int, QPair<QString, VersionFilePtr>> files;
	for (auto info : patches.entryInfoList(QStringList() << "*.json", QDir::Files))
	{
		// parse the file
		QLOG_INFO() << "Reading" << info.fileName();
		auto file = parseJsonFile(info, true);
		// ignore builtins
		if (file->fileId == "net.minecraft")
			continue;
		if (file->fileId == "org.lwjgl")
			continue;
		// do not load what we already loaded in the first pass
		if (userOrder.contains(file->fileId))
			continue;
		if (files.contains(file->order))
		{
			// FIXME: do not throw?
			throw VersionBuildError(QObject::tr("%1 has the same order as %2")
										.arg(file->fileId, files[file->order].second->fileId));
		}
		files.insert(file->order, qMakePair(info.fileName(), file));
	}
	for (auto order : files.keys())
	{
		auto &filePair = files[order];
		m_version->VersionPatches.append(filePair.second);
	}
}

void VersionBuilder::buildInternal()
{
	m_version->VersionPatches.clear();

	// Minecraft - just the builtin stuff for now
	auto minecraftList = MMC->minecraftlist();
	auto mcversion = minecraftList->findVersion(m_instance->intendedVersionId());
	auto minecraftPatch = std::dynamic_pointer_cast<ProfilePatch>(mcversion);
	if (!minecraftPatch)
	{
		throw VersionIncomplete("net.minecraft");
	}
	minecraftPatch->setOrder(-2);
	m_version->VersionPatches.append(minecraftPatch);

	// TODO: this is obviously fake.
	QResource LWJGL(":/versions/LWJGL/2.9.1.json");
	auto lwjgl = parseJsonFile(LWJGL.absoluteFilePath(), false, false);
	auto lwjglPatch = std::dynamic_pointer_cast<ProfilePatch>(lwjgl);
	if (!lwjglPatch)
	{
		throw VersionIncomplete("org.lwjgl");
	}
	lwjglPatch->setOrder(-1);
	lwjgl->setVanilla(true);
	m_version->VersionPatches.append(lwjglPatch);

	// load all patches, put into map for ordering, apply in the right order
	readInstancePatches();

	m_version->finalize();
}

void VersionBuilder::readJsonAndApply(const QJsonObject &obj)
{
	m_version->clear();

	auto file = VersionFile::fromJson(QJsonDocument(obj), QString(), false);

	file->applyTo(m_version);
	m_version->VersionPatches.append(file);
}

VersionFilePtr VersionBuilder::parseJsonFile(const QFileInfo &fileInfo, const bool requireOrder,
											 bool isFTB)
{
	QFile file(fileInfo.absoluteFilePath());
	if (!file.open(QFile::ReadOnly))
	{
		throw JSONValidationError(QObject::tr("Unable to open the version file %1: %2.")
									  .arg(fileInfo.fileName(), file.errorString()));
	}
	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError)
	{
		throw JSONValidationError(
			QObject::tr("Unable to process the version file %1: %2 at %3.")
				.arg(fileInfo.fileName(), error.errorString())
				.arg(error.offset));
	}
	return VersionFile::fromJson(doc, file.fileName(), requireOrder, isFTB);
}

VersionFilePtr VersionBuilder::parseBinaryJsonFile(const QFileInfo &fileInfo)
{
	QFile file(fileInfo.absoluteFilePath());
	if (!file.open(QFile::ReadOnly))
	{
		throw JSONValidationError(QObject::tr("Unable to open the version file %1: %2.")
									  .arg(fileInfo.fileName(), file.errorString()));
	}
	QJsonDocument doc = QJsonDocument::fromBinaryData(file.readAll());
	file.close();
	if (doc.isNull())
	{
		file.remove();
		throw JSONValidationError(
			QObject::tr("Unable to process the version file %1.").arg(fileInfo.fileName()));
	}
	return VersionFile::fromJson(doc, file.fileName(), false, false);
}

static const int currentOrderFileVersion = 1;

bool VersionBuilder::readOverrideOrders(OneSixInstance *instance, PatchOrder &order)
{
	QFile orderFile(instance->instanceRoot() + "/order.json");
	if (!orderFile.exists())
	{
		QLOG_WARN() << "Order file doesn't exist. Ignoring.";
		return false;
	}
	if (!orderFile.open(QFile::ReadOnly))
	{
		QLOG_ERROR() << "Couldn't open" << orderFile.fileName()
					 << " for reading:" << orderFile.errorString();
		QLOG_WARN() << "Ignoring overriden order";
		return false;
	}

	// and it's valid JSON
	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(orderFile.readAll(), &error);
	if (error.error != QJsonParseError::NoError)
	{
		QLOG_ERROR() << "Couldn't parse" << orderFile.fileName() << ":" << error.errorString();
		QLOG_WARN() << "Ignoring overriden order";
		return false;
	}

	// and then read it and process it if all above is true.
	try
	{
		auto obj = MMCJson::ensureObject(doc);
		// check order file version.
		auto version = MMCJson::ensureInteger(obj.value("version"), "version");
		if (version != currentOrderFileVersion)
		{
			throw JSONValidationError(QObject::tr("Invalid order file version, expected %1")
										  .arg(currentOrderFileVersion));
		}
		auto orderArray = MMCJson::ensureArray(obj.value("order"));
		for(auto item: orderArray)
		{
			order.append(MMCJson::ensureString(item));
		}
	}
	catch (JSONValidationError &err)
	{
		QLOG_ERROR() << "Couldn't parse" << orderFile.fileName() << ": bad file format";
		QLOG_WARN() << "Ignoring overriden order";
		order.clear();
		return false;
	}
	return true;
}

bool VersionBuilder::writeOverrideOrders(OneSixInstance *instance, const PatchOrder &order)
{
	QJsonObject obj;
	obj.insert("version", currentOrderFileVersion);
	QJsonArray orderArray;
	for(auto str: order)
	{
		orderArray.append(str);
	}
	obj.insert("order", orderArray);
	QFile orderFile(instance->instanceRoot() + "/order.json");
	if (!orderFile.open(QFile::WriteOnly))
	{
		QLOG_ERROR() << "Couldn't open" << orderFile.fileName()
					 << "for writing:" << orderFile.errorString();
		return false;
	}
	orderFile.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return true;
}

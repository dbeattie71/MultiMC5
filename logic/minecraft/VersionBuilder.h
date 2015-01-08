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

#pragma once

#include <QString>
#include <QMap>
#include "VersionFile.h"

class MinecraftProfile;
class OneSixInstance;
class QJsonObject;
class QFileInfo;

typedef QStringList PatchOrder;

class VersionBuilder
{
	VersionBuilder();
public:
	static void build(MinecraftProfile *version, OneSixInstance *instance);
	static void readJsonAndApplyToVersion(MinecraftProfile *version, const QJsonObject &obj);
	static VersionFilePtr parseJsonFile(const QFileInfo &fileInfo, const bool requireOrder, bool isFTB = false);
	static VersionFilePtr parseBinaryJsonFile(const QFileInfo &fileInfo);

	bool readOverrideOrders(OneSixInstance *instance, PatchOrder &order);
	static bool writeOverrideOrders(OneSixInstance *instance, const PatchOrder &order);

private:
	MinecraftProfile *m_version;
	OneSixInstance *m_instance;

	void buildInternal();
	void buildFromExternalPatches();
	void buildFromCustomJson();
	void buildFromVersionJson();
	void buildFromMultilayer();

	void readInstancePatches();

	void readJsonAndApply(const QJsonObject &obj);
};

#include "SolderInstance.h"
#include "logic/minecraft/MinecraftProfile.h"
#include "logic/tasks/SequentialTask.h"
#include "logic/OneSixUpdate.h"
#include "SolderUpdate.h"
#include "SolderProfileStrategy.h"
#include <pathutils.h>

SolderInstance::SolderInstance(const QString &rootDir, SettingsObject *settings,
							   QObject *parent)
	: OneSixInstance(rootDir, settings, parent)
{
	settings->registerSetting("solderPack", "");
	settings->registerSetting("packStatus", "NotInstalled");
}

void SolderInstance::createProfile()
{
	m_version.reset(new MinecraftProfile(new SolderProfileStrategy(this)));
}


void SolderInstance::setSolderVersion(SolderVersionPtr url)
{
	m_solderVersion = url;
	QJsonDocument doc(url->toJson());
	settings().set("solderPack", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

QList<Mod> SolderInstance::getJarMods() const
{
	QList<Mod> mods;
	QFileInfo modpackJar = PathCombine(minecraftRoot(), "bin", "modpack.jar");
	if(modpackJar.exists())
	{
		mods.append(Mod(modpackJar));
	}
	return mods;
}

SolderVersionPtr SolderInstance::solderVersion()
{
	if(m_solderVersion)
		return m_solderVersion;
	QString packed = settings().get("solderPack").toString();
	auto doc = QJsonDocument::fromJson(packed.toUtf8());
	return SolderVersion::fromJson(doc.object());
}

std::shared_ptr<Task> SolderInstance::doUpdate()
{
	auto task = std::make_shared<SequentialTask>(this);
	task->addTask(std::make_shared<SolderUpdate>(this));
	task->addTask(std::make_shared<OneSixUpdate>(this));
	return task;
}

#include "SolderInstance.moc"

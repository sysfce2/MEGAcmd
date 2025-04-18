/**
 * @file src/configurationmanager.h
 * @brief MEGAcmd: configuration manager
 *
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGAcmd.
 *
 * MEGAcmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "megacmd.h"
#include <map>
#include <set>

#ifndef _WIN32
#include <sys/file.h> // LOCK_EX and LOCK_NB
#endif

#define CONFIGURATIONSTOREDBYVERSION -2
namespace megacmd {
class ConfigurationManager
{
private:
    inline static fs::path mConfigFolder;
    inline static bool hasBeenUpdated = false;
#ifdef WIN32
    static HANDLE mLockFileHandle;
#elif defined(LOCK_EX) && defined(LOCK_NB)
    static int fd;
#endif

    static void loadConfigDir();

    static void removeSyncConfig(sync_struct *syncToRemove);

#ifdef MEGACMD_TESTING_CODE
public:
#endif
    static void createFolderIfNotExisting(const fs::path &folder);

    static bool lockExecution(const fs::path &lockFileFolder);
    static bool unlockExecution(const fs::path &lockFileFolder);

public:
    static std::map<std::string, sync_struct *> oldConfiguredSyncs; //This will refer to old syncs from now on
    static std::map<std::string, backup_struct *> configuredBackups;

    static std::recursive_mutex settingsMutex;

    static std::string session;

    static void loadConfiguration(bool debug);
    static void clearConfigurationFile();
    static bool lockExecution();
    static bool unlockExecution();

    static void loadsyncs();
    static void loadbackups();

    static void saveSyncs(std::map<std::string, sync_struct *> *syncsmap);
    static void saveBackups(std::map<std::string, backup_struct *> *backupsmap);

    static void transitionLegacyExclusionRules(mega::MegaApi& api);

    static void saveSession(const char*session);

    static std::string /* prev value, if any */ saveProperty(const char* property, const char* value);

    template<typename T,
             typename Opt_T = std::optional<typename std::conditional_t<std::is_same_v<std::decay_t<T>, const char*>, std::string, T>>>
    static Opt_T savePropertyValue(const char* property, const T& value)
    {
        constexpr bool isStr = std::is_same_v<std::remove_cv_t<T>, std::string>;
        constexpr bool isConstChar = std::is_same_v<std::decay_t<T>, const char*>;

        std::string prevValueStr;
        if constexpr (isStr)
        {
            prevValueStr = saveProperty(property, value.c_str());
        }
        else if constexpr (isConstChar)
        {
            prevValueStr = saveProperty(property, value);
        }
        else
        {
            prevValueStr = saveProperty(property, std::to_string(value).c_str());
        }

        if (prevValueStr.empty())
        {
            return std::nullopt;
        }

        if constexpr (isStr || isConstChar)
        {
            return prevValueStr;
        }
        else
        {
            T prevValue;
            std::istringstream is(prevValueStr);
            is >> prevValue;
            return prevValue;
        }
    }

    template<typename T>
    static void savePropertyValueList(const char* property, std::list<T> value, char separator = ';')
    {
        std::ostringstream os;
        typename std::list<T>::iterator it = value.begin();
        for (; it != value.end(); ++it){
            os << (*it);

            if ( std::distance( it, value.end() ) != 1 ) // not last
            {
                os << separator;
            }
        }
        saveProperty(property,os.str().c_str());
    }

    template<typename T>
    static void savePropertyValueSet(const char* property, std::set<T> value, char separator = ';')
    {
        std::ostringstream os;
        typename std::set<T>::iterator it = value.begin();
        for (; it != value.end(); ++it){
            os << (*it);

            if ( std::distance( it, value.end() ) != 1 ) // not last
            {
                os << separator;
            }
        }
        saveProperty(property,os.str().c_str());
    }

    static std::string getConfigurationSValue(std::string propertyName);
    template <typename T>
    static T getConfigurationValue(std::string propertyName, T defaultValue)
    {
        std::string propValue = getConfigurationSValue(propertyName);
        if (!propValue.size()) return defaultValue;

        T i;
        std::istringstream is(propValue);
        is >> i;
        return i;
    }

    template <typename T>
    static std::list<T> getConfigurationValueList(std::string propertyName, char separator = ';')
    {
        std::list<T> toret;

        std::string propValue = getConfigurationSValue(propertyName);
        if (!propValue.size())
        {
            return toret;
        }
        size_t possep;
        do {
            possep = propValue.find(separator);

            std::string current = propValue.substr(0,possep);

            if (possep != std::string::npos && ((possep + 1 ) != propValue.size()))
            {
                propValue = propValue.substr(possep + 1);
            }
            else
            {
                possep = std::string::npos;
            }

            if (current.size())
            {
                T i;
                std::istringstream is(current);
                is >> i;
                toret.push_back(i);
            }
        } while (possep != std::string::npos);

        return toret;
    }

    static std::list<std::string> getConfigurationValueList(std::string propertyName, char separator = ';')
    {
        std::list<std::string> toret;

        std::string propValue = getConfigurationSValue(propertyName);
        if (!propValue.size())
        {
            return toret;
        }
        size_t possep;
        do {
            possep = propValue.find(separator);

            std::string current = propValue.substr(0,possep);

            if (possep != std::string::npos && ((possep + 1 ) != propValue.size()))
            {
                propValue = propValue.substr(possep + 1);
            }
            else
            {
                possep = std::string::npos;
            }

            if (current.size())
            {
                toret.push_back(current);
            }
        } while (possep != std::string::npos);

        return toret;
    }

    template <typename T>
    static std::set<T> getConfigurationValueSet(std::string propertyName, char separator = ';')
    {
        std::set<T> toret;

        std::string propValue = getConfigurationSValue(propertyName);
        if (!propValue.size())
        {
            return toret;
        }
        size_t possep;
        do {
            possep = propValue.find(separator);

            std::string current = propValue.substr(0,possep);

            if (possep != std::string::npos && ((possep + 1 ) != propValue.size()))
            {
                propValue = propValue.substr(possep + 1);
            }
            else
            {
                possep = std::string::npos;
            }

            if (current.size())
            {
                T i;
                std::istringstream is(current);
                is >> i;
                toret.insert(i);
            }
        } while (possep != std::string::npos);

        return toret;
    }

    static fs::path getConfigFolder();

    // creates a subfolder within the state dir and returns it (utf8)
    static fs::path getConfigFolderSubdir(const fs::path& subdirName);
    static fs::path getAndCreateRuntimeDir();
    static fs::path getAndCreateConfigDir();

    static bool getHasBeenUpdated();

    static void unloadConfiguration();

    static void migrateSyncConfig(mega::MegaApi *api);
};

}//end namespace
#endif // CONFIGURATIONMANAGER_H

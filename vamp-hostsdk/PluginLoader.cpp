/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006 Chris Cannam.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#include "PluginLoader.h"
#include "PluginHostAdapter.h"

#include "system.h"

#include <dirent.h> // POSIX directory open and read

namespace Vamp {
	
PluginLoader::PluginLoader()
{
}

PluginLoader::~PluginLoader()
{
}

std::vector<PluginLoader::PluginKey>
PluginLoader::listPlugins() 
{
    if (m_pluginLibraryMap.empty()) {

        std::vector<std::string> path = PluginHostAdapter::getPluginPath();

        size_t suffixLen = strlen(PLUGIN_SUFFIX);

        for (size_t i = 0; i < path.size(); ++i) {

            DIR *d = opendir(path[i].c_str());
            if (!d) {
//                perror("Failed to open directory");
                continue;
            }
            
            struct dirent *e = 0;
            while ((e = readdir(d))) {

                if (!(e->d_type & DT_REG) || !e->d_name) {
                    continue;
                }

                int len = strlen(e->d_name);
                if (len < int(suffixLen + 2) ||
                    e->d_name[len - suffixLen - 1] != '.' ||
                    strcmp(e->d_name + len - suffixLen, PLUGIN_SUFFIX)) {
                    continue;
                }

                std::string basename = e->d_name;
                basename = basename.substr(0, basename.length() - suffixLen - 1);
                std::string fullPath = path[i].c_str();
                fullPath = fullPath + "/" + e->d_name;
                void *handle = DLOPEN(fullPath, RTLD_LAZY);

                if (!handle) {
                    std::cerr << "Vamp::PluginLoader: " << e->d_name
                              << ": unable to load library (" << DLERROR()
                              << ")" << std::endl;
                    continue;
                }
            
                VampGetPluginDescriptorFunction fn =
                    (VampGetPluginDescriptorFunction)DLSYM
                    (handle, "vampGetPluginDescriptor");

                if (!fn) {
                    DLCLOSE(handle);
                    continue;
                }

                int index = 0;
                const VampPluginDescriptor *descriptor = 0;

                while ((descriptor = fn(VAMP_API_VERSION, index))) {
                    PluginKey key = basename + ":" + descriptor->identifier;
                    if (m_pluginLibraryMap.find(key) ==
                        m_pluginLibraryMap.end()) {
                        m_pluginLibraryMap[key] = fullPath;
                    }
                    ++index;
                }

                DLCLOSE(handle);
            }

            closedir(d);
        }
    }

    std::vector<PluginKey> plugins;
    for (std::map<PluginKey, std::string>::iterator mi =
             m_pluginLibraryMap.begin();
         mi != m_pluginLibraryMap.end(); ++mi) {
        plugins.push_back(mi->first);
    }

    return plugins;
}

std::string
PluginLoader::getLibraryPath(PluginKey key)
{
    if (m_pluginLibraryMap.empty()) (void)listPlugins();
    if (m_pluginLibraryMap.find(key) == m_pluginLibraryMap.end()) return "";
    return m_pluginLibraryMap[key];
}    

Plugin *
PluginLoader::load(PluginKey key, float inputSampleRate)
{
    std::string fullPath = getLibraryPath(key);
    if (fullPath == "") return 0;
    
    std::string::size_type ki = key.find(':');
    if (ki == std::string::npos) {
        //!!! flag error
        return 0;
    }

    std::string identifier = key.substr(ki + 1);
    
    void *handle = DLOPEN(fullPath, RTLD_LAZY);

    if (!handle) {
        std::cerr << "Vamp::PluginLoader: " << fullPath
                  << ": unable to load library (" << DLERROR()
                  << ")" << std::endl;
        return 0;
    }
    
    VampGetPluginDescriptorFunction fn =
        (VampGetPluginDescriptorFunction)DLSYM
        (handle, "vampGetPluginDescriptor");

    if (!fn) {
        //!!! refcount this! --!!! no, POSIX says dlopen/dlclose will
        // reference count. check on win32
        DLCLOSE(handle);
        return 0;
    }

    int index = 0;
    const VampPluginDescriptor *descriptor = 0;

    while ((descriptor = fn(VAMP_API_VERSION, index))) {
        if (std::string(descriptor->identifier) == identifier) {
            return new Vamp::PluginHostAdapter(descriptor, inputSampleRate);
        }
        ++index;
    }
    
    //!!! flag error
    return 0;
}

}


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

#ifndef _VAMP_PLUGIN_LOADER_H_
#define _VAMP_PLUGIN_LOADER_H_

#include <vector>
#include <string>
#include <map>

#include "PluginWrapper.h"

namespace Vamp {

class Plugin;

namespace HostExt {

class PluginLoader
{
public:
    static PluginLoader *getInstance();

    typedef std::string PluginKey;
    typedef std::vector<PluginKey> PluginKeyList;
    typedef std::vector<std::string> PluginCategoryHierarchy;

    PluginKeyList listPlugins(); //!!! pass in version number?

    PluginKey composePluginKey(std::string libraryName, std::string identifier);

    Plugin *loadPlugin(PluginKey plugin, float inputSampleRate);

    PluginCategoryHierarchy getPluginCategory(PluginKey plugin);

    std::string getLibraryPathForPlugin(PluginKey plugin);

protected:
    PluginLoader();
    virtual ~PluginLoader();

    class PluginDeletionNotifyAdapter : public PluginWrapper {
    public:
        PluginDeletionNotifyAdapter(Plugin *plugin, PluginLoader *loader);
        virtual ~PluginDeletionNotifyAdapter();
    protected:
        PluginLoader *m_loader;
    };

    virtual void pluginDeleted(PluginDeletionNotifyAdapter *adapter);

    std::map<PluginKey, std::string> m_pluginLibraryNameMap;
    void generateLibraryMap();

    std::map<PluginKey, PluginCategoryHierarchy> m_taxonomy;
    void generateTaxonomy();

    std::map<Plugin *, void *> m_pluginLibraryHandleMap;

    void *loadLibrary(std::string path);
    void unloadLibrary(void *handle);
    void *lookupInLibrary(void *handle, const char *symbol);

    std::string splicePath(std::string a, std::string b);
    std::vector<std::string> listFiles(std::string dir, std::string ext);

    static PluginLoader *m_instance;
};

}

}

#endif


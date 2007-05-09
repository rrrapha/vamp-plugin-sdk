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

#include <fstream>

#include <dirent.h> // POSIX directory open and read

using namespace std;

namespace Vamp {
	
PluginLoader::PluginLoader()
{
}

PluginLoader::~PluginLoader()
{
}

vector<PluginLoader::PluginKey>
PluginLoader::listPlugins() 
{
    if (m_pluginLibraryMap.empty()) {

        vector<string> path = PluginHostAdapter::getPluginPath();

        size_t suffixLen = strlen(PLUGIN_SUFFIX);

        for (size_t i = 0; i < path.size(); ++i) {
            
            vector<string> files = getFilesInDir(path[i], PLUGIN_SUFFIX);
            

            for (vector<string>::iterator fi = files.begin();
                 fi != files.end(); ++fi) {

                string basename = *fi;
                basename = basename.substr(0, basename.length() - suffixLen - 1);

                string fullPath = path[i];
                fullPath = fullPath + "/" + *fi; //!!! systemize
                void *handle = DLOPEN(fullPath, RTLD_LAZY);

                if (!handle) {
                    cerr << "Vamp::PluginLoader: " << *fi
                              << ": unable to load library (" << DLERROR()
                              << ")" << endl;
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
        }
    }

    vector<PluginKey> plugins;
    for (map<PluginKey, string>::iterator mi =
             m_pluginLibraryMap.begin();
         mi != m_pluginLibraryMap.end(); ++mi) {
        plugins.push_back(mi->first);
    }

    return plugins;
}

PluginLoader::PluginCategoryHierarchy
PluginLoader::getPluginCategory(PluginKey plugin)
{
    if (m_taxonomy.empty()) generateTaxonomy();
    if (m_taxonomy.find(plugin) == m_taxonomy.end()) return PluginCategoryHierarchy();
    return m_taxonomy[plugin];
}

string
PluginLoader::getLibraryPathForPlugin(PluginKey plugin)
{
    if (m_pluginLibraryMap.empty()) (void)listPlugins();
    if (m_pluginLibraryMap.find(plugin) == m_pluginLibraryMap.end()) return "";
    return m_pluginLibraryMap[plugin];
}    

Plugin *
PluginLoader::load(PluginKey key, float inputSampleRate)
{
    string fullPath = getLibraryPathForPlugin(key);
    if (fullPath == "") return 0;
    
    string::size_type ki = key.find(':');
    if (ki == string::npos) {
        //!!! flag error
        return 0;
    }

    string identifier = key.substr(ki + 1);
    
    void *handle = DLOPEN(fullPath, RTLD_LAZY);

    if (!handle) {
        cerr << "Vamp::PluginLoader: " << fullPath
                  << ": unable to load library (" << DLERROR()
                  << ")" << endl;
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
        if (string(descriptor->identifier) == identifier) {
            return new Vamp::PluginHostAdapter(descriptor, inputSampleRate);
        }
        ++index;
    }
    
    //!!! flag error
    return 0;
}

vector<string>
PluginLoader::getFilesInDir(string dir, string extension)
{
    vector<string> files;

    DIR *d = opendir(dir.c_str());
    if (!d) return files;
            
    struct dirent *e = 0;
    while ((e = readdir(d))) {
        
        if (!(e->d_type & DT_REG) || !e->d_name) {
            continue;
        }
        
        int len = strlen(e->d_name);
        if (len < int(extension.length() + 2) ||
            e->d_name[len - extension.length() - 1] != '.' ||
            strcmp(e->d_name + len - extension.length(), extension.c_str())) {
            continue;
        }

        files.push_back(e->d_name);
    }

    closedir(d);

    return files;
}

void
PluginLoader::generateTaxonomy()
{
//    cerr << "PluginLoader::generateTaxonomy" << endl;

    vector<string> path = PluginHostAdapter::getPluginPath();
    string libfragment = "/lib/";
    vector<string> catpath;

    string suffix = "cat";

    for (vector<string>::iterator i = path.begin();
         i != path.end(); ++i) {
        
        string dir = *i;
        string::size_type li = dir.find(libfragment);

        if (li != string::npos) {
            catpath.push_back
                (dir.substr(0, li)
                 + "/share/"
                 + dir.substr(li + libfragment.length()));
        }

        catpath.push_back(dir);
    }

    char buffer[1024];

    for (vector<string>::iterator i = catpath.begin();
         i != catpath.end(); ++i) {
        
        vector<string> files = getFilesInDir(*i, suffix);

        for (vector<string>::iterator fi = files.begin();
             fi != files.end(); ++fi) {

            string filepath = *i + "/" + *fi; //!!! systemize
            ifstream is(filepath.c_str(), ifstream::in | ifstream::binary);

            if (is.fail()) {
//                cerr << "failed to open: " << filepath << endl;
                continue;
            }

//            cerr << "opened: " << filepath << endl;

            while (!!is.getline(buffer, 1024)) {

                string line(buffer);

//                cerr << "line = " << line << endl;

                string::size_type di = line.find("::");
                if (di == string::npos) continue;

                string id = line.substr(0, di);
                string encodedCat = line.substr(di + 2);

                if (id.substr(0, 5) != "vamp:") continue;
                id = id.substr(5);

                while (encodedCat.length() >= 1 &&
                       encodedCat[encodedCat.length()-1] == '\r') {
                    encodedCat = encodedCat.substr(0, encodedCat.length()-1);
                }

//                cerr << "id = " << id << ", cat = " << encodedCat << endl;

                PluginCategoryHierarchy category;
                string::size_type ai;
                while ((ai = encodedCat.find(" > ")) != string::npos) {
                    category.push_back(encodedCat.substr(0, ai));
                    encodedCat = encodedCat.substr(ai + 3);
                }
                if (encodedCat != "") category.push_back(encodedCat);

                m_taxonomy[id] = category;
            }
        }
    }
}    


}

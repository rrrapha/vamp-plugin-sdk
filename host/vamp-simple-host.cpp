/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006 Chris Cannam.
    FFT code from Don Cross's public domain FFT implementation.
  
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

#include "PluginHostAdapter.h"
#include "PluginInputDomainAdapter.h"
#include "PluginLoader.h"
#include "vamp.h"

#include <iostream>
#include <sndfile.h>

#include "system.h"

#include <cmath>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

#define HOST_VERSION "1.0"

void printFeatures(int, int, int, Vamp::Plugin::FeatureSet);
void transformInput(float *, size_t);
void fft(unsigned int, bool, double *, double *, double *, double *);
void printPluginPath();
void enumeratePlugins();

/*
    A very simple Vamp plugin host.  Given the name of a plugin
    library and the name of a sound file on the command line, it loads
    the first plugin in the library and runs it on the sound file,
    dumping the plugin's first output to stdout.
*/

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 4 ||
        (argc == 2 &&
         (!strcmp(argv[1], "-?") ||
          !strcmp(argv[1], "-h") ||
          !strcmp(argv[1], "--help")))) {

        char *scooter = argv[0];
        char *name = 0;
        while (scooter && *scooter) {
            if (*scooter == '/' || *scooter == '\\') name = ++scooter;
            else ++scooter;
        }
        if (!name || !*name) name = argv[0];
        cerr << "\n"
             << name << ": A simple Vamp plugin host.\n\n"
            "Centre for Digital Music, Queen Mary, University of London.\n"
            "Copyright 2006 Chris Cannam and QMUL.\n"
            "Freely redistributable; published under a BSD-style license.\n\n"
            "Usage:\n\n"
            "  " << name << " pluginlibrary." << PLUGIN_SUFFIX << "\n\n"
            "    -- Load \"pluginlibrary\" and list the Vamp plugins it contains.\n\n"
            "  " << name << " pluginlibrary." << PLUGIN_SUFFIX << ":plugin file.wav [outputno]\n\n"
            "    -- Load plugin id \"plugin\" from \"pluginlibrary\" and run it on the\n"
            "       audio data in \"file.wav\", dumping the output from \"outputno\"\n"
            "       (default 0) to standard output.\n\n"
#ifdef HAVE_OPENDIR
            "  " << name << " -l\n\n"
            "    -- List the plugin libraries and Vamp plugins in the plugin search path.\n\n"
#endif
            "  " << name << " -p\n\n"
            "    -- Print out the Vamp plugin search path.\n\n"
	    "  " << name << " -v\n\n"
	    "    -- Display version information only.\n\n"
	    "Note that this host does not use the plugin search path when loadinga plugin.\nIf a plugin library is specified, it should be with a full file path.\n"
             << endl;
        return 2;
    }
    
    if (argc == 2 && !strcmp(argv[1], "-v")) {
	cout << "Simple Vamp plugin host version: " << HOST_VERSION << endl
	     << "Vamp API version: " << VAMP_API_VERSION << endl
	     << "Vamp SDK version: " << VAMP_SDK_VERSION << endl;
	return 0;
    }
    
    if (argc == 2 && !strcmp(argv[1], "-l")) {
        enumeratePlugins();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "-p")) {
        printPluginPath();
        return 0;
    }

    cerr << endl << argv[0] << ": Running..." << endl;

    string soname = argv[1];
    string plugid = "";
    string wavname;
    if (argc >= 3) wavname = argv[2];

    int sep = soname.find(":");
    if (sep >= 0 && sep < int(soname.length())) {
        plugid = soname.substr(sep + 1);
        soname = soname.substr(0, sep);
    }

    void *libraryHandle = DLOPEN(soname, RTLD_LAZY);

    if (!libraryHandle) {
        cerr << argv[0] << ": Failed to open plugin library " 
                  << soname << ": " << DLERROR() << endl;
        return 1;
    }

    cerr << argv[0] << ": Opened plugin library " << soname << endl;

    VampGetPluginDescriptorFunction fn = (VampGetPluginDescriptorFunction)
        DLSYM(libraryHandle, "vampGetPluginDescriptor");
    
    if (!fn) {
        cerr << argv[0] << ": No Vamp descriptor function in library "
                  << soname << endl;
        DLCLOSE(libraryHandle);
        return 1;
    }

    cerr << argv[0] << ": Found plugin descriptor function" << endl;

    int index = 0;
    int plugnumber = -1;
    const VampPluginDescriptor *descriptor = 0;

    while ((descriptor = fn(VAMP_API_VERSION, index))) {

        Vamp::PluginHostAdapter plugin(descriptor, 48000);
        cerr << argv[0] << ": Plugin " << (index+1)
                  << " is \"" << plugin.getIdentifier() << "\"" << endl;

        if (plugin.getIdentifier() == plugid) plugnumber = index;
        
        ++index;
    }

    cerr << argv[0] << ": Done\n" << endl;

    if (wavname == "") {
        DLCLOSE(libraryHandle);
        return 0;
    }

    if (plugnumber < 0) {
        if (plugid != "") {
            cerr << "ERROR: No such plugin as " << plugid << " in library"
                 << endl;
            DLCLOSE(libraryHandle);
            return 0;
        } else {
            plugnumber = 0;
        }
    }

    descriptor = fn(VAMP_API_VERSION, plugnumber);
    if (!descriptor) {
        DLCLOSE(libraryHandle);
        return 0;
    }
    
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(SF_INFO));

    sndfile = sf_open(wavname.c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
	cerr << "ERROR: Failed to open input file \"" << wavname << "\": "
	     << sf_strerror(sndfile) << endl;
        DLCLOSE(libraryHandle);
	return 1;
    }

    Vamp::Plugin *plugin =
        new Vamp::PluginInputDomainAdapter
        (new Vamp::PluginHostAdapter(descriptor, sfinfo.samplerate));

    cerr << "Running " << plugin->getIdentifier() << "..." << endl;

    int blockSize = plugin->getPreferredBlockSize();
    int stepSize = plugin->getPreferredStepSize();

    cerr << "Preferred block size = " << blockSize << ", step size = "
         << stepSize << endl;

    if (blockSize == 0) blockSize = 1024;

    bool rightBlockSize = true;

    if (plugin->getInputDomain() == Vamp::Plugin::FrequencyDomain) {

        int p = 1, b = blockSize;
        while (b) {
            p <<= 1;
            b >>= 1;
        }
        if (p != blockSize * 2) {
            cerr << "WARNING: Plugin requested non-power-of-two block size of "
                 << blockSize << ",\nwhich is not supported by this host.  ";
            blockSize = p;
            cerr << "Rounding up to " << blockSize << "." << endl;
            rightBlockSize = false;
        }
        if (stepSize == 0) stepSize = blockSize / 2;

    } else {

        if (stepSize == 0) stepSize = blockSize;
    }

    int channels = sfinfo.channels;

    float *filebuf = new float[blockSize * channels];
    float **plugbuf = new float*[channels];
    for (int c = 0; c < channels; ++c) plugbuf[c] = new float[blockSize + 2];

    cerr << "Using block size = " << blockSize << ", step size = "
              << stepSize << endl;

    int minch = plugin->getMinChannelCount();
    int maxch = plugin->getMaxChannelCount();
    cerr << "Plugin accepts " << minch << " -> " << maxch << " channel(s)" << endl;

    Vamp::Plugin::OutputList outputs = plugin->getOutputDescriptors();
    Vamp::Plugin::OutputDescriptor od;

    int returnValue = 1;

    int output = 0;
    if (argc == 4) output = atoi(argv[3]);

    bool mix = false;

    if (minch > channels || maxch < channels) {
        if (minch == 1) {
            cerr << "WARNING: Sound file has " << channels << " channels, mixing down to 1" << endl;
            mix = true;
            channels = 1;
        } else {
            cerr << "ERROR: Sound file has " << channels << " channels, out of range for plugin" << endl;
            goto done;
        }
    }

    if (outputs.empty()) {
	cerr << "Plugin has no outputs!" << endl;
        goto done;
    }

    if (int(outputs.size()) <= output) {
	cerr << "Output " << output << " requested, but plugin has only " << outputs.size() << " output(s)" << endl;
        goto done;
    }        

    od = outputs[output];
    cerr << "Output is " << od.identifier << endl;

    if (!plugin->initialise(channels, stepSize, blockSize)) {
        cerr << "ERROR: Plugin initialise (channels = " << channels
             << ", stepSize = " << stepSize << ", blockSize = "
             << blockSize << ") failed." << endl;
        if (!rightBlockSize) {
            cerr << "(Probably because I couldn't provide the plugin's preferred block size.)" << endl;
        }
        goto done;
    }

    for (size_t i = 0; i < sfinfo.frames; i += stepSize) {

        int count;

        if (sf_seek(sndfile, i, SEEK_SET) < 0) {
            cerr << "ERROR: sf_seek failed: " << sf_strerror(sndfile) << endl;
            break;
        }
        
        if ((count = sf_readf_float(sndfile, filebuf, blockSize)) < 0) {
            cerr << "ERROR: sf_readf_float failed: " << sf_strerror(sndfile) << endl;
            break;
        }

        for (int c = 0; c < channels; ++c) {
            for (int j = 0; j < blockSize; ++j) {
                plugbuf[c][j] = 0.0f;
            }
        }

        for (int j = 0; j < blockSize && j < count; ++j) {
            int tc = 0;
            for (int c = 0; c < sfinfo.channels; ++c) {
                tc = c;
                if (mix) tc = 0;
                plugbuf[tc][j] += filebuf[j * sfinfo.channels + c];
            }
            if (mix) {
                plugbuf[0][j] /= sfinfo.channels;
            }
        }

        printFeatures
            (i, sfinfo.samplerate, output, plugin->process
             (plugbuf, Vamp::RealTime::frame2RealTime(i, sfinfo.samplerate)));
    }

    printFeatures(sfinfo.frames, sfinfo.samplerate, output,
                  plugin->getRemainingFeatures());

    returnValue = 0;

done:
    delete plugin;

    DLCLOSE(libraryHandle);
    sf_close(sndfile);
    return returnValue;
}

void
printPluginPath()
{
    vector<string> path = Vamp::PluginHostAdapter::getPluginPath();
    for (size_t i = 0; i < path.size(); ++i) {
        cerr << path[i] << endl;
    }
}

void
enumeratePlugins()
{
    Vamp::PluginLoader loader;

    cerr << endl << "Vamp plugin libraries found in search path:" << endl;

    std::vector<Vamp::PluginLoader::PluginKey> plugins = loader.listPlugins();
    typedef std::multimap<std::string, Vamp::PluginLoader::PluginKey>
        LibraryMap;
    LibraryMap libraryMap;

    for (size_t i = 0; i < plugins.size(); ++i) {
        std::string path = loader.getLibraryPath(plugins[i]);
        libraryMap.insert(LibraryMap::value_type(path, plugins[i]));
    }

    std::string prevPath = "";
    int index = 0;

    for (LibraryMap::iterator i = libraryMap.begin();
         i != libraryMap.end(); ++i) {
        
        std::string path = i->first;
        Vamp::PluginLoader::PluginKey key = i->second;

        if (path != prevPath) {
            prevPath = path;
            index = 0;
            cerr << "\n  " << path << ":" << endl;
        }

        Vamp::Plugin *plugin = loader.load(key, 48000);
        if (plugin) {

            char c = char('A' + index);
            if (c > 'Z') c = char('a' + (index - 26));

            cerr << "    [" << c << "] [v"
                 << plugin->getVampApiVersion() << "] "
                 << plugin->getName() << ", \""
                 << plugin->getIdentifier() << "\"" << " ["
                 << plugin->getMaker() << "]" << endl;

            if (plugin->getDescription() != "") {
                cerr << "        - " << plugin->getDescription() << endl;
            }

            Vamp::Plugin::OutputList outputs =
                plugin->getOutputDescriptors();

            if (outputs.size() > 1) {
                for (size_t j = 0; j < outputs.size(); ++j) {
                    cerr << "         (" << j << ") "
                         << outputs[j].name << ", \""
                         << outputs[j].identifier << "\"" << endl;
                    if (outputs[j].description != "") {
                        cerr << "             - " 
                             << outputs[j].description << endl;
                    }
                }
            }

            ++index;
        }
    }

    cerr << endl;
}

void
printFeatures(int frame, int sr, int output, Vamp::Plugin::FeatureSet features)
{
    for (unsigned int i = 0; i < features[output].size(); ++i) {
        Vamp::RealTime rt = Vamp::RealTime::frame2RealTime(frame, sr);
        if (features[output][i].hasTimestamp) {
            rt = features[output][i].timestamp;
        }
        cout << rt.toString() << ":";
        for (unsigned int j = 0; j < features[output][i].values.size(); ++j) {
            cout << " " << features[output][i].values[j];
        }
        cout << endl;
    }
}


        

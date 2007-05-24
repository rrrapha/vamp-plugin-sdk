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

#ifndef _VAMP_PLUGIN_INPUT_DOMAIN_ADAPTER_H_
#define _VAMP_PLUGIN_INPUT_DOMAIN_ADAPTER_H_

#include "PluginWrapper.h"

namespace Vamp {

namespace HostExt {

/**
 * An adapter that converts time-domain input into frequency-domain
 * input for plugins that need it.  In every other respect this
 * adapter behaves like the plugin it wraps.  The wrapped plugin may
 * be a time-domain plugin, in which case this wrapper does nothing.
 *
 * Uses a Hanning windowed FFT.  The FFT implementation is not the
 * fastest, so a host can do much better if it cares enough, but it is
 * simple and self-contained.
 *
 * Note that this adapter does not support non-power-of-two block
 * sizes.
 */

//!!! It would also be nice to have a channel wrapper, which deals
//with mixing down channels if the plugin needs a different number
//from the input source.  It would have some sort of mixdown/channel
//input policy selection.  Probably this class and that one should
//both inherit a PluginAdapter class which contains a plugin and
//delegates all calls through to it; the subclass can then override
//only the ones it needs to handle.

class PluginInputDomainAdapter : public PluginWrapper
{
public:
    PluginInputDomainAdapter(Plugin *plugin); // I take ownership of plugin
    virtual ~PluginInputDomainAdapter();
    
    bool initialise(size_t channels, size_t stepSize, size_t blockSize);

    InputDomain getInputDomain() const;

    size_t getPreferredStepSize() const;
    size_t getPreferredBlockSize() const;

    FeatureSet process(const float *const *inputBuffers, RealTime timestamp);

protected:
    size_t m_channels;
    size_t m_blockSize;
    float **m_freqbuf;
    double *m_ri;
    double *m_ro;
    double *m_io;

    void fft(unsigned int n, bool inverse,
             double *ri, double *ii, double *ro, double *io);
};

}

}

#endif

#pragma once

// Per-strip convolution reverb (uniform-partitioned overlap-save FFT convolution).
// The impulse response is precomputed once (off the audio thread) into a shared,
// immutable ConvIR; the audio thread only reads it. Per-strip state (FDL, overlap
// buffers) lives in the insert chain so tails persist across blocks.

#include "dsp/InsertEffects.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace synth { namespace fx {

// ---- Small iterative radix-2 FFT (in-place, complex split arrays) ----
inline void convFft(std::vector<float> &re, std::vector<float> &im, bool inverse)
{
    const int n = int(re.size());
    if(n < 2) return;
    // bit reversal
    for(int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) { std::swap(re[(size_t)i], re[(size_t)j]); std::swap(im[(size_t)i], im[(size_t)j]); }
    }
    const float pi = 3.14159265358979323846f;
    for(int len = 2; len <= n; len <<= 1)
    {
        const float ang = (inverse ? 2.0f : -2.0f) * pi / float(len);
        const float wlr = std::cos(ang), wli = std::sin(ang);
        for(int i = 0; i < n; i += len)
        {
            float wr = 1.0f, wi = 0.0f;
            for(int k = 0; k < len / 2; ++k)
            {
                const int a = i + k, b = i + k + len / 2;
                const float ur = re[(size_t)a], ui = im[(size_t)a];
                const float vr = re[(size_t)b] * wr - im[(size_t)b] * wi;
                const float vi = re[(size_t)b] * wi + im[(size_t)b] * wr;
                re[(size_t)a] = ur + vr; im[(size_t)a] = ui + vi;
                re[(size_t)b] = ur - vr; im[(size_t)b] = ui - vi;
                const float nwr = wr * wlr - wi * wli;
                wi = wr * wli + wi * wlr; wr = nwr;
            }
        }
    }
    if(inverse)
        for(int i = 0; i < n; ++i) { re[(size_t)i] /= float(n); im[(size_t)i] /= float(n); }
}

// Build a partitioned, FFT-domain ConvIR from a mono impulse response.
// hop must be a power of two; the FFT size is 2*hop.
inline std::shared_ptr<const ConvIR> buildConvIR(const std::vector<float> &samples,
                                                 int hop, float maxSeconds, double sampleRate,
                                                 const std::string &name)
{
    auto ir = std::make_shared<ConvIR>();
    if(samples.empty() || hop < 16)
        return ir;
    const int maxLen = std::max(hop, int(maxSeconds * float(sampleRate)));
    const int len = std::min(int(samples.size()), maxLen);
    const int fftN = hop * 2;
    const int parts = (len + hop - 1) / hop;
    ir->hop = hop; ir->fftN = fftN; ir->parts = parts;
    ir->name = name;
    ir->re.assign(size_t(parts) * size_t(fftN), 0.0f);
    ir->im.assign(size_t(parts) * size_t(fftN), 0.0f);

    // Normalize IR energy so wet level is reasonable across different files.
    double energy = 0.0;
    for(int i = 0; i < len; ++i) energy += double(samples[(size_t)i]) * double(samples[(size_t)i]);
    const float norm = energy > 1e-9 ? float(1.0 / std::sqrt(energy)) : 1.0f;

    std::vector<float> re(size_t(fftN), 0.0f), im(size_t(fftN), 0.0f);
    for(int p = 0; p < parts; ++p)
    {
        std::fill(re.begin(), re.end(), 0.0f);
        std::fill(im.begin(), im.end(), 0.0f);
        for(int k = 0; k < hop; ++k)
        {
            const int idx = p * hop + k;
            re[(size_t)k] = idx < len ? samples[(size_t)idx] * norm : 0.0f; // first hop, rest zero-padded
        }
        convFft(re, im, false);
        float *dr = ir->re.data() + size_t(p) * size_t(fftN);
        float *di = ir->im.data() + size_t(p) * size_t(fftN);
        std::copy(re.begin(), re.end(), dr);
        std::copy(im.begin(), im.end(), di);
    }
    return ir;
}

struct ConvChannel
{
    std::vector<float> prev;     // previous hop samples (size hop)
    std::vector<float> accum;    // current input hop being filled (size hop)
    int fill = 0;
    std::vector<float> fdlRe, fdlIm; // parts * fftN ring of input block spectra
    int fdlPos = 0;
    std::vector<float> outFifo;  // ready output block (size hop)
    int outPos = 0;
    // scratch
    std::vector<float> wr, wi, yr, yi;
};

struct ConvReverbFxState
{
    const ConvIR *bound = nullptr;
    int hop = 0, fftN = 0, parts = 0;
    std::array<ConvChannel, 2> ch;
    // pre-delay line
    std::vector<float> pdL, pdR;
    int pdPos = 0;

    void reset()
    {
        bound = nullptr; hop = fftN = parts = 0; pdPos = 0;
        pdL.clear(); pdR.clear();
        for(auto &c : ch) c = ConvChannel{};
    }

    void bindTo(const ConvIR *ir)
    {
        bound = ir; hop = ir->hop; fftN = ir->fftN; parts = ir->parts;
        for(auto &c : ch)
        {
            c.prev.assign((size_t)hop, 0.0f);
            c.accum.assign((size_t)hop, 0.0f);
            c.fill = 0;
            c.fdlRe.assign(size_t(parts) * size_t(fftN), 0.0f);
            c.fdlIm.assign(size_t(parts) * size_t(fftN), 0.0f);
            c.fdlPos = 0;
            c.outFifo.assign((size_t)hop, 0.0f);
            c.outPos = 0;
            c.wr.assign((size_t)fftN, 0.0f); c.wi.assign((size_t)fftN, 0.0f);
            c.yr.assign((size_t)fftN, 0.0f); c.yi.assign((size_t)fftN, 0.0f);
        }
    }
};

inline void convRunHop(ConvChannel &c, const ConvIR &ir)
{
    const int hop = ir.hop, fftN = ir.fftN, parts = ir.parts;
    // window = [prev | accum]
    for(int k = 0; k < hop; ++k) { c.wr[(size_t)k] = c.prev[(size_t)k]; c.wi[(size_t)k] = 0.0f; }
    for(int k = 0; k < hop; ++k) { c.wr[(size_t)(hop + k)] = c.accum[(size_t)k]; c.wi[(size_t)(hop + k)] = 0.0f; }
    convFft(c.wr, c.wi, false);
    // store into FDL
    std::copy(c.wr.begin(), c.wr.end(), c.fdlRe.begin() + size_t(c.fdlPos) * size_t(fftN));
    std::copy(c.wi.begin(), c.wi.end(), c.fdlIm.begin() + size_t(c.fdlPos) * size_t(fftN));
    // accumulate Y = sum_p FDL[(pos-p)] * IR[p]
    std::fill(c.yr.begin(), c.yr.end(), 0.0f);
    std::fill(c.yi.begin(), c.yi.end(), 0.0f);
    for(int p = 0; p < parts; ++p)
    {
        int fi = c.fdlPos - p; if(fi < 0) fi += parts;
        const float *xr = c.fdlRe.data() + size_t(fi) * size_t(fftN);
        const float *xi = c.fdlIm.data() + size_t(fi) * size_t(fftN);
        const float *hr = ir.re.data() + size_t(p) * size_t(fftN);
        const float *hi = ir.im.data() + size_t(p) * size_t(fftN);
        for(int k = 0; k < fftN; ++k)
        {
            c.yr[(size_t)k] += xr[k] * hr[k] - xi[k] * hi[k];
            c.yi[(size_t)k] += xr[k] * hi[k] + xi[k] * hr[k];
        }
    }
    convFft(c.yr, c.yi, true);
    // overlap-save: keep the LAST hop samples as the valid output block
    for(int k = 0; k < hop; ++k) c.outFifo[(size_t)k] = c.yr[(size_t)(hop + k)];
    c.outPos = 0;
    // shift
    c.prev = c.accum;
    c.fdlPos = (c.fdlPos + 1) % parts;
}

// modOff: {mix, gain, predelay, -}
inline void processConvReverb(float *L, float *R, int n, double sampleRate,
                              const ConvSlotParams &cp, ConvReverbFxState &st, const float modOff[4])
{
    const ConvIR *ir = cp.ir.get();
    if(ir == nullptr || ir->parts <= 0 || ir->hop <= 0)
        return; // no IR loaded → passthrough
    if(st.bound != ir)
        st.bindTo(ir);

    const float mix  = std::max(0.0f, std::min(1.0f, cp.mix + modOff[0]));
    const float gain = std::max(0.0f, std::min(2.0f, cp.gain + modOff[1]));
    const float predelayMs = std::max(0.0f, std::min(200.0f, cp.predelayMs + modOff[2] * 200.0f));

    // pre-delay ring
    const int pdLen = std::max(1, int(0.001f * predelayMs * float(sampleRate)) + 1);
    if(int(st.pdL.size()) != pdLen)
    {
        st.pdL.assign((size_t)pdLen, 0.0f);
        st.pdR.assign((size_t)pdLen, 0.0f);
        st.pdPos = 0;
    }
    const int pd = std::clamp(int(0.001f * predelayMs * float(sampleRate)), 0, pdLen - 1);

    auto &cl = st.ch[0];
    auto &cr = st.ch[1];
    for(int s = 0; s < n; ++s)
    {
        // pre-delay tap
        int rp = st.pdPos - pd; if(rp < 0) rp += pdLen;
        const float inL = st.pdL[(size_t)rp];
        const float inR = st.pdR[(size_t)rp];
        st.pdL[(size_t)st.pdPos] = L[s];
        st.pdR[(size_t)st.pdPos] = R[s];
        if(++st.pdPos >= pdLen) st.pdPos = 0;

        const float wetL = cl.outFifo[(size_t)cl.outPos++];
        const float wetR = cr.outFifo[(size_t)cr.outPos++];
        cl.accum[(size_t)cl.fill] = inL;
        cr.accum[(size_t)cr.fill] = inR;
        ++cl.fill; ++cr.fill;
        if(cl.fill >= st.hop) { convRunHop(cl, *ir); cl.fill = 0; }
        if(cr.fill >= st.hop) { convRunHop(cr, *ir); cr.fill = 0; }

        L[s] = L[s] * (1.0f - mix) + wetL * gain * mix;
        R[s] = R[s] * (1.0f - mix) + wetR * gain * mix;
    }
}

}} // namespace synth::fx

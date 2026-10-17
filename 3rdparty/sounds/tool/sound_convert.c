/* sound_convert: turns any file miniaudio can decode (WAV, FLAC, MP3, Ogg Vorbis)
 * into the format the game embeds: mono, signed 16-bit WAV, peak -1 dBFS.
 * Dev tool only (cmake --build build --target sound_convert); never shipped.
 *
 *   sound_convert <in> <out.wav> [--start S] [--length S] [--loop] [--xfade S]
 *                 [--rate HZ] [--rms DBFS [--limit]]
 *
 * --start/--length cut a section (seconds). Without --loop both ends get 5 ms fades.
 * With --loop the last X seconds are crossfaded (equal power) into the start
 * (X = --xfade, default 0.5 s, at most length / 4), so the result repeats without a click;
 * the output is X shorter than the section.
 * --rate is the output sample rate (default 22050).
 * --rms sets the loudness instead of the peak: the gain brings the RMS level to DBFS
 * (e.g. -20), lowered if the peak would then pass -1 dBFS. With --limit the gain is kept
 * and a peak limiter (5 ms lookahead, 150 ms release) holds the peaks at -1 dBFS instead. */
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 4096

static int fail(const char* what, const char* detail) {
    fprintf(stderr, "sound_convert: %s %s\n", what, detail);
    return 1;
}

int main(int argc, char** argv) {
    double start = 0.0, length = 0.0, xfade = 0.5, rms_db = 0.0;
    int loop = 0, use_rms = 0, limit = 0, rate = 22050;
    if (argc < 3) {
        fprintf(stderr, "usage: sound_convert <in> <out.wav> [--start S] [--length S] [--loop] [--xfade S] [--rate HZ] [--rms DBFS]\n");
        return 2;
    }
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--start") == 0 && i + 1 < argc)
            start = atof(argv[++i]);
        else if (strcmp(argv[i], "--length") == 0 && i + 1 < argc)
            length = atof(argv[++i]);
        else if (strcmp(argv[i], "--loop") == 0)
            loop = 1;
        else if (strcmp(argv[i], "--xfade") == 0 && i + 1 < argc)
            xfade = atof(argv[++i]);
        else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
            rate = atoi(argv[++i]);
        else if (strcmp(argv[i], "--rms") == 0 && i + 1 < argc) {
            rms_db = atof(argv[++i]);
            use_rms = 1;
        } else if (strcmp(argv[i], "--limit") == 0)
            limit = 1;
        else
            return fail("unknown option", argv[i]);
    }
    if (rate < 8000 || rate > 96000)
        return fail("--rate must be within", "8000..96000");

    ma_decoder_config dconf = ma_decoder_config_init(ma_format_f32, 1, (ma_uint32)rate);
    ma_decoder dec;
    if (ma_decoder_init_file(argv[1], &dconf, &dec) != MA_SUCCESS)
        return fail("cannot decode", argv[1]);
    size_t total = 0, cap = 0;
    float* pcm = NULL;
    for (;;) {
        if (total + CHUNK > cap) {
            cap = cap ? cap * 2 : (size_t)1 << 16;
            float* grown = realloc(pcm, cap * sizeof(float));
            if (!grown)
                return fail("out of memory", "");
            pcm = grown;
        }
        ma_uint64 got = 0;
        ma_decoder_read_pcm_frames(&dec, pcm + total, CHUNK, &got);
        total += (size_t)got;
        if (got < CHUNK)
            break;
    }
    ma_decoder_uninit(&dec);

    const size_t first = (size_t)(start * rate);
    if (first >= total)
        return fail("--start is past the end of", argv[1]);
    size_t n = total - first;
    if (length > 0.0 && (size_t)(length * rate) < n)
        n = (size_t)(length * rate);
    float* s = pcm + first;

    if (loop) {
        size_t x = (size_t)(xfade * rate);
        if (x > n / 4)
            x = n / 4;
        for (size_t i = 0; i < x; ++i) {
            const float w = (float)i / (float)x;
            s[i] = s[i] * sqrtf(w) + s[n - x + i] * sqrtf(1.f - w);
        }
        n -= x;
    } else {
        size_t f = (size_t)(0.005 * rate);
        if (f > n / 2)
            f = n / 2;
        for (size_t i = 0; i < f; ++i) {
            const float w = (float)i / (float)f;
            s[i] *= w;
            s[n - 1 - i] *= w;
        }
    }

    float peak = 0.f;
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (fabsf(s[i]) > peak)
            peak = fabsf(s[i]);
        sum += (double)s[i] * s[i];
    }
    const float max_gain = peak > 0.f ? 0.8912509f / peak : 1.f; /* -1 dBFS */
    float gain = max_gain;
    if (use_rms && sum > 0.0) {
        gain = (float)(pow(10.0, rms_db / 20.0) / sqrt(sum / (double)n));
        if (gain > max_gain && limit) {
            const size_t ahead = (size_t)(0.005 * rate);
            const float release = 1.f - expf(-1.f / (0.15f * (float)rate));
            float g = 1.f;
            for (size_t i = 0; i < n; ++i) {
                float need = 0.f;
                for (size_t j = i; j < n && j <= i + ahead; ++j)
                    if (fabsf(s[j]) > need)
                        need = fabsf(s[j]);
                need *= gain;
                const float target = need > 0.8912509f ? 0.8912509f / need : 1.f;
                g = target < g ? target : g + (1.f - g) * release;
                if (g > target)
                    g = target;
                s[i] *= g;
            }
        } else if (gain > max_gain) {
            fprintf(stderr, "sound_convert: %s: peak limits the gain, RMS ends up %.1f dB below %.1f dBFS\n",
                    argv[1], 20.0 * log10(gain / max_gain), rms_db);
            gain = max_gain;
        }
    }

    ma_int16* out = malloc(n * sizeof(ma_int16));
    if (!out)
        return fail("out of memory", "");
    for (size_t i = 0; i < n; ++i)
        out[i] = (ma_int16)lrintf(s[i] * gain * 32767.f);

    ma_encoder_config econf = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 1, (ma_uint32)rate);
    ma_encoder enc;
    if (ma_encoder_init_file(argv[2], &econf, &enc) != MA_SUCCESS)
        return fail("cannot write", argv[2]);
    ma_uint64 written = 0;
    ma_encoder_write_pcm_frames(&enc, out, n, &written);
    ma_encoder_uninit(&enc);
    printf("%s: %.2f s -> %s: %.2f s, gain %.2f\n", argv[1], (double)total / rate, argv[2],
           (double)n / rate, (double)gain);
    free(out);
    free(pcm);
    return written == n ? 0 : fail("short write to", argv[2]);
}

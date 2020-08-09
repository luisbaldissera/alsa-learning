#include <stdio.h>
#include <stdlib.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#define SWAP16LE(i) ((((i)&0x00ff)<<8)|(((i)&0xff00)>>8))

int16_t sum16le(int16_t a, int16_t b) {
    int na, nb, r;
    na = SWAP16LE(a);
    nb = SWAP16LE(b);
    r = na + nb;
    return SWAP16LE(r);
}

int16_t div16le(int16_t a, int16_t b) {
    int na, nb, r;
    na = SWAP16LE(a);
    nb = SWAP16LE(b);
    r = na / nb;
    return SWAP16LE(r);
}

void filter(int16_t *src, int16_t *dest, snd_pcm_uframes_t frames, int n) {
    int i, chan[2], j, c[2], a[2];
    int16_t nle = SWAP16LE(n);
    for (i = n; i < frames; i += n) {
        a[0] = src[2*(i-n)];
        a[1] = src[2*(i-n) + 1];
        c[0] = div16le(sum16le(src[2*i], a[0]), nle);
        c[1] = div16le(sum16le(src[2*i + 1], a[1]), nle);
        for (j = i-n+1; j < i; j++) {
            a[0] = sum16le(a[0], c[0]);
            a[1] = sum16le(a[1], c[1]);
            dest[2*j] = a[0];
            dest[2*j+1] = a[1];
        }
        if (i == frames - 1) break;
        if (i + n > frames) {
            i = frames - 1;
        }
    }
}

void pitch(int16_t *src, int16_t *dest, snd_pcm_uframes_t frames, int pitch) {
    int i, r, l, n;
    for (i = 0; i < 2 * frames; i++) {
        r = src[i] & 0xff;
        l = (src[i] >> 8);
        n = (r << 8) | l;
        n = n + pitch;
        r = n & 0xff;
        l = (n >> 8);
        dest[i] = (r << 8) | l;
    }
}

int main(int argc, char const *argv[]) {
    long loops;
    int rc;
    int size;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int dir;
    int val;
    snd_pcm_uframes_t frames;
    char *buffer;
    char *filtered;
    int is_filter = 0;
    int n_filter;
    int is_pitch = 0;
    int n_pitch;
    if (argc > 2) {
        if (strcmp(argv[1], "filter") == 0) {
            is_filter = 1;
            n_filter = atoi(argv[2]);
        } else if (strcmp(argv[1], "pitch") == 0) {
            is_pitch = 1;
            n_pitch = atoi(argv[2]);
        }
    }
    /* Open PCM for playback */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }
    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);
    /* Fill it with default values */
    snd_pcm_hw_params_any(handle, params);
    /* Set desired parameters */
    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 2);
    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    /* Set period size to 32 frames */
    frames = (snd_pcm_uframes_t) 32;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    /* Write parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }
    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4; /* 2 bytes per sample, 2 channels */
    buffer = (char*) malloc(size);
    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    /* 5 seconds in microseconds divided by period time */
    loops = 5000000 / val;
    while (loops > 0) {
        loops--;
        rc = read(0, buffer, size);
        if (rc == 0) {
            fprintf(stderr, "end of file on input\n");
            break;
        } else if (rc != size) {
            fprintf(stderr, "short read: read %d bytes\n", rc);
        }
        if (is_filter) {
            filter((int16_t*) buffer, (int16_t*) buffer, frames, n_filter);
        }
        if (is_pitch) {
            pitch((int16_t*) buffer, (int16_t*) buffer, frames, n_pitch);
        }
        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) {
            /* EPIPE means underrun */
            fprintf(stderr, "underrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
        } else if (rc != (int) frames) {
            fprintf(stderr, "short write, write %d frames\n", rc);
        }
    }
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}

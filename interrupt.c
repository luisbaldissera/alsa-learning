#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define DEBUG 1
#define AHANDLE(cmd) do{if(DEBUG){fprintf(stderr,"exec: %s\n", #cmd);};int err;if((err = (cmd))<0){if(err==-EPIPE){fprintf(stderr,"An xrun occured.\n");}else{fprintf(stderr,"Alsa error in '%s' line %d: %s\n",__FILE__,__LINE__,snd_strerror(err));exit(1);}}}while(0)

snd_pcm_t *playback_handle;
short buf[4096];

int playback_callback(snd_pcm_sframes_t nframes) {
    int err;
    printf("playback callback called with %lu frames\n", nframes);

    /* ... fill buf with data ... */
    // for (int i = 0; i < 4096; i++) {
    //     buf[i] = (short) sin(i);
    // }

    err = snd_pcm_writei(playback_handle, buf, nframes);
    if (err < 0) {
        fprintf(stderr, "write failed (%s)\n", snd_strerror(err));
    }
    return err;
}

int main(int argc, char *argv[]) {
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sframes_t frames_to_deliver;
    int nfds;
    int err;
    int rate = 44100;
    struct pollfd *pfds;
    AHANDLE(snd_pcm_open(&playback_handle, argv[1], SND_PCM_STREAM_PLAYBACK, 0));
    AHANDLE(snd_pcm_hw_params_malloc(&hw_params));
    AHANDLE(snd_pcm_hw_params_any(playback_handle, hw_params));
    AHANDLE(snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    AHANDLE(snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE));
    AHANDLE(snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0));
    AHANDLE(snd_pcm_hw_params_set_channels(playback_handle, hw_params, 2));
    AHANDLE(snd_pcm_hw_params(playback_handle, hw_params));
    snd_pcm_hw_params_free(hw_params);
    /* Tell ALSA to wake us up whenever 4096 or more frames
     * of playback data can be delivered. Also, tell
     * ALSA that we'll start the device ourselves.  */
    AHANDLE(snd_pcm_sw_params_malloc(&sw_params));
    AHANDLE(snd_pcm_sw_params_current(playback_handle, sw_params));
    AHANDLE(snd_pcm_sw_params_set_avail_min(playback_handle, sw_params, 4096));
    AHANDLE(snd_pcm_sw_params_set_start_threshold(playback_handle, sw_params, 0U));
    AHANDLE(snd_pcm_sw_params(playback_handle, sw_params));
    /* The interface will interrupt the kernel every 4096 frames,
     * and ALSA will wake up this program very soon after that */
    AHANDLE(snd_pcm_prepare(playback_handle));
    while (1) {
        /* Wait till the interface is ready for data, or 1 second
         * has elapsed */
        AHANDLE(snd_pcm_wait(playback_handle, 1000));
        /* Find out how much space is available for playback data */
        AHANDLE(frames_to_deliver = snd_pcm_avail_update(playback_handle));
        frames_to_deliver = frames_to_deliver > 4096 ? 4096 : frames_to_deliver;
        /* Deliver the data */
        if (playback_callback(frames_to_deliver) != frames_to_deliver) {
            fprintf(stderr, "playback callback failed\n");
            break;
        }
    }
    snd_pcm_close(playback_handle);
    return 0;
}



#include "sound/core.h"
#include "heap.h"
#include "string.h"
#include "console.h"

static LIST_HEAD(snd_cards);

int snd_card_new(int index, const char *id, struct snd_card **card_ret) {
    struct snd_card *card = (struct snd_card*)kmalloc(sizeof(struct snd_card));
    if (!card) return -1;
    
    memset(card, 0, sizeof(struct snd_card));
    card->number = index;
    if (id) strncpy(card->id, id, 15);
    
    INIT_LIST_HEAD(&card->devices);
    
    *card_ret = card;
    return 0;
}

int snd_card_register(struct snd_card *card) {
    list_add_tail(&card->list, &snd_cards);
    kprint_str("[ALSA] Registered Card: ");
    kprint_str(card->longname);
    kprint_newline();
    return 0;
}

int snd_pcm_new(struct snd_card *card, const char *id, int device, int playback_count, int capture_count, struct snd_pcm **pcm_ret) {
    struct snd_pcm *pcm = (struct snd_pcm*)kmalloc(sizeof(struct snd_pcm));
    if (!pcm) return -1;
    
    memset(pcm, 0, sizeof(struct snd_pcm));
    pcm->card = card;
    pcm->device = device;
    if (id) strncpy(pcm->name, id, 79);
    
    if (playback_count > 0) {
        pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].stream = SNDRV_PCM_STREAM_PLAYBACK;
        pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].pcm = pcm;
    }
    
    list_add_tail(&pcm->list, &card->devices);
    *pcm_ret = pcm;
    return 0;
}

void snd_print_cards(void) {
    struct snd_card *card;
    struct list_head *pos;
    kprint_str("ALSA Sound Cards:\n");
    list_for_each(pos, &snd_cards) {
        card = list_entry(pos, struct snd_card, list);
        kprint_str("  Card "); kprint_dec(card->number); kprint_str(": ");
        kprint_str(card->longname);
        kprint_newline();
    }
}

/*
 * ALSA Soc TABX PCM codec support
 * Generic PCM support.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/ac97_codec.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "tabx_pcm.h"

/*
 * Note this is a simple chip with no configuration interface, sample rate is
 * determined automatically by examining the Master clock and Bit clock ratios
 */
#define TABX_PCM_RATES (SNDRV_PCM_RATE_8000_96000)

struct snd_soc_dai tabx_pcm_dai = {
    .name = "TABX_PCM",
    .playback = {
        .stream_name  = "Playback",
        .channels_min = 1,
        .channels_max = 2,
        .rates        = TABX_PCM_RATES,
        .formats      = SNDRV_PCM_FMTBIT_S16_LE,
        },
};
EXPORT_SYMBOL_GPL(tabx_pcm_dai);

static int tabx_pcm_soc_probe(struct platform_device *pdev)
{
    struct snd_soc_device *socdev = platform_get_drvdata(pdev);
    struct snd_soc_codec *codec;
    int ret = 0;

    codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
    if(codec == NULL)return(-ENOMEM);
    mutex_init(&codec->mutex);
    codec->name     = "TABX_PCM";
    codec->owner    = THIS_MODULE;
    codec->dai      = &tabx_pcm_dai;
    codec->num_dai  = 1;
    socdev->card->codec = codec;
    INIT_LIST_HEAD(&codec->dapm_widgets);
    INIT_LIST_HEAD(&codec->dapm_paths);

    /* register pcms */
    ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
    if(ret < 0) {
        printk(KERN_ERR "tabx_pcm: failed to create pcms\n");
        goto pcm_err;
    }
    return(ret);

pcm_err:
    kfree(socdev->card->codec);
    socdev->card->codec = NULL;
    return(ret);
}

static int tabx_pcm_soc_remove(struct platform_device *pdev)
{
    struct snd_soc_device *socdev = platform_get_drvdata(pdev);
    struct snd_soc_codec *codec   = socdev->card->codec;

    if(codec == NULL) return(0);
    snd_soc_free_pcms(socdev);
    kfree(codec);
    return(0);
}

struct snd_soc_codec_device soc_codec_dev_tabx_pcm = {
    .probe  = tabx_pcm_soc_probe,
    .remove = tabx_pcm_soc_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_tabx_pcm);

static int __init tabx_pcm_init(void)
{
    int ret;
    ret = snd_soc_register_dai(&tabx_pcm_dai);
    if(ret != 0) {
        printk(KERN_ERR "tabx_pcm: Failed to register DAI: %d\n", ret);
    	return(ret);
    }
    printk(KERN_ERR "tabx_pcm: pcm dai registered\n");
    return(ret);
}
module_init(tabx_pcm_init);

static void __exit tabx_pcm_exit(void)
{
    snd_soc_unregister_dai(&tabx_pcm_dai);
}
module_exit(tabx_pcm_exit);

MODULE_DESCRIPTION("Soc TABX PCM CODEC driver");
MODULE_AUTHOR("Donnaware International LLC");
MODULE_LICENSE("GPL");


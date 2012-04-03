/*---------------------------------------------------------------------------*/
/*                                                                           */
/* File:         sound/soc/atmel/at91rm9200_tabx.c                           */
/* Description:  board driver for tabx sound chip                            */
/* based on sam9g45_wm8727.c by Ankur Patel<ankur.patel@quipment.in>         */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define DEBUG	1
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <linux/atmel-ssc.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>

#include <asm/dma.h>
#include <linux/gpio.h>

#include "atmel-pcm.h"
#include "atmel_ssc_dai.h"

#include "../codecs/tabx_pcm.h"

#define MCLK	60000000	/* Main Clock	*/

/*---------------------------------------------------------------------------*/
/* Set up hardware parameters                                                */
/*---------------------------------------------------------------------------*/
static int soc_at91rm9200_tabx_hw_params(struct snd_pcm_substream *substream,
                                         struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai   = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai     = rtd->dai->cpu_dai;
	int ret;

	unsigned int rate;
	int cmr_div, period;

//	pr_debug("%s enter\n", __func__);
	pr_debug("%s rate %d Hz format %x\n", __func__, params_rate(params),params_format(params));
	printk("SoC AT91RM9200 TABX hw_params: cpu_dai\n");

	/* set cpu DAI configuration:
	 *   SND_SOC_DAIFMT_I2S     = I2S mode 
	 *   SND_SOC_DAIFMT_NB_NF   = normal bit clock + frame polarity
	 *   SND_SOC_DAIFMT_CBS_CFS = SSC is master 
	 */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
						SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if(ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return(ret);
	}

	rate = params_rate(params);
	switch(rate) {
		case 8000:  	cmr_div =117;	/* BCLK = MCLK / (2*117)  =  256,410Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 11025:  	cmr_div = 85;	/* BCLK = 60Mhz/ (2* 85)  =  352,941Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 16000:  	cmr_div = 58;	/* BCLK = 60Mhz/ (2* 58)  =  517,241Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 22050: 	cmr_div = 42;	/* BCLK = 60Mhz/ (2* 42)  =  714,286Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;	
		case 32000: 	cmr_div = 29;	/* BCLK = 60Mhz/ (2* 29)  =1,034,483Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 44100: 	cmr_div = 21;	/* BCLK = 60Mhz/ (2* 21)  =1,428,571Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 48000: 	cmr_div = 19;	/* BCLK = 60Mhz/ (2* 19)  =1,578,947Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 64000:		cmr_div = 14;	/* BCLK = 60Mhz/ (2* 14)  =2,142,857Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 88200: 	cmr_div = 10;	/* BCLK = 60Mhz/ (2* 10)  =3,000,000Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		case 96000:		cmr_div =  9;	/* BCLK = 60Mhz/ (2* 58)  =3,333,333Hz */
						period  = 15;	/* LRC  = BCLK /(2*(15+1))=     8000Hz */
						break;
		default:
			printk(KERN_WARNING "unsupported rate %d on at91rm92000 board\n", rate);
			return(-EINVAL);
	}

	/* set the MCK divider for BCLK */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, ATMEL_SSC_CMR_DIV, cmr_div);
	if(ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return(ret);
	}
	switch(substream->stream) {
		case SNDRV_PCM_STREAM_PLAYBACK:
			printk(KERN_DEBUG "%s: substream->stream is SNDRV_PCM_STREAM_PLAYBACK\n", __func__); break;
		case SNDRV_PCM_STREAM_CAPTURE:
			printk(KERN_DEBUG "%s: substream->stream is SNDRV_PCM_STREAM_CAPTURE\n", __func__); break;
		default:
			printk(KERN_DEBUG "%s: substream->stream is INVALID !!!\n", __func__); break;
	}

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { /* set the BCLK divider for DACLRC */
		ret = snd_soc_dai_set_clkdiv(cpu_dai, ATMEL_SSC_TCMR_PERIOD, period);
		printk(KERN_DEBUG "%s: snd_soc_dai_set_clkdiv() --> I'm transmitter !\n", __func__);
	}
  	else {  		/* set the BCLK divider for ADCLRC */
		ret = snd_soc_dai_set_clkdiv(cpu_dai, ATMEL_SSC_RCMR_PERIOD, period);
		printk(KERN_DEBUG "%s: snd_soc_dai_set_clkdiv() --> I'm receiver !\n", __func__);
	}
	if(ret < 0) {
		printk(KERN_ERR "can't set dai system clock\n");
		return(ret);
	}
	printk("SoC AT91RM9200 TABX hw_params OK\n");
	return(0);
}

static struct snd_soc_ops soc_at91rm9200_tabx_ops = {
	.hw_params = soc_at91rm9200_tabx_hw_params,
};

static int soc_at91rm9200_tabx_init(struct snd_soc_codec *codec)
{
	pr_debug("%s enter\n", __func__);
	printk(KERN_DEBUG "SoC AT91RM9200 TABX init\n");
	return(0);
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link soc_at91rm9200_tabx_dai =
{
	.name        = "TABX_PCM",
	.stream_name = "TABX_PCM PCM",
	.cpu_dai     = &atmel_ssc_dai[1],
	.codec_dai   = &tabx_pcm_dai,
	.init        = soc_at91rm9200_tabx_init,
	.ops         = &soc_at91rm9200_tabx_ops,
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_at91rm9200_tabx =
{
	.name      = "TABX-PCM",
	.platform  = &atmel_soc_platform,
	.dai_link  = &soc_at91rm9200_tabx_dai,
	.num_links = 1,	    // ARRAY_SIZE(soc_at91rm9200_tabx_dai),
};

/* Audio subsystem */
static struct snd_soc_device soc_at91rm9200_tabx_snd_devdata =
{
	.card      = &snd_soc_at91rm9200_tabx,
    .codec_dev = &soc_codec_dev_tabx_pcm,
};

static struct platform_device *soc_at91rm9200_tabx_snd_device;

static int __init soc_at91rm9200_init(void)
{
	struct atmel_ssc_info *ssc_p_at91rm9200 = soc_at91rm9200_tabx_dai.cpu_dai->private_data;
	struct ssc_device *ssc = NULL;
	int ret;

	pr_debug("%s enter\n", __func__);

	/* Request SSC device */
	ssc = ssc_request(1);
	if(IS_ERR(ssc)) {
		printk(KERN_ERR "ASoC: Failed to request SSC 1\n");
		ret = PTR_ERR(ssc);
		ssc = NULL;
		goto err;
	}
	ssc_p_at91rm9200->ssc = ssc;
	printk("ASoC: SSC0 device request OK\n");

	/* tabx pcm codec */
	soc_at91rm9200_tabx_snd_device = platform_device_alloc("soc-audio", -1);
	if(!soc_at91rm9200_tabx_snd_device) {
		printk(KERN_ERR "ASoC: Platform device allocation failed: tabx_pcm\n");
		ret = -ENOMEM;
		goto err_ssc;
	}
	printk(KERN_DEBUG "ASoC: tabx_pcm device allocation OK\n");

	platform_set_drvdata(soc_at91rm9200_tabx_snd_device, &soc_at91rm9200_tabx_snd_devdata);
	soc_at91rm9200_tabx_snd_devdata.dev = &soc_at91rm9200_tabx_snd_device->dev;

	ret = platform_device_add(soc_at91rm9200_tabx_snd_device);
	if(ret) {
		printk(KERN_ERR "ASoC: Platform device allocation failed: tabx_pcm\n");
		ret = -ENODEV;
		goto err_ssc;
	}
//	platform_device_put(soc_at91rm9200_tabx_snd_device);

	printk(KERN_DEBUG "ASoC: tabx_pcm device registration OK\n");

	return(ret);

err_ssc:
	ssc_free(ssc);
	ssc_p_at91rm9200->ssc = NULL;
err:
	return(ret);
}

static void __exit soc_at91rm9200_exit(void)
{
	struct atmel_ssc_info *ssc_p_tabx = soc_at91rm9200_tabx_dai.cpu_dai->private_data;
	struct ssc_device *ssc;

	if(ssc_p_tabx != NULL) {
		ssc = ssc_p_tabx->ssc;
		if(ssc != NULL) ssc_free(ssc);
		ssc_p_tabx->ssc = NULL;
	}

	platform_device_unregister(soc_at91rm9200_tabx_snd_device);
	soc_at91rm9200_tabx_snd_device = NULL;
}

module_init(soc_at91rm9200_init);
module_exit(soc_at91rm9200_exit);

/* Module information */
MODULE_AUTHOR("Donnaware International LLC (donnaware.github.com)");
MODULE_DESCRIPTION("ALSA SoC TABX AT91RM9200");
MODULE_LICENSE("GPL");


/**
 * @brief demo of split mpegts file to segments for hls
 */

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavutil/avutil.h"

#define SEGMENT_DURATION 10000 //in ms
#define MAX_SEGMENT_SIZE (1024 * 1024 * 100) //简便起见，这里分配一块固定大小的内存作为缓存
static char m_seg_buf[MAX_SEGMENT_SIZE];

static int write_segment(const char *file, const char *outdir, int seq, int64_t start, int64_t end)
{
    FILE *fp_in = NULL;
    FILE *fp_out = NULL;
    char path[1024] = { 0 };
    int ret = -1;

    fp_in = fopen(file, "rb");
    if (fp_in == NULL)
    {
        printf("open input file FAILED! %s\n", file);
        goto FAIL;
    }

    snprintf(path, 1023, "%s/%d.ts", outdir, seq);
    fp_out = fopen(path, "wb");
    if (fp_out == NULL)
    {
        printf("open out file FAILED! %s\n", path);
        goto FAIL;
    }

    if (end < 0)
    {
        fseek(fp_in, 0, SEEK_END);
        end = ftell(fp_in);
    }
    if (end - start <= 0)
    {
        printf("nothing to write, start:%"PRId64", end:%"PRId64"\n", start, end);
        goto FAIL;
    }
    if (end - start > MAX_SEGMENT_SIZE)
    {
        printf("segment too large, truncated. start:%"PRId64", end:%"PRId64"\n", start, end);
        end = start - MAX_SEGMENT_SIZE;
    }

    //TODO: check IO errors
    fseek(fp_in, start, SEEK_SET);
    fread(m_seg_buf, end - start, 1, fp_in);
    fwrite(m_seg_buf, end - start, 1, fp_out);

    ret = 0;

FAIL:
    fclose(fp_in);
    fclose(fp_out);
    return ret;
}


int main(int argc, char **argv)
{
    AVFormatContext *fmt_ctx = NULL;
    AVInputFormat *iformat = NULL;
    AVRational timebase;
    int video_index = -1;
    AVPacket pkt;
    uint64_t last_pos;
    int dur_sum;
    int seq;
    int i;
    int err;

    avcodec_register_all();
    avdevice_register_all();
    av_register_all();
    avformat_network_init();

    fmt_ctx = avformat_alloc_context();
    if ((err = avformat_open_input(&fmt_ctx, argv[1], iformat, NULL)) < 0)
    {
        printf("av_open_input_file (%s) FAILED! err:%d", argv[1], err);
        goto FAIL;
    }

    if ((err = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        printf("av_find_stream_info FAILED! err:%d", err);
        goto FAIL;
    }

    if (fmt_ctx->nb_programs > 1)
    {
        printf("nb_programs:%d\n", fmt_ctx->nb_programs);
        goto FAIL;
    }

    for (i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index = i;
            timebase = fmt_ctx->streams[i]->time_base;
            printf("video stream_index:%d, time_base.num:%d, time_base.den:%d\n", video_index, timebase.num, timebase.den);
            break;
        }
    }
    if (video_index == -1)
    {
        printf("no video stream found\n");
        goto FAIL;
    }

    av_init_packet(&pkt);
    last_pos = 0;
    dur_sum = 0;
    seq = 10000; //为了便于用cat程序合并测试，这里从10000开始

    for (i = 0; ; i++)
    {
        if (av_read_frame(fmt_ctx, &pkt) != 0)
            break;
        if (pkt.stream_index == video_index)
        {
            if ((pkt.flags & AV_PKT_FLAG_KEY) && dur_sum >= SEGMENT_DURATION)
            {
                write_segment(argv[1], argv[2], seq, last_pos, pkt.pos);
                dur_sum = 0;
                last_pos = pkt.pos;
                seq += 1;
            }
            dur_sum += (pkt.duration * 1000 * timebase.num / timebase.den);
        }
    }

    //the last segment
    write_segment(argv[1], argv[2], seq, last_pos, -1);

    avformat_close_input(&fmt_ctx);
    return 0;

FAIL:
    avformat_close_input(&fmt_ctx);
    return -1;
}


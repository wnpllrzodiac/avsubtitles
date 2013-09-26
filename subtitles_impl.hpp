//
// subtitles_impl.h
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
//

#ifndef AV_SUBTITLES_IMPL_HPP
#define AV_SUBTITLES_IMPL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <vector>
#include <map>
#include <string>

#ifdef AV_SUBTITLES_USE_THREAD
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/bind.hpp>
#include <list>
#endif

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#include "libavutil/time.h"
#include "enca.h"
#include "ass.h"
#include "png.h"
}
#include "iconv.h"

class subtitles_impl
{
public:
	subtitles_impl(void);
	~subtitles_impl(void);

public:
	// filename 打开一个字幕(或内含字幕流的视频)文件.
	// width 指定将来渲染画面的宽.
	// height 指定将来渲染画面的高.
	// index 字幕流索引, 默认第1个字幕流.
	// 打开成功返回true, 失败返回false.
	bool open_subtilte(const std::string& filename, int width, int height, int index = 0);

	// 返回字幕列表, 在一些文件中, 可能包含多个字幕流.
	std::vector<std::string> subtitle_list();

	// 渲染一帧字幕到YUV420图片上.
	// yuv420_data 指定的yuv420数据, 必须按
	// planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)编码.
	// 时间戳, 单位ms(毫秒).
	bool subtitle_do(void* yuv420_data, long long time_stamp);

	// 关闭字幕.
	void close();

	// 修改时间偏移.
	// offset 表示时间偏移, +向前, -向后. 单位ms(毫秒).
	void time_offset(long long offset)
	{
		m_time_offset = offset;
	}

	// 设置字体文件.
	void set_font(const std::string& font)
	{
		if (font.empty())
			m_used_fontconfig = true;
		else
		{
			m_user_font = font;
			m_used_fontconfig = false;
		}
	}

private:
	static void static_msg_callback(int level, const char* fmt, va_list va, void *data);
	static int decode_interrupt_cb(void* ctx);
	static int read_data(void* opaque, uint8_t* buf, int buf_size);
	static int write_data(void* opaque, uint8_t* buf, int buf_size);
	static int64_t seek_data(void* opaque, int64_t offset, int whence);

	inline bool render_frame(void* yuv420_data,
		AVSubtitle& sub, int64_t& pts, int64_t& time, int64_t& duration);

	int seek_file(int64_t& time);
	int read_frame(AVPacket *pkt, int64_t& time);

#ifdef AV_SUBTITLES_USE_THREAD
	void read_thread();
#endif

private:
	// 用于打开字幕文件, 无论是ass/ssa/srt/dvdsub格
	// 式, 均使用AVFormatContext此打开, 这样在后面可
	// 以方便seek、解码等操作.
	AVFormatContext* m_format;

	// 只用于字幕解码context.
	AVCodecContext* m_codec_ctx;

	// 保存字幕AVStream.
	std::vector<AVStream*> m_streams;

	// 已经被处理过的帧.
	std::map<uint32_t, int64_t> m_expired;

	// 数据IO上下文指针.
	AVIOContext *m_avio_ctx;

	// IO数据缓冲.
	unsigned char *m_io_buffer;

#ifdef AV_SUBTITLES_USE_THREAD
	bool m_abort;
	boost::thread m_thread;
	boost::mutex m_mutex;
	bool m_req_seek;
#endif

	int64_t m_seek_point;
	std::map<int64_t, AVPacket> m_cached;

	// 是否使用fontconfig.
	bool m_used_fontconfig;

	// 用户指定的字体.
	std::string m_user_font;

	// 时间偏移.
	int64_t m_time_offset;

	// 读取偏移.
	int64_t m_offset;

	// 视频画面宽.
	int m_width;

	// 视频画面高.
	int m_height;

	// 用户选择的字幕流索引.
	int m_index;

	// 是否使用ass渲染字幕.
	bool m_use_ass;

	// 内存字幕.
	bool m_memory_ass;

	// 字幕缓冲.
	char* m_subtitle_buf;

	// 字幕缓冲大小.
	size_t m_subtitle_buf_sz;

	// ass handle.
	ass_library* m_ass_library;

	// ass字幕渲染器.
	ASS_Renderer* m_ass_renderer;

	// ass字幕track.
	ASS_Track* m_ass_track;
};


#endif // AV_SUBTITLES_IMPL_HPP

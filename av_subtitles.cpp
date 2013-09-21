#include "av_subtitles.hpp"
#include "subtitles_impl.hpp"

av_subtitles::av_subtitles(void)
	: m_impl(new subtitles_impl())
{
}

av_subtitles::~av_subtitles(void)
{
	delete m_impl;
}

bool av_subtitles::open_subtilte(const std::string& filename, int width, int height, int index /*= 0*/)
{
	return m_impl->open_subtilte(filename, width, height, index);
}

std::vector<std::string> av_subtitles::subtitle_list()
{
	return m_impl->subtitle_list();
}

void av_subtitles::subtitle_do(void* yuv420_data, long long time_stamp)
{
	return m_impl->subtitle_do(yuv420_data, time_stamp);
}

void av_subtitles::close()
{
	m_impl->close();
}

void av_subtitles::set_font(const std::string& font)
{
	m_impl->set_font(font);
}

#include "av_subtitles.h"

#ifdef __cplusplus
extern "C" {
#endif

// C接口导出.
// 分配一个av_subtitle_handle句柄.
// 必须使用free_subtitle来释放.
av_subtitle_handle alloc_subtitle()
{
	av_subtitle_handle h = (av_subtitle_handle)new av_subtitles();
	return h;
}

// 释放分配的av_subtitle_handle.
// handle 必须是由alloc_subtitle分配的av_subtitle_handle.
void free_subtitle(av_subtitle_handle handle)
{
	av_subtitles* h = (av_subtitles*)handle;
	delete h;
}

// handle 已经分配好的av_subtitle_handle.
// filename 打开一个字幕(或内含字幕流的视频)文件.
// width 指定将来渲染画面的宽, 必须和视频原宽一至.
// height 指定将来渲染画面的高, 必须和视频原高一至.
// index 字幕流索引, 默认第1个字幕流.
// 打开成功返回0, 失败返回非0.
int open_subtitle(av_subtitle_handle handle,
	const char* filename, int width, int height, int index)
{
	av_subtitles* h = (av_subtitles*)handle;
	return h->open_subtilte(filename, width, height, index) ? 0 : -1;
}

// 渲染一帧字幕到YUV420图片上.
// handle 已经分配好的av_subtitle_handle.
// yuv420_data 指定的yuv420数据, 必须按
//  planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)编码.
//  宽和高在open_subtilte被指定.
// time_stamp 时间戳, 单位ms(毫秒).
void subtitle_do(av_subtitle_handle handle,
	void* yuv420_data, long long time_stamp)
{
	av_subtitles* h = (av_subtitles*)handle;
	h->subtitle_do(yuv420_data, time_stamp);
}

// 关闭字幕.
// handle 指定被open_subtitle打开的handle.
void close_subtitle(av_subtitle_handle handle)
{
	av_subtitles* h = (av_subtitles*)handle;
	h->close();
}

// 指定字体文件, 在调用open_subtilte前设置.
// font 为指定的字体文件完整路径.
void set_font(av_subtitle_handle handle, const char* font)
{
	av_subtitles* h = (av_subtitles*)handle;
	h->set_font(font);
}

#ifdef __cplusplus
}
#endif

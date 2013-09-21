//
// av_subtitles.h
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//

#ifndef AV_SUBTITLES_H
#define AV_SUBTITLES_H

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#ifdef _MSC_VER
#	include <windows.h>
#	define inline
#	define __CRT__NO_INLINE
#	ifdef API_EXPORTS
#		define EXPORT_API __declspec(dllexport)
#	else
#		define EXPORT_API __declspec(dllimport)
#	endif
#else
#	define EXPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* av_subtitle_handle;

// C接口导出.
// 分配一个av_subtitle_handle句柄.
// 必须使用free_subtitle来释放.
EXPORT_API av_subtitle_handle alloc_subtitle();

// 释放分配的av_subtitle_handle.
// handle 必须是由alloc_subtitle分配的av_subtitle_handle.
EXPORT_API void free_subtitle(av_subtitle_handle handle);

// handle 已经分配好的av_subtitle_handle.
// filename 打开一个字幕(或内含字幕流的视频)文件.
// width 指定将来渲染画面的宽, 必须和视频原宽一至.
// height 指定将来渲染画面的高, 必须和视频原高一至.
// index 字幕流索引, 默认第1个字幕流.
// 打开成功返回0, 失败返回非0.
// !!!注意, 首次调用open将会自动使用fontconfig缓冲字体, 缓冲估计需要几分钟,
// 会在执行目录下创建fonts-cache的目录, 你可以在open_subtitle前手工指定
// 字体文件调用set_font, 将禁用fontconfig, 以避免cache字体长时间阻塞.
EXPORT_API int open_subtitle(av_subtitle_handle handle,
	const char* filename, int width, int height, int index);

// 渲染一帧字幕到YUV420图片上.
// handle 已经打开的av_subtitle_handle.
// yuv420_data 指定的yuv420数据, 必须按
//  planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)编码.
//  宽和高在open_subtilte被指定.
// time_stamp 时间戳, 单位ms(毫秒).
EXPORT_API void subtitle_do(av_subtitle_handle handle,
	void* yuv420_data, long long time_stamp);

// 关闭字幕.
// handle 指定被open_subtitle打开的handle.
EXPORT_API void close_subtitle(av_subtitle_handle handle);

// 指定字体文件, 在调用open_subtilte前设置.
// handle 已经分配好的av_subtitle_handle.
// font 为指定的字体文件完整路径.
EXPORT_API void set_font(av_subtitle_handle handle, const char* font);

#ifdef __cplusplus
}
#endif

#endif // AV_SUBTITLES_H

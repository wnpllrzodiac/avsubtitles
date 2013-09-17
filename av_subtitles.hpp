//
// av_subtitles.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//

#ifndef AV_SUBTITLES_HPP
#define AV_SUBTITLES_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <vector>
#include <string>

class subtitles_impl;
class av_subtitles // : noncopyable
{
public:
	av_subtitles(void);
	virtual ~av_subtitles(void);

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
	// yuv420_data 指定的yuv420数据, 必须按planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)编码.
	// 时间戳, 单位ms(毫秒).
	void subtitle_do(void* yuv420_data, long long time_stamp);

	// 关闭字幕.
	void close();

protected:
	av_subtitles(const av_subtitles&);
	av_subtitles& operator=(const av_subtitles&);

private:
	subtitles_impl *m_impl;
};

#endif // AV_SUBTITLES_HPP

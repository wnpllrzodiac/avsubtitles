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

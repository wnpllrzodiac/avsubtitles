#include <iostream>
#include <algorithm>
#include <assert.h>
#include "subtitles_impl.hpp"

#if defined(_MSC_VER) && defined(WIN32)

#include <windows.h>

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")

#pragma comment(lib, "libass.a")
#pragma comment(lib, "libpng16.a")
#pragma comment(lib, "libxml2.a")

#pragma comment(lib, "libiconv.a")
#pragma comment(lib, "libfreetype.a")
#pragma comment(lib, "libfontconfig.a")
#pragma comment(lib, "libenca.a")
#pragma comment(lib, "libz.a")
#pragma comment(lib, "libfribidi.a")
#pragma comment(lib, "libmingwex.a")
#pragma comment(lib, "libmingw32.a")
#pragma comment(lib, "libgcc.a")
#pragma comment(lib, "ws2_32.lib")

#define strcasecmp lstrcmpiA

#endif // _MSC_VER

#define IO_BUFFER_SIZE	32768

#define _A(c)  ((c)>>24)
#define _B(c)  (((c)>>16)&0xFF)
#define _G(c)  (((c)>>8)&0xFF)
#define _R(c)  ((c)&0xFF)

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

#define rgba2y(c)  ( (( 263*_r(c) + 516*_g(c) + 100*_b(c)) >> 10) + 16  )
#define rgba2u(c)  ( ((-152*_r(c) - 298*_g(c) + 450*_b(c)) >> 10) + 128 )
#define rgba2v(c)  ( (( 450*_r(c) - 376*_g(c) -  73*_b(c)) >> 10) + 128 )

#define abgr2y(c)  ( (( 263*_R(c) + 516*_G(c) + 100*_B(c)) >> 10) + 16  )
#define abgr2u(c)  ( ((-152*_R(c) - 298*_G(c) + 450*_B(c)) >> 10) + 128 )
#define abgr2v(c)  ( (( 450*_R(c) - 376*_G(c) -  73*_B(c)) >> 10) + 128 )

#define MAX_TRANS   255
#define TRANS_BITS  8

typedef struct yuv_image_t {
	int width, height;
	unsigned char *buffer;      // Yuv image
} yuv_image;

static
void write_png(char *fname, char* buffer, int width, int height, int ppb)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;
	int k;

	png_ptr =
		png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);
	fp = NULL;

	fp = fopen(fname, "wb");
	if (fp == NULL) {
		printf("PNG Error opening %s for writing!\n", fname);
		return;
	}

	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, 0);

	png_set_IHDR(png_ptr, info_ptr, width, height,
		8, ppb == 24 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	png_set_bgr(png_ptr);
	int stride = ((ppb / 8) * width);
	row_pointers = (png_byte **) malloc(height * sizeof(png_byte *));
	for (k = 0; k < height; k++)
		row_pointers[k] = (unsigned char *)buffer + stride * k;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row_pointers);

	fclose(fp);
}

static
void blend_subrect_yuv420(yuv_image& dest, const AVSubtitleRect *rect)
{
	uint32_t *pal;
	uint8_t *dsty, *dstu, *dstv;
	uint8_t* src, *src2;

	pal = (uint32_t*)rect->pict.data[1];
	
	// Y份量的开始位置.
	dsty = dest.buffer + rect->y * dest.width + rect->x;
	dstu = dest.buffer + dest.width * dest.height
		+ ((rect->y / 2) * (dest.width / 2)) + rect->x / 2;
	dstv = dest.buffer + dest.width * dest.height + ((dest.width / 2) * (dest.height / 2))
		+((rect->y / 2) * (dest.width / 2)) + rect->x / 2;
	
	src = rect->pict.data[0];

	bool b_even_scanline = rect->w % 2;
	// alpha融合YUV各分量.
	for (int i = 0; i < rect->h
		; i++
		, dsty += dest.width
		, dstu += b_even_scanline ? dest.width / 2 : 0
		, dstv += b_even_scanline ? dest.width / 2 : 0
		)
	{
		src2 = src;
		b_even_scanline = !b_even_scanline;
		for (int j = 0; j < rect->w; j++)
		{
			uint32_t color = pal[*(src2++)];
			unsigned char cy = abgr2y(color);
			unsigned char cu = abgr2u(color);
			unsigned char cv = abgr2v(color);
			unsigned opacity = _A(color);
			if (!opacity) continue;
			if (opacity == MAX_TRANS)
			{
				dsty[j] = cy;
				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = cu;
					dstv[j / 2] = cv;
				}
			}
			else
			{
				dsty[j] = (cy * opacity + dsty[j] * (MAX_TRANS - opacity)) >> TRANS_BITS;
				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = (cu * opacity + dstu[j / 2] * (MAX_TRANS - opacity)) >> TRANS_BITS;
					dstv[j / 2] = (cv * opacity + dstv[j / 2] * (MAX_TRANS - opacity)) >> TRANS_BITS;
				}
			}
		}
		src += rect->pict.linesize[0];
	}
}

static
char *read_file(const char* fname, size_t* bufsize)
{
	int res;
	long sz;
	long bytes_read;
	char *buf;

	FILE *fp = fopen(fname, "rb");
	if (!fp) {
		return 0;
	}
	res = fseek(fp, 0, SEEK_END);
	if (res == -1) {
		fclose(fp);
		return 0;
	}

	sz = ftell(fp);
	rewind(fp);

	buf = (char*)malloc(sz + 1);
	assert(buf);
	bytes_read = 0;
	do {
		res = fread(buf + bytes_read, 1, sz - bytes_read, fp);
		if (res <= 0) {
			fclose(fp);
			free(buf);
			return 0;
		}
		bytes_read += res;
	} while (sz - bytes_read > 0);
	buf[sz] = '\0';
	fclose(fp);

	if (bufsize)
		*bufsize = sz;
	return buf;
}

static
char* guess_buffer_cp(const char *buffer,
	int buflen, char *preferred_language, char *fallback)
{
	const char **languages;
	size_t langcnt;
	EncaAnalyser analyser;
	EncaEncoding encoding;
	char *detected_sub_cp = NULL;
	int i;

	languages = enca_get_languages(&langcnt);
	std::cout << "ENCA supported languages\n";
	for (i = 0; i < langcnt; i++)
		std::cout << "lang " << languages[i] << std::endl;

	for (i = 0; i < langcnt; i++) {
		const char *tmp;
		if (strcasecmp(languages[i], preferred_language) != 0)
			continue;
		analyser = enca_analyser_alloc(languages[i]);
		encoding = enca_analyse_const(analyser, (const unsigned char*)buffer, buflen);
		tmp = enca_charset_name(encoding.charset, ENCA_NAME_STYLE_ICONV);
		if (tmp && encoding.charset != ENCA_CS_UNKNOWN) {
			detected_sub_cp = strdup(tmp);
			std::cout << "ENCA detected charset:" << tmp << std::endl;
		}
		enca_analyser_free(analyser);
	}

	free(languages);

	if (!detected_sub_cp) {
		detected_sub_cp = strdup(fallback);
		std::cout << "ENCA detection failed: fallback to " << fallback << std::endl;
	}

	return detected_sub_cp;
}

static
char *sub_recode(char *data, size_t size, char *codepage)
{
	iconv_t icdsc;
	char *tocp = "UTF-8";
	char *outbuf;
	assert(codepage);

	{
		const char *cp_tmp = codepage;
		char enca_lang[3], enca_fallback[100];
		// enca:zh:cp936
		if (sscanf(codepage, "enca:%2s:%99s", enca_lang, enca_fallback) == 2
			|| sscanf(codepage, "ENCA:%2s:%99s", enca_lang,
			enca_fallback) == 2)
		{
			cp_tmp = guess_buffer_cp((const char *)data, size, enca_lang, enca_fallback);
		}

		if ((icdsc = iconv_open(tocp, cp_tmp)) != (iconv_t) (-1)) {
			std::cout << "Opened iconv descriptor" << std::endl;
		} else
			std::cout << "Error opening iconv descriptor" << std::endl;
	}

	{
		size_t osize = size;
		size_t ileft = size;
		size_t oleft = size - 1;
		char *ip;
		char *op;
		size_t rc;
		int clear = 0;

		outbuf = (char*)malloc(osize);
		assert(outbuf);
		ip = data;
		op = outbuf;

		while (1) {
			if (ileft)
				rc = iconv(icdsc, &ip, &ileft, &op, &oleft);
			else {              // clear the conversion state and leave
				clear = 1;
				rc = iconv(icdsc, NULL, NULL, &op, &oleft);
			}
			if (rc == (size_t) (-1)) {
				if (errno == E2BIG) {
					size_t offset = op - outbuf;
					outbuf = (char *) realloc(outbuf, osize + size);
					op = outbuf + offset;
					osize += size;
					oleft += size;
				} else {
					std::cout << "Error recoding file" << std::endl;
					free(outbuf);
					outbuf = NULL;
					goto out;
				}
			} else if (clear)
				break;
		}
		outbuf[osize - oleft - 1] = 0;
	}

out:
	if (icdsc != (iconv_t) (-1)) {
		(void) iconv_close(icdsc);
		icdsc = (iconv_t) (-1);
		std::cout << "Closed iconv descriptor" << std::endl;
	}

	return outbuf;
}

static
char *read_file_recode(const char *fname, char *codepage, size_t *size)
{
	char *buf;
	size_t bufsize;

	buf = read_file(fname, &bufsize);
	if (!buf)
		return 0;
	if (codepage) {
		char *tmpbuf = sub_recode(buf, bufsize, codepage);
		free(buf);
		buf = tmpbuf;
	}
	if (!buf)
		return 0;
	*size = bufsize;
	return buf;
}

static
std::string extension(std::string const& f)
{
	char const* ext = strrchr(f.c_str(), '.');
	if (ext == 0) return "";
	return ext;
}

static
void blend_single(yuv_image& frame, ASS_Image* img)
{
	unsigned char cy = rgba2y(img->color);
	unsigned char cu = rgba2u(img->color);
	unsigned char cv = rgba2v(img->color);
	unsigned opacity = 255 - _a(img->color);
	unsigned char *src, *dsty, *dstu, *dstv;

	src = img->bitmap;
	dsty = frame.buffer + img->dst_y * frame.width + img->dst_x;
	dstu = frame.buffer + frame.width * frame.height +
		(img->dst_y / 2) * (frame.width / 2) + img->dst_x / 2;
	dstv = frame.buffer + frame.width * frame.height + ((frame.width / 2) * (frame.height / 2)) +
		((img->dst_y / 2) * (frame.width / 2)) + img->dst_x / 2;

	bool b_even_scanline = img->dst_y % 2;

	for (int i = 0; i < img->h; i++
		, dsty += frame.width
		, dstu += b_even_scanline ? frame.width / 2 : 0
		, dstv += b_even_scanline ? frame.width / 2 : 0
		)
	{
		b_even_scanline = !b_even_scanline;
		for (int j = 0; j < img->w; ++j)
		{
			unsigned int k = src[j] * opacity / 255.0f;
			if (!k) continue;
			if (k == MAX_TRANS)
			{
				dsty[j] = cy;
				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = cu;
					dstv[j / 2] = cv;
				}
			}
			else
			{
				dsty[j] = (cy * k + dsty[j] * (MAX_TRANS - k)) >> TRANS_BITS;
				if (b_even_scanline && j % 2 == 0)
				{
					dstu[j / 2] = (cu * k + dstu[j / 2] * (MAX_TRANS - k)) >> TRANS_BITS;
					dstv[j / 2] = (cv * k + dstv[j / 2] * (MAX_TRANS - k)) >> TRANS_BITS;
				}
			}
		}
		src += img->stride;
	}
}

static
void render_subtitle_frame(yuv_image& frame, ASS_Image* img)
{
	int cnt = 0;
	while (img) {
		blend_single(frame, img);
		++cnt;
		img = img->next;
	}
#ifdef DEBUG
	printf("%d images blended\n", cnt);
#endif // DEBUG
}

void subtitles_impl::static_msg_callback(int level, const char* fmt, va_list va, void *data)
{
	subtitles_impl* this_ = (subtitles_impl*)data;
	if (level > 6)
		return;
	char buffer[4096] = { 0 };
	vsprintf(buffer, fmt, va);
	std::string msg = "ass: " + std::string(buffer) + "\n";
	std::cout << msg;
}

int subtitles_impl::decode_interrupt_cb(void *ctx)
{
	subtitles_impl* this_ = (subtitles_impl*)ctx;
	// return (int)subtitles_impl->m_abort;
	// return demux->is_abort();
	return 0;
}

int subtitles_impl::read_data(void *opaque, uint8_t *buf, int buf_size)
{
	subtitles_impl* this_ = (subtitles_impl*)opaque;
	// 修正最大返回值.
	if (this_->m_offset + buf_size > this_->m_subtitle_buf_sz)
		buf_size = this_->m_subtitle_buf_sz - this_->m_offset;
	// 复制数据.
	memcpy(buf, this_->m_subtitle_buf + this_->m_offset, buf_size);
	// 重新计算偏移.
	this_->m_offset += buf_size;
	// 返回缓冲大小.
	return buf_size;
}

int subtitles_impl::write_data(void *opaque, uint8_t *buf, int buf_size)
{
	subtitles_impl* this_ = (subtitles_impl*)opaque;
	assert(0);
	return 0;
}

int64_t subtitles_impl::seek_data(void *opaque, int64_t offset, int whence)
{
	subtitles_impl* this_ = (subtitles_impl*)opaque;
	switch (whence)
	{
	case SEEK_SET:	// 文件起始位置计算.
		this_->m_offset = offset;
		break;
	case SEEK_CUR:	// 文件指针当前位置开始计算.
		this_->m_offset += offset;
		break;
	case SEEK_END:	// 文件尾开始计算.
		this_->m_offset = this_->m_subtitle_buf_sz - offset;
		break;
	case AVSEEK_SIZE:
		return this_->m_subtitle_buf_sz;
		break;
	}
	return this_->m_offset;
}

subtitles_impl::subtitles_impl(void)
{
	// 式, 均使用AVFormatContext此打开, 这样在后面可
	// 以方便seek、解码等操作.
	m_format = NULL;

	// 只用于字幕解码context.
	m_codec_ctx = NULL;

	// 数据IO上下文指针.
	m_avio_ctx = NULL;

	// IO缓冲指针.
	m_io_buffer = NULL;

	// 是否使用fontconfig.
	m_used_fontconfig = true;

	// 视频画面宽.
	m_width = -1;

	// 视频画面高.
	m_height = -1;

	// 用户选择的字幕流索引.
	m_index = 0;

	// 是否使用ass渲染字幕.
	m_use_ass = false;

	// 内存字幕.
	m_memory_ass = false;

	// 字幕缓冲.
	m_subtitle_buf = NULL;

	// 字幕缓冲大小.
	m_subtitle_buf_sz = 0;

	// ass handle.
	m_ass_library = NULL;

	// ass字幕渲染器.
	m_ass_renderer = NULL;

	// ass字幕track.
	m_ass_track = NULL;

	av_register_all();
	avcodec_register_all();
}

subtitles_impl::~subtitles_impl(void)
{
	close();
}

bool subtitles_impl::open_subtilte(const std::string& filename, int width, int height, int index/*= 0*/)
{
	// 先关闭已经打开的字幕.
	close();

	// 获得文件名后辍, 如果是常用的文本字幕格式, 由于FFmpeg并没有帮我们正确处理一些
	// 文本编码, 所以我们将其转码成我们需要的文本编码格式, 并保存在内存中, 以便后面
	// FFmpeg正确解码处理, 以下代码来源于伟大的libass, 需要注意的是需要启用libiconv和
	// libenca.
	if (extension(filename) == ".ass"
		|| extension(filename) == ".ssa"
		|| extension(filename) == ".srt"
		|| extension(filename) == ".txt")
	{
		// 默认为enca:zh:cp936.
		m_subtitle_buf = read_file_recode(filename.c_str(), "enca:zh:cp936", &m_subtitle_buf_sz);
		if (!m_subtitle_buf)
			return false;
		m_memory_ass = true;
	}

	// 保存宽高.
	m_width = width;
	m_height = height;
	int ret = 0;

	if (m_memory_ass)
	{
		// 打开字幕文件.
		m_format = avformat_alloc_context();
		// 设置参数.
		// m_format->flags = AVFMT_FLAG_GENPTS;
		m_format->interrupt_callback.callback = decode_interrupt_cb;
		m_format->interrupt_callback.opaque = (void*)this;
		// 注意它的内存释放, 由avio_close负责释放, 又由avformat_close_input调用.
		m_io_buffer = (unsigned char*)av_malloc(IO_BUFFER_SIZE);
		assert(m_io_buffer);
		m_avio_ctx = avio_alloc_context(m_io_buffer,
			IO_BUFFER_SIZE, 0, (void*)this, read_data, NULL, seek_data);
		assert(m_avio_ctx);
		m_avio_ctx->write_flag = 0;
		AVInputFormat *iformat = NULL;
		ret = av_probe_input_buffer(m_avio_ctx, &iformat, "", NULL, 0, 0);
		m_format->pb = m_avio_ctx;
		ret = avformat_open_input(&m_format, "", iformat, NULL);
		if (ret)
			return false;
	}
	else
	{
		// 使用FFmpeg直接打开.
		 ret = avformat_open_input(&m_format, filename.c_str(), NULL, NULL);
		 if (ret < 0)
			 return false;
	}

	// 查找媒体流信息.
	ret = avformat_find_stream_info(m_format, NULL);
	if (ret < 0)
		return false;
#if 0
	// 查找媒体流信息.
	ret = av_find_best_stream(m_format, AVMEDIA_TYPE_SUBTITLE, -1, -1, NULL, 0);
	if (ret < 0)
		return false;
#endif
	m_index = index;
	unsigned int i;
	for (i = 0; (unsigned int) i < m_format->nb_streams; i++)
	{
		if (m_format->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE)
			m_streams.push_back(m_format->streams[i]);
	}
	// 所选择的索引号不满足条件, 返回失败.
	if (!(m_index >= 0 && m_index < (int)m_streams.size()))
		return false;
	// 查找字幕解码器.
	m_codec_ctx = m_streams[m_index]->codec;
	AVCodec* dec = avcodec_find_decoder(m_codec_ctx->codec_id);
	const AVCodecDescriptor* dec_desc = avcodec_descriptor_get(m_codec_ctx->codec_id);
	if (dec_desc->id == AV_CODEC_ID_SSA
		|| dec_desc->id == AV_CODEC_ID_SUBRIP
		|| dec_desc->id == AV_CODEC_ID_SSA)
	{
		m_use_ass = true;
		// 使用ass渲染字幕, 初始化ass字幕渲染器.
		m_ass_library = ass_library_init();
		ass_set_message_cb(m_ass_library,
			subtitles_impl::static_msg_callback, (void*)this);
		m_ass_renderer = ass_renderer_init(m_ass_library);
		m_ass_track = ass_new_track(m_ass_library);
		ass_set_frame_size(m_ass_renderer, m_width, m_height);
		ass_set_margins(m_ass_renderer, 0, 0, 0, 0);
		ass_set_use_margins(m_ass_renderer, 0);
		ass_set_hinting(m_ass_renderer, ASS_HINTING_LIGHT);
		ass_set_font_scale(m_ass_renderer, 1.0);
		ass_set_line_spacing(m_ass_renderer, 0.0);
		// 如果没有字体缓存, 将更新, 第1次这里可能阻塞几分钟, 当然也可以指定不使用fontconfig,
		// 而指定具体的字体, 这样就不会阻塞在这里, 但这里是使用fontconfig, 主要便于移植到
		// 非windows系统.
		if (m_used_fontconfig || m_user_font.empty())
			ass_set_fonts(m_ass_renderer, NULL, "Arial", 1, NULL, 1);
		else
			ass_set_fonts(m_ass_renderer, m_user_font.c_str(), NULL, 0, NULL, 0);
	}
	else if (dec_desc->id == AV_CODEC_ID_DVD_SUBTITLE
		|| dec_desc->id == AV_CODEC_ID_DVB_SUBTITLE)
	{
		m_use_ass = false;
	}
	else
	{
		// 暂时不支持的格式.
		return false;
	}

	// 尝试打开字幕解码器.
	ret = avcodec_open2(m_codec_ctx, dec, NULL);
	if (ret < 0)
		return false;
	const char* header = (const char*)m_codec_ctx->subtitle_header;
	const int header_size = m_codec_ctx->subtitle_header_size;
	if (header && header > 0)
	{
		// 输出字幕解码信息.
#ifdef DEBUG
		std::cout.write(header, header_size);
#endif
		if (m_use_ass)
		{
			// 开始使用ass解码ass格式数据.
			ass_process_data(m_ass_track, (char*)header, header_size);
		}
	}
	else if (m_use_ass)
	{
		assert(0);
		return false;
	}

	return true;
}

std::vector<std::string> subtitles_impl::subtitle_list()
{
	std::vector<std::string> ret;
	if (m_codec_ctx && m_streams.size() != 0)
	{
		for (std::vector<AVStream*>::iterator i = m_streams.begin();
			i != m_streams.end(); i++)
		{
			enum AVCodecID& id = (**i).codec->codec_id;
			const AVCodecDescriptor* dec_desc = avcodec_descriptor_get(id);
			ret.push_back(dec_desc->name);
		}
	}
	return ret;
}

void subtitles_impl::subtitle_do(void* yuv420_data, long long time_stamp)
{
	// 跳转到指定时间开始读取字幕.
	int64_t time = (double)time_stamp / 1000.0f * AV_TIME_BASE;
	if (avformat_seek_file(m_format, -1, INT64_MIN, time, INT64_MAX, 0) < 0)
		return;
	AVPacket pkt;
	av_init_packet(&pkt);
	AVSubtitle sub = { 0 };
	long long base_time;
	long long duration;
	while (av_read_frame(m_format, &pkt) >= 0)
	{
		base_time = (pkt.pts * av_q2d(m_codec_ctx->time_base)) * 1000.0f;
		duration = pkt.duration * av_q2d(m_codec_ctx->time_base) * 1000.0f;
		int got_subtitle;
		if (pkt.stream_index == m_streams[m_index]->index)
		{
			// 不在时间范围内, 返回.
			if (!(time_stamp >= base_time && time_stamp < base_time + duration))
				return;
			int ret = avcodec_decode_subtitle2(m_codec_ctx, &sub, &got_subtitle, &pkt);
			if (ret < 0)
				return;
			else if (got_subtitle)
			{
				if (m_use_ass)
				{
					for (unsigned i = 0; i < sub.num_rects; i++)
					{
						char *ass_line = sub.rects[i]->ass;
						if (!ass_line)
							break;
						if (m_expired.find(pkt.pts) == m_expired.end())
						{
							m_expired.insert(pkt.pts);
							ass_process_data(m_ass_track, ass_line, strlen(ass_line));
						}
						base_time = base_time + duration / 2;
						ASS_Image *img =
							ass_render_frame(m_ass_renderer, m_ass_track, base_time, NULL);
						yuv_image yuv_img;
						yuv_img.width = m_width;
						yuv_img.height = m_height;
						yuv_img.buffer = (unsigned char *)yuv420_data;
						render_subtitle_frame(yuv_img, img);
					}
				}
				else
				{
					// 直接从FFmpeg渲染到YUV.
					if (sub.format == 0)
					{
						for (int i = 0; i < sub.num_rects; i++)
						{
							AVSubtitleRect *rect = sub.rects[i];
							if (rect->type != SUBTITLE_BITMAP)
								continue;
							yuv_image yuv_img;
							yuv_img.width = m_width;
							yuv_img.height = m_height;
							yuv_img.buffer = (unsigned char *)yuv420_data;
							blend_subrect_yuv420(yuv_img, rect);
						}
					}
				}
			}
		}
		av_free_packet(&pkt);
		avsubtitle_free(&sub);
	}
}

void subtitles_impl::close()
{
	if (m_format)
	{
		avformat_close_input(&m_format);
	}

	if (m_avio_ctx)
	{
		av_freep(&m_avio_ctx);
	}

	m_streams.clear();
	m_width = -1;
	m_height = -1;
	m_offset = 0;
	m_memory_ass = false;
	m_use_ass = false;
	m_used_fontconfig = true;
	m_user_font = "";

	if (m_subtitle_buf)
	{
		free(m_subtitle_buf);
		m_subtitle_buf = NULL;
	}

	if (m_ass_track)
	{
		ass_free_track(m_ass_track);
		m_ass_track = NULL;
	}

	if (m_ass_renderer)
	{
		ass_renderer_done(m_ass_renderer);
		m_ass_renderer = NULL;
	}

	if (m_ass_library)
	{
		ass_library_done(m_ass_library);
		m_ass_library = NULL;
	}
}

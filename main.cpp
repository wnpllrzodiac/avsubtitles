//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//

#include <vector>
#include <iostream>
#include "av_subtitles.hpp"

// 使用字幕插件示例.
int main(int argc, char** argv)
{
	if (argc != 5)
	{
		std::cout << "usage: " << argv[0]
		<< "<subtilte/video> <time> <video_width> <video_frame>" << std::endl;
	}

	char* filename = strdup(argv[1]);
	int time = atol(argv[2]);
	int frame_w = atol(argv[3]);
	int frame_h = atol(argv[4]);

	av_subtitles s;
	s.open_subtilte(filename, frame_w, frame_h, 2);
	int buffer_size = frame_w * frame_h * 3 / 2;
	char* buffer = (char*)malloc(buffer_size);
	s.subtitle_do(buffer, time);

	FILE* fp = fopen("test.yuv", "w+b");
	fwrite(buffer, buffer_size, 1, fp);
	fclose(fp);
	free(buffer);

	return 0;
}

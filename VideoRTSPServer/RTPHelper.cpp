#include "RTPHelper.h"
#include <Windows.h>
#define RTP_MAX_SIZE 1300

/* 发送流媒体数据 */
// Param1: rtp 数据包
// Param2: h.264 数据帧
// Param3: 发送至的客户端
int RTPHelper::SendMediaFrame(RTPFrame& rtpframe, EBuffer& frame, const EAddress& client)
{
	size_t frame_size = frame.size();
	int sepsize = GetFrameSepSize(frame);
	frame_size -= sepsize;       // frame 实际的大小
	BYTE* pFrame = sepsize + (BYTE*)frame;  // 偏移 sepsize 个位置，才到这个帧真正的开始
	if (frame_size > RTP_MAX_SIZE) {//分片
		BYTE nalu = pFrame[0] & 0x1F;  // 表示这一帧的第一个字节
		size_t restsize = frame_size % RTP_MAX_SIZE;  // 分片完成后的尾部数据大小
		size_t count = frame_size / RTP_MAX_SIZE;  // 一共要分几次片
		for (size_t i = 0; i < count; i++) {
			rtpframe.m_pyload.resize(RTP_MAX_SIZE + 2);
			((BYTE*)rtpframe.m_pyload)[0] = 0x60 | 28;//0110 0000  | 0001 1100
			((BYTE*)rtpframe.m_pyload)[1] = nalu;// 0000 0000 中间
			if (i == 0)
				((BYTE*)rtpframe.m_pyload)[1] |= 0x80;// 1000 0000 开始
			else if ((restsize == 0) && (i == count - 1))
				((BYTE*)rtpframe.m_pyload)[1] |= 0x40;// 0100 0000 结束
			/* 跳过前两个字节 */
			memcpy(2 + (BYTE*)rtpframe.m_pyload, pFrame + RTP_MAX_SIZE * i + 1, RTP_MAX_SIZE);// 2+ 表示前面两个是荷载位 //+1表示跳过第一个字节
			SendFrame(rtpframe, client);//rtpframe 里面包括了 header
			rtpframe.m_head.serial++;   // header 主要是 序号累加，其他不需要干什么
		}
		if (restsize > 0) {
			//处理尾巴
			rtpframe.m_pyload.resize(restsize + 2);
			((BYTE*)rtpframe.m_pyload)[0] = 0x60 | 28;//0110 0000  | 0001 1100
			((BYTE*)rtpframe.m_pyload)[1] = nalu;// 0000 0000 中间
			((BYTE*)rtpframe.m_pyload)[1] = 0x40 | ((BYTE*)rtpframe.m_pyload)[1];// 0100 0000 结束
			memcpy(2 + (BYTE*)rtpframe.m_pyload, pFrame + RTP_MAX_SIZE * count + 1, restsize);
			SendFrame(rtpframe, client);
			rtpframe.m_head.serial++;
		}
	}
	else {//小包发送
		rtpframe.m_pyload.resize(frame.size() - sepsize);   // 去掉 00 00 01 或者 00 00 00 01
		memcpy(rtpframe.m_pyload, pFrame, frame.size() - sepsize);
		SendFrame(rtpframe, client);
		//序号是累加，时间戳一般是计算出来的，从0开始，每帧追加 时钟频率90000/每秒帧数24
		rtpframe.m_head.serial++;
	}
	//时间戳是累加的，累加的量是时钟频率/每秒帧数 取整
	rtpframe.m_head.timestamp += 90000 / 24;
	//发送后，加入休眠，等待发送完成，控制发送速度
	Sleep(1000 / 30);
	return 0;
}

int RTPHelper::GetFrameSepSize(EBuffer& frame)  // 得到 h.264 的头是从 00 00 00 01 开始还是从 00 00 01 开始
{
	BYTE buf[] = { 0,0,0,1 };
	if (memcmp(frame, buf, 4) == 0) return 4;
	return 3;
}

int RTPHelper::SendFrame(const EBuffer& frame, const EAddress& client)
{
	/* 使用 Udp 发送流媒体数据 */
	int ret = sendto(m_udp, frame, frame.size(), 0, client, client.size());
	return 0;
}

RTPHeader::RTPHeader()
{
	csrccount = 0;
	extension = 0;
	padding = 0;
	version = 2;
	pytype = 96;
	mark = 0;
	serial = 0;
	timestamp = 0;
	ssrc = 0x98765432;
	memset(csrc, 0, sizeof(csrc));
}

RTPHeader::RTPHeader(const RTPHeader& header)
{
	memset(csrc, 0, sizeof(csrc));
	int size = 14 + 4 * csrccount;
	memcpy(this, &header, size);
}

RTPHeader& RTPHeader::operator=(const RTPHeader& header)
{
	if (this != &header) {
		int size = 14 + 4 * csrccount;
		memcpy(this, &header, size);
	}
	return *this;
}

RTPHeader::operator EBuffer()
{
	RTPHeader header = *this;
	header.serial = htons(header.serial);
	header.timestamp = htonl(header.timestamp);
	header.ssrc = htonl(header.ssrc);
	int size = 12 + 4 * csrccount;
	EBuffer result(size);
	memcpy(result, &header, size);
	return result;
}

RTPFrame::operator EBuffer()
{
	EBuffer result;
	result += (EBuffer)m_head;
	result += m_pyload;
	return result;
}
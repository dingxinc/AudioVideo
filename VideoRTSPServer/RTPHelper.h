#pragma once
#include "base.h"
#include "Socket.h"

/* RTP 的头 */
class RTPHeader
{
public://见RTP header定义图
	unsigned short csrccount : 4;
	unsigned short extension : 1;
	unsigned short padding : 1;
	unsigned short version : 2;//位域
	unsigned short pytype : 7;
	unsigned short mark : 1;
	unsigned short serial;
	unsigned timestamp;
	unsigned ssrc;
	unsigned csrc[15];
public:
	RTPHeader();
	RTPHeader(const RTPHeader& header);
	RTPHeader& operator=(const RTPHeader& header);
	operator EBuffer();
};

/* RTP 的帧 */
class RTPFrame
{
public:
	RTPHeader m_head;     // 数据头
	EBuffer m_pyload;     // 负载类型，如果要分片前两个字节保留
	operator EBuffer();
};

class RTPHelper
{
public:
	RTPHelper() :timestamp(0), m_udp(false) {// 创建 tcp 的套接字
		m_udp.Bind(EAddress("0.0.0.0", (short)55000));
	}
	~RTPHelper() {}
	int SendMediaFrame(RTPFrame& rtpframe, EBuffer& frame, const EAddress& client);
private:
	int GetFrameSepSize(EBuffer& frame);
	int SendFrame(const EBuffer& frame, const EAddress& client);
	DWORD timestamp;
	ESocket m_udp;
};


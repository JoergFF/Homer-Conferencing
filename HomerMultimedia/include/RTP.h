/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: real-time transport protocol handling
 * Author:  Thomas Volkert
 * Since:   2009-01-20
 */

#ifndef _MULTIMEDIA_RTP_
#define _MULTIMEDIA_RTP_

#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>

#include <sys/types.h>
#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of send RTP packets
//#define RTP_DEBUG_PACKET_ENCODER_FFMPEG
//#define RTP_DEBUG_PACKET_ENCODER
//#define RTP_DEBUG_PACKET_ENCODER_PTS

// the following de/activates debugging of received RTP packets
//#define RTP_DEBUG_PACKET_DECODER
//#define RTP_DEBUG_PACKET_DECODER_SEQUENCE_NUMBERS
//#define RTP_DEBUG_PACKET_DECODER_TIMESTAMPS

//#define RTCP_DEBUG_PACKETS_ENCODER
//#define RTCP_DEBUG_PACKET_ENCODER_FFMPEG

// the following de/activates debugging of RTCP packets
//#define RTCP_DEBUG_PACKETS_DECODER

///////////////////////////////////////////////////////////////////////////////

// ########################## RTCP ###########################################
union RtcpHeader{
    struct{ // send via separate port
        unsigned short int Length;          /* length of report */
        unsigned int Type:8;                /* report type */
        unsigned int RC:5;                  /* report counter */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */
        unsigned int Ssrc;                  /* synchronization source */
        unsigned int Data[5];              /*  */
    } __attribute__((__packed__))General;
    struct{ // send within media stream as intermediate packets
        unsigned short int Length;          /* length of report */
        unsigned int Type:8;                /* Payload type (PT) */
        unsigned int Fmt:5;                 /* Feedback message type (FMT) */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */
        unsigned int Ssrc;                  /* synchronization source */
        unsigned int TimestampHigh;         /* high part of reference timestamp */
        unsigned int TimestampLow;          /* low part of reference timestamp */
        unsigned int RtpTimestamp;          /* reference RTP timestamp */
        unsigned int Packets;               /* packet count */
        unsigned int Octets;                /* byte count */
    } __attribute__((__packed__))Feedback;
    uint32_t Data[7];
};

// calculate the size of an RTCP header: "size of structure"
#define RTCP_HEADER_SIZE                      sizeof(RtcpHeader)

///////////////////////////////////////////////////////////////////////////////

// ########################## RTP ############################################
union RtpHeader{
    struct{
        unsigned short int SequenceNumber; /* sequence number */

        unsigned int PayloadType:7;         /* payload type */
        unsigned int Marked:1;              /* marker bit */
        unsigned int CsrcCount:4;           /* CSRC count */
        unsigned int Extension:1;           /* header extension flag */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Timestamp;             /* timestamp */

        unsigned int Ssrc;                  /* synchronization source */
        //HINT: we do not support CSRC because it is not necessary!
        //unsigned int Csrc[1];               /* optional CSRC list */
    } __attribute__((__packed__));
    uint32_t Data[3];
};

///////////////////////////////////////////////////////////////////////////////

// calculate the size of an RTP header: "size of structure"
#define RTP_HEADER_SIZE                      sizeof(RtpHeader)

///////////////////////////////////////////////////////////////////////////////

class RTP
{
public:
    RTP();

    virtual ~RTP( );

    static unsigned int CodecToPayloadId(std::string pName);
    static std::string PayloadIdToCodec(int pId);
    static std::string PayloadType(int pId);
    static bool IsPayloadSupported(enum CodecID pId);
    static int GetPayloadHeaderSizeMax(enum CodecID pCodec);// calculate the maximum header size of the RTP payload (not the RTP header!)
    static int GetHeaderSizeMax(enum CodecID pCodec);
    static void SetH261PayloadSizeMax(unsigned int pMaxSize);
    static unsigned int GetH261PayloadSizeMax();

    /* RTP packetizing/parsing */
    bool RtpCreate(char *&pData, unsigned int &pDataSize, int64_t pPacketPts);
    unsigned int GetLostPacketsFromRTP();
    float GetRelativeLostPacketsFromRTP(); // uses RTCP packets and concludes relative packet loss in "per cent", which occurred within the last synch. period

    static void LogRtpHeader(RtpHeader *pRtpHeader);
    bool ReceivedCorrectPayload(unsigned int pType);
    bool RtpParse(char *&pData, int &pDataSize, bool &pIsLastFragment, bool &pIsSenderReport, enum CodecID pCodecId, bool pReadOnly);
    bool OpenRtpEncoder(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool CloseRtpEncoder();

    void RTPRegisterPacketStatistic(Homer::Monitor::PacketStatistic *pStatistic);

    /* RTP state */
    unsigned int GetRTPPayloadType();

    /* RTCP packetizing/parsing */
    static void LogRtcpHeader(RtcpHeader *pRtcpHeader);
    bool RtcpParseSenderReport(char *&pData, int &pDataSize, int64_t &pEndToEndDelay /* in micro seconds */, unsigned int &pPackets, unsigned int &pOctets, float &pRelativeLoss);

protected:
    uint64_t GetCurrentPtsFromRTP(); // uses the timestamps from the RTP header to derive a valid PTS value
    void GetSynchronizationReferenceFromRTP(uint64_t &pReferenceNtpTime, uint64_t &pReferencePts);
    unsigned int GetSourceIdentifierFromRTP(); // returns the RTP source identifier
    bool HasSourceChangedFromRTP(); // return if RTP source identifier has changed and resets the flag

    /* for clock rate adaption, e.g., 8, 16, 90 kHz */
    float CalculateClockRateFactor();

    void Init();

private:
    void AnnounceLostPackets(uint64_t pCount);

    void RtcpPatchLiveSenderReport(char *pHeader, uint32_t pTimestamp);

    /* internal RTP packetizer for h.261 */
    bool OpenRtpEncoderH261(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool RtpCreateH261(char *&pData, unsigned int &pDataSize, int64_t pPacketPts);
    void RtcpCreateH261SenderReport(char *&pData, unsigned int &pDataSize, int64_t pCurPts);

    /* RTP packet stream */
    static int StoreRtpPacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);
    void OpenRtpPacketStream();
    int CloseRtpPacketStream(char** pBuffer);

    Homer::Monitor::PacketStatistic *mPacketStatistic;
    AVStream            *mRtpEncoderStream;
    AVFormatContext     *mRtpFormatContext;
    unsigned int        mPayloadId;
    bool                mIntermediateFragment;
    bool                mEncoderOpened;
    std::string         mTargetHost;
    unsigned int        mTargetPort;
    uint64_t	        mLostPackets;
    float               mRelativeLostPackets;
    unsigned int        mLocalSourceIdentifier;
    enum CodecID        mStreamCodecID;
    uint64_t			mRemoteSequenceNumber; // without overflows
    unsigned short int	mLastSequenceNumberFromRTPHeader; // for overflow check
    uint64_t			mRemoteSequenceNumberOverflowShift; // offset for shifting the value range
    uint64_t			mRemoteSequenceNumberLastPacket;
    int					mRemoteSequenceNumberConsecutiveOverflows;
    unsigned short int  mRemoteStartSequenceNumber;
    uint64_t            mRemoteTimestamp; // without overflows
    unsigned int		mLastTimestampFromRTPHeader; // for overflow check
    uint64_t			mRemoteTimestampOverflowShift; // offset for shifting the value range
    uint64_t            mRemoteTimestampLastPacket;
    int 				mRemoteTimestampConsecutiveOverflows;
    uint64_t            mRemoteTimestampLastCompleteFrame;
    uint64_t            mRemoteStartTimestamp;
    bool				mRtpRemoteSourceChanged;
    int                 mRemoteSourceChangedLastPayload;
    int                 mRemoteSourceChangedResetScore;
    unsigned int        mRemoteSourceIdentifier;
    uint64_t            mReceivedPackets;
    /* MP3 RTP hack */
    unsigned int        mMp3Hack_EntireBufferSize;
    /* RTP packet stream */
    AVIOContext         *mAVIOContext;
    char                *mRtpPacketBuffer;
    char                *mRtpPacketStream;
    char                *mRtpPacketStreamPos;
    char                *mRtcpLastSenderReport;
    /* H261 RTP encoder */
    static unsigned int mH261PayloadSizeMax;
    bool                mH261UseInternalEncoder;
    unsigned short int  mH261LocalSequenceNumber;
    uint64_t            mH261SentPackets;
    uint64_t            mH261SentOctets;
    uint64_t            mH261SentOctetsLastSenderReport;
    uint64_t            mH261SentNtpTimeLastSenderReport;
    uint64_t            mH261SentNtpTimeBase;
    int                 mH261SenderReports;
    bool                mH261FirstPacket;
    /* RTCP */
    Mutex               mSynchDataMutex;
    uint64_t            mRtcpLastRemoteNtpTime; // (NTP timestamp)
    uint64_t            mRtcpLastRemoteTimestamp; // PTS value (without clock rata adaption!)
    unsigned int        mRtcpLastRemotePackets; // sent packets, reported via RTCP
    unsigned int        mRtcpLastRemoteOctets; // sent bytes, reported via RTCP
    uint64_t            mRtcpLastReceivedPackets;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif

# Frame acknowledgement

Experimental implementation of `draft-ietf-avtcore-frame-acknowledgement-01`.

This directory intentionally starts as an isolated libwebrtc module so the wire format and receiver state behavior can be tested before modifying the live RTP/RTCP send and receive paths.

The next integration step is to register the RTP header-extension URI, expose this value type through `RtpPacket`/`RtpPacketToSend`, add the RTPFB parser/sender path, and connect receiver decode completion to `ReceiverStatusWindow`.

The RTCP feedback format currently uses FMT 12 because the draft suggests that value. It must remain experimental until IANA assigns a final value.

# Security constraints

Frame acknowledgement metadata is transport/control information, not media content. Media payload encryption must remain end-to-end between participants even when RTP/RTCP is relayed by an SFU.

The E2EE layer will be implemented above the codec output and below RTP packetization using `FrameEncryptorInterface` and `FrameDecryptorInterface`, while standard SRTP remains enabled. SFUs must not receive media content keys.

Frame acknowledgement, dependency/layer routing information required by the SFU, and congestion-control metadata may remain visible as protocol metadata, but immutable metadata relied upon by the decryptor/decoder must be authenticated by the frame cipher to prevent undetected tampering.

No custom cryptographic primitive should be introduced. Cipher selection, nonce construction, key epochs, replay protection, and group key distribution must use established constructions and receive separate review before production use.

# Frame ACK + E2EE RTC roadmap

Priority: FaceTime-quality or better perceived quality, with real media end-to-end encryption.

## Milestone 1: Frame ACK wire protocol

- Implement draft-ietf-avtcore-frame-acknowledgement-01 wire model.
- Keep RTCP FMT 12 explicitly experimental until IANA assignment.
- Validate wrap-around, status windows, feedback vectors, and resync requests.

## Milestone 2: libwebrtc RTP/RTCP integration

- Register `urn:ietf:params:rtp-hdrext:frame-acknowledgement`.
- Add RTP extension parsing/writing on selected video frames.
- Add RTCP feedback parsing/sending in the RTCP receiver/sender paths.
- Expose decoded-frame feedback to the video sender.

## Milestone 3: quality controller

- Maintain safe acknowledged references.
- Use Frame ACK together with TWCC, RTT, loss, jitter, decoder stalls, and frame dependency metadata.
- Prefer a known-good resync reference over a keyframe when possible.
- Adapt acknowledgement frequency to network stability and frame importance.

## Milestone 4: AV1 LTR/reference control

- Track AV1 reference slots and their receiver acknowledgement state.
- Preserve one or more safe long-term references.
- On chain break, encode from the newest usable acknowledged reference.
- Fall back to a keyframe only when no safe encoder reference remains.

## Milestone 5: media E2EE

- Use WebRTC `FrameEncryptorInterface` / `FrameDecryptorInterface` as the media encryption insertion points.
- Keep SRTP enabled; frame encryption is an additional E2EE layer.
- Servers/SFUs never receive media content keys.
- Authenticate immutable frame metadata required for safe decoding and replay protection.
- Add key IDs, epochs, nonce/counter management, replay windows, and rotation.
- Design group key distribution separately from the media cipher.

## Quality acceptance criteria

Benchmark against unmodified WebRTC under controlled impairment:

- glass-to-glass latency
- freeze duration
- recovery latency
- keyframes per minute
- recovery bytes
- dropped frames
- VMAF/SSIM or equivalent objective quality metrics
- audio interruption duration

Primary scenario: sudden bandwidth collapse plus burst loss and jitter. The target behavior is recovery via a known-good reference without a visible freeze or large keyframe burst whenever the codec reference state allows it.

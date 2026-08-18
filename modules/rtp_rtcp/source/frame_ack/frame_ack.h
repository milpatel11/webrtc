/*
 * Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_FRAME_ACK_FRAME_ACK_H_
#define MODULES_RTP_RTCP_SOURCE_FRAME_ACK_FRAME_ACK_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/array_view.h"

namespace webrtc {
namespace frame_ack {

// Wire model for draft-ietf-avtcore-frame-acknowledgement-01.
// FMT=12 is experimental until assigned by IANA.
constexpr uint8_t kRtcpPayloadType = 205;
constexpr uint8_t kExperimentalRtcpFmt = 12;
constexpr size_t kDefaultStatusWindowSize = 255;
constexpr char kUri[] = "urn:ietf:params:rtp-hdrext:frame-acknowledgement";

enum class FeedbackRequestFormat : uint8_t {
  kFrameIdOnly = 0,
  kImplicitFeedback = 1,
  kIndependentFeedback = 2,
};

struct HeaderExtension {
  FeedbackRequestFormat format = FeedbackRequestFormat::kFrameIdOnly;
  uint16_t frame_id = 0;
  uint16_t feedback_start = 0;
  uint8_t feedback_length = 0;

  size_t ValueSize() const;
  bool Write(ArrayView<uint8_t> data) const;
  static std::optional<HeaderExtension> Parse(ArrayView<const uint8_t> data);
};

struct Feedback {
  uint32_t sender_ssrc = 0;
  uint32_t media_ssrc = 0;
  bool resync_request = false;
  uint16_t start_frame_id = 0;
  std::vector<bool> status;

  std::vector<uint8_t> Build() const;
  static std::optional<Feedback> Parse(ArrayView<const uint8_t> packet);
};

bool IsNewerFrameId(uint16_t lhs, uint16_t rhs);

class FrameIdAllocator {
 public:
  explicit FrameIdAllocator(uint16_t initial = 0) : next_(initial) {}
  uint16_t Next();

 private:
  uint16_t next_;
};

class ReceiverStatusWindow {
 public:
  explicit ReceiverStatusWindow(size_t capacity = kDefaultStatusWindowSize);

  void Record(uint16_t frame_id, bool decoded_or_will_decode);

  Feedback BuildFeedback(uint32_t sender_ssrc,
                         uint32_t media_ssrc,
                         uint16_t request_start,
                         uint8_t request_length,
                         bool resync_request = false) const;

  size_t size() const { return entries_.size(); }
  size_t capacity() const { return capacity_; }

 private:
  struct Entry {
    uint16_t frame_id;
    bool decoded;
  };

  size_t capacity_;
  std::vector<Entry> entries_;
};

}  // namespace frame_ack
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_FRAME_ACK_FRAME_ACK_H_

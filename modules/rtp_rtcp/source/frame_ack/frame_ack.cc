/*
 * Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/frame_ack/frame_ack.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace frame_ack {

namespace {
constexpr uint8_t kResyncMask = 0x80;
constexpr uint8_t kFfrShift = 6;

uint16_t AddFrameId(uint16_t base, uint16_t delta) {
  return static_cast<uint16_t>(base + delta);
}
}  // namespace

size_t HeaderExtension::ValueSize() const {
  return format == FeedbackRequestFormat::kIndependentFeedback ? 6 : 3;
}

bool HeaderExtension::Write(ArrayView<uint8_t> data) const {
  if (data.size() != ValueSize()) {
    return false;
  }
  data[0] = static_cast<uint8_t>(static_cast<uint8_t>(format) << kFfrShift);
  ByteWriter<uint16_t>::WriteBigEndian(data.data() + 1, frame_id);
  if (format == FeedbackRequestFormat::kIndependentFeedback) {
    ByteWriter<uint16_t>::WriteBigEndian(data.data() + 3, feedback_start);
    data[5] = feedback_length;
  }
  return true;
}

std::optional<HeaderExtension> HeaderExtension::Parse(
    ArrayView<const uint8_t> data) {
  if (data.size() != 3 && data.size() != 6) {
    return std::nullopt;
  }
  const uint8_t raw_format = data[0] >> kFfrShift;
  if (raw_format > 2) {
    return std::nullopt;
  }

  HeaderExtension extension;
  extension.format = static_cast<FeedbackRequestFormat>(raw_format);
  extension.frame_id = ByteReader<uint16_t>::ReadBigEndian(data.data() + 1);

  if (extension.format == FeedbackRequestFormat::kIndependentFeedback) {
    if (data.size() != 6) {
      return std::nullopt;
    }
    extension.feedback_start =
        ByteReader<uint16_t>::ReadBigEndian(data.data() + 3);
    extension.feedback_length = data[5];
  } else if (data.size() != 3) {
    return std::nullopt;
  }
  return extension;
}

std::vector<uint8_t> Feedback::Build() const {
  if (status.size() > 255) {
    return {};
  }
  const size_t vector_bytes = (status.size() + 7) / 8;
  const size_t padded_vector_bytes = ((vector_bytes + 3) / 4) * 4;
  const size_t packet_size = 16 + padded_vector_bytes;
  std::vector<uint8_t> packet(packet_size, 0);

  packet[0] = static_cast<uint8_t>(0x80 | kExperimentalRtcpFmt);
  packet[1] = kRtcpPayloadType;
  ByteWriter<uint16_t>::WriteBigEndian(
      packet.data() + 2, static_cast<uint16_t>(packet_size / 4 - 1));
  ByteWriter<uint32_t>::WriteBigEndian(packet.data() + 4, sender_ssrc);
  ByteWriter<uint32_t>::WriteBigEndian(packet.data() + 8, media_ssrc);
  packet[12] = resync_request ? kResyncMask : 0;
  ByteWriter<uint16_t>::WriteBigEndian(packet.data() + 13, start_frame_id);
  packet[15] = static_cast<uint8_t>(status.size());

  for (size_t i = 0; i < status.size(); ++i) {
    if (status[i]) {
      packet[16 + i / 8] |= static_cast<uint8_t>(0x80 >> (i % 8));
    }
  }
  return packet;
}

std::optional<Feedback> Feedback::Parse(ArrayView<const uint8_t> packet) {
  if (packet.size() < 16 || packet.size() % 4 != 0) {
    return std::nullopt;
  }
  if ((packet[0] >> 6) != 2 || (packet[0] & 0x20) != 0 ||
      (packet[0] & 0x1f) != kExperimentalRtcpFmt ||
      packet[1] != kRtcpPayloadType) {
    return std::nullopt;
  }
  const size_t declared_size =
      (static_cast<size_t>(ByteReader<uint16_t>::ReadBigEndian(packet.data() + 2)) +
       1) *
      4;
  if (declared_size != packet.size() || (packet[12] & 0x7f) != 0) {
    return std::nullopt;
  }

  const uint8_t status_length = packet[15];
  const size_t vector_bytes = (static_cast<size_t>(status_length) + 7) / 8;
  const size_t padded_vector_bytes = ((vector_bytes + 3) / 4) * 4;
  if (16 + padded_vector_bytes != packet.size()) {
    return std::nullopt;
  }

  Feedback feedback;
  feedback.sender_ssrc = ByteReader<uint32_t>::ReadBigEndian(packet.data() + 4);
  feedback.media_ssrc = ByteReader<uint32_t>::ReadBigEndian(packet.data() + 8);
  feedback.resync_request = (packet[12] & kResyncMask) != 0;
  feedback.start_frame_id =
      ByteReader<uint16_t>::ReadBigEndian(packet.data() + 13);
  feedback.status.resize(status_length);
  for (size_t i = 0; i < status_length; ++i) {
    feedback.status[i] =
        (packet[16 + i / 8] & static_cast<uint8_t>(0x80 >> (i % 8))) != 0;
  }
  return feedback;
}

bool IsNewerFrameId(uint16_t lhs, uint16_t rhs) {
  return lhs != rhs && static_cast<uint16_t>(lhs - rhs) < 0x8000;
}

uint16_t FrameIdAllocator::Next() {
  const uint16_t current = next_;
  next_ = static_cast<uint16_t>(next_ + 1);
  return current;
}

ReceiverStatusWindow::ReceiverStatusWindow(size_t capacity)
    : capacity_(std::clamp<size_t>(capacity, 1, 32767)) {}

void ReceiverStatusWindow::Record(uint16_t frame_id,
                                  bool decoded_or_will_decode) {
  auto existing = std::find_if(entries_.begin(), entries_.end(),
                               [frame_id](const Entry& entry) {
                                 return entry.frame_id == frame_id;
                               });
  if (existing != entries_.end()) {
    existing->decoded = decoded_or_will_decode;
    return;
  }

  if (entries_.empty()) {
    entries_.push_back({frame_id, decoded_or_will_decode});
  } else if (IsNewerFrameId(frame_id, entries_.back().frame_id)) {
    uint16_t cursor = static_cast<uint16_t>(entries_.back().frame_id + 1);
    while (cursor != frame_id) {
      entries_.push_back({cursor, false});
      cursor = static_cast<uint16_t>(cursor + 1);
      if (entries_.size() > capacity_) {
        entries_.erase(entries_.begin());
      }
    }
    entries_.push_back({frame_id, decoded_or_will_decode});
  } else {
    auto insertion = std::find_if(entries_.begin(), entries_.end(),
                                  [frame_id](const Entry& entry) {
                                    return IsNewerFrameId(entry.frame_id, frame_id);
                                  });
    if (insertion != entries_.end()) {
      entries_.insert(insertion, {frame_id, decoded_or_will_decode});
    }
  }

  while (entries_.size() > capacity_) {
    entries_.erase(entries_.begin());
  }
}

Feedback ReceiverStatusWindow::BuildFeedback(uint32_t sender_ssrc,
                                             uint32_t media_ssrc,
                                             uint16_t request_start,
                                             uint8_t request_length,
                                             bool resync_request) const {
  Feedback feedback;
  feedback.sender_ssrc = sender_ssrc;
  feedback.media_ssrc = media_ssrc;
  feedback.resync_request = resync_request;
  feedback.start_frame_id = request_start;

  if (request_length == 0 || entries_.empty()) {
    return feedback;
  }

  size_t first_offset = request_length;
  for (size_t offset = 0; offset < request_length; ++offset) {
    const uint16_t id = AddFrameId(request_start, static_cast<uint16_t>(offset));
    if (std::any_of(entries_.begin(), entries_.end(),
                    [id](const Entry& entry) { return entry.frame_id == id; })) {
      first_offset = offset;
      break;
    }
  }
  if (first_offset == request_length) {
    return feedback;
  }

  feedback.start_frame_id =
      AddFrameId(request_start, static_cast<uint16_t>(first_offset));
  for (size_t offset = first_offset; offset < request_length; ++offset) {
    const uint16_t id = AddFrameId(request_start, static_cast<uint16_t>(offset));
    auto entry = std::find_if(entries_.begin(), entries_.end(),
                              [id](const Entry& candidate) {
                                return candidate.frame_id == id;
                              });
    if (entry == entries_.end()) {
      break;
    }
    feedback.status.push_back(entry->decoded);
  }
  return feedback;
}

}  // namespace frame_ack
}  // namespace webrtc

/*
 * Copyright 2026 The WebRTC project authors. All Rights Reserved.
 */
#include "modules/rtp_rtcp/source/frame_ack/frame_ack.h"

#include <cstdint>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace frame_ack {
namespace {

TEST(FrameAckHeaderExtensionTest, SerializesIndependentRequest) {
  HeaderExtension extension;
  extension.format = FeedbackRequestFormat::kIndependentFeedback;
  extension.frame_id = 0x0102;
  extension.feedback_start = 65534;
  extension.feedback_length = 3;

  std::vector<uint8_t> bytes(extension.ValueSize());
  ASSERT_TRUE(extension.Write(bytes));
  EXPECT_EQ(bytes,
            (std::vector<uint8_t>{0x80, 0x01, 0x02, 0xff, 0xfe, 0x03}));

  auto parsed = HeaderExtension::Parse(bytes);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->frame_id, 0x0102);
  EXPECT_EQ(parsed->feedback_start, 65534);
  EXPECT_EQ(parsed->feedback_length, 3);
}

TEST(FrameAckFeedbackTest, RoundTripsStatusVector) {
  Feedback feedback;
  feedback.sender_ssrc = 0x01020304;
  feedback.media_ssrc = 0xaabbccdd;
  feedback.start_frame_id = 10;
  feedback.status = {true, false, true, true, false};

  std::vector<uint8_t> packet = feedback.Build();
  ASSERT_EQ(packet.size(), 20u);
  EXPECT_EQ(packet[0], 0x8c);
  EXPECT_EQ(packet[1], 205);
  EXPECT_EQ(packet[16], 0xb0);

  auto parsed = Feedback::Parse(packet);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->start_frame_id, 10);
  EXPECT_EQ(parsed->status, feedback.status);
}

TEST(FrameAckStatusWindowTest, ImplementsDraftLossExample) {
  ReceiverStatusWindow window;
  window.Record(10, true);
  window.Record(11, false);
  window.Record(12, false);

  Feedback feedback = window.BuildFeedback(1, 2, 10, 3);
  EXPECT_EQ(feedback.start_frame_id, 10);
  EXPECT_EQ(feedback.status, (std::vector<bool>{true, false, false}));
}

TEST(FrameAckStatusWindowTest, HandlesWrapAround) {
  ReceiverStatusWindow window;
  window.Record(65534, true);
  window.Record(65535, true);
  window.Record(0, false);

  Feedback feedback = window.BuildFeedback(1, 2, 65534, 3);
  EXPECT_EQ(feedback.status, (std::vector<bool>{true, true, false}));
  EXPECT_TRUE(IsNewerFrameId(0, 65535));
}

TEST(FrameAckFrameIdAllocatorTest, WrapsAtUint16Boundary) {
  FrameIdAllocator allocator(65534);
  EXPECT_EQ(allocator.Next(), 65534);
  EXPECT_EQ(allocator.Next(), 65535);
  EXPECT_EQ(allocator.Next(), 0);
}

}  // namespace
}  // namespace frame_ack
}  // namespace webrtc

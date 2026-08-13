/*
 * Copyright 2025 Jinwoo Sung
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "wirestead/wrapper/context.hpp"

using namespace wirestead;
using namespace wirestead::wrapper;

namespace {

std::vector<uint8_t> make_payload(const std::string& text) { return std::vector<uint8_t>(text.begin(), text.end()); }

}  // namespace

// The borrowing constructor exists to keep the single-shot on_data()/
// on_message() dispatch allocation-free. Pointer identity is the only direct
// evidence that no copy happened, so assert it - a future change that
// reintroduces a per-chunk copy has to fail here.
TEST(MessageContextTest, ViewConstructorDoesNotCopyThePayload) {
  auto payload = make_payload("borrowed");
  const MessageContext ctx(7, memory::ConstByteSpan(payload.data(), payload.size()));

  EXPECT_EQ(ctx.client_id(), 7u);
  EXPECT_EQ(ctx.data(), "borrowed");
  EXPECT_EQ(static_cast<const void*>(ctx.data().data()), static_cast<const void*>(payload.data()));
}

TEST(MessageContextTest, OwningConstructorCopiesThePayload) {
  auto payload = make_payload("owned");
  const MessageContext ctx(3, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())));

  EXPECT_EQ(ctx.data(), "owned");
  EXPECT_NE(static_cast<const void*>(ctx.data().data()), static_cast<const void*>(payload.data()));

  // Mutating the source must not be visible through an owning context.
  payload[0] = static_cast<uint8_t>('X');
  EXPECT_EQ(ctx.data(), "owned");
}

TEST(MessageContextTest, AccessorsAgreeAcrossBothConstructors) {
  auto payload = make_payload("hello wirestead");
  const MessageContext view_ctx(1, memory::ConstByteSpan(payload.data(), payload.size()));
  const MessageContext owned_ctx(1, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())));

  EXPECT_EQ(view_ctx.data(), owned_ctx.data());
  EXPECT_EQ(view_ctx.data_as_string(), owned_ctx.data_as_string());
  EXPECT_EQ(view_ctx.data_as_vector(), owned_ctx.data_as_vector());
  EXPECT_EQ(view_ctx.data_as_string(), "hello wirestead");
  EXPECT_EQ(view_ctx.data_as_vector(), payload);
}

// safe_data() is the one accessor that still has to hand back a real
// SafeDataBuffer, so a borrowing context materializes one on demand.
TEST(MessageContextTest, SafeDataMaterializesForABorrowingContext) {
  auto payload = make_payload("materialize me");
  const MessageContext ctx(0, memory::ConstByteSpan(payload.data(), payload.size()));

  const auto& buffer = ctx.safe_data();
  EXPECT_EQ(buffer.size(), payload.size());
  EXPECT_EQ(buffer.as_string(), "materialize me");

  // Cached: a second call returns the same object, and the other accessors
  // keep agreeing with it afterwards.
  EXPECT_EQ(&buffer, &ctx.safe_data());
  EXPECT_EQ(ctx.data(), "materialize me");
  EXPECT_EQ(ctx.data_as_vector(), payload);
}

TEST(MessageContextTest, SafeDataReturnsTheOwnedBufferDirectly) {
  auto payload = make_payload("already owned");
  const MessageContext ctx(0, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())));

  EXPECT_EQ(ctx.safe_data().as_string(), "already owned");
  EXPECT_EQ(static_cast<const void*>(ctx.data().data()), static_cast<const void*>(ctx.safe_data().data()));
}

// The batch handlers move contexts into a queue and then move the whole queue
// out to flush it, so moving must not leave a context reading from relocated
// storage.
TEST(MessageContextTest, MovingAnOwningContextKeepsItsPayload) {
  auto payload = make_payload("survives the move");
  MessageContext ctx(11, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())));

  std::vector<MessageContext> queue;
  queue.emplace_back(std::move(ctx));
  std::vector<MessageContext> flushed = std::move(queue);

  ASSERT_EQ(flushed.size(), 1u);
  EXPECT_EQ(flushed[0].client_id(), 11u);
  EXPECT_EQ(flushed[0].data(), "survives the move");
  EXPECT_EQ(flushed[0].data_as_vector(), payload);
}

// Copying is the only way a caller can keep a context past the callback (it is
// handed out as a const reference), so a copy of a borrowing context has to
// take ownership rather than carry the soon-to-dangle view.
TEST(MessageContextTest, CopyOfABorrowingContextOutlivesTheSourceBuffer) {
  MessageContext copy(0, memory::ConstByteSpan{});
  {
    auto payload = make_payload("source goes away");
    const MessageContext borrowed(5, memory::ConstByteSpan(payload.data(), payload.size()), "peer");
    copy = borrowed;
    EXPECT_NE(static_cast<const void*>(copy.data().data()), static_cast<const void*>(payload.data()));
  }
  EXPECT_EQ(copy.client_id(), 5u);
  EXPECT_EQ(copy.data(), "source goes away");
  EXPECT_EQ(copy.client_info(), "peer");
}

TEST(MessageContextTest, CopyConstructingABorrowingContextTakesOwnership) {
  std::vector<MessageContext> kept;
  {
    auto payload = make_payload("kept past the callback");
    const MessageContext borrowed(9, memory::ConstByteSpan(payload.data(), payload.size()));
    kept.push_back(borrowed);  // copy, not move
  }
  ASSERT_EQ(kept.size(), 1u);
  EXPECT_EQ(kept[0].data(), "kept past the callback");
}

// The batch queues rely on moves staying noexcept, otherwise vector
// reallocation silently downgrades to the deep-copying copy constructor.
TEST(MessageContextTest, MovesAreNoexcept) {
  EXPECT_TRUE(std::is_nothrow_move_constructible_v<MessageContext>);
  EXPECT_TRUE(std::is_nothrow_move_assignable_v<MessageContext>);
}

TEST(MessageContextTest, CopyingAMaterializedContextKeepsItsPayload) {
  auto payload = make_payload("copied");
  const MessageContext ctx(0, memory::ConstByteSpan(payload.data(), payload.size()));
  ASSERT_EQ(ctx.safe_data().as_string(), "copied");

  const MessageContext copy = ctx;
  EXPECT_EQ(copy.data(), "copied");
  EXPECT_EQ(copy.safe_data().as_string(), "copied");
}

TEST(MessageContextTest, EmptyPayloadIsSafeForBothConstructors) {
  const MessageContext view_ctx(0, memory::ConstByteSpan{});
  EXPECT_TRUE(view_ctx.data().empty());
  EXPECT_TRUE(view_ctx.data_as_string().empty());
  EXPECT_TRUE(view_ctx.data_as_vector().empty());
  EXPECT_TRUE(view_ctx.safe_data().empty());

  const MessageContext owned_ctx(0, memory::SafeDataBuffer(memory::ConstByteSpan{}));
  EXPECT_TRUE(owned_ctx.data().empty());
  EXPECT_TRUE(owned_ctx.data_as_string().empty());
  EXPECT_TRUE(owned_ctx.data_as_vector().empty());
}

// received_at() is stamped by a default member initializer, so the copy
// operations are the one place it can go wrong: a copy that let the
// initializer run again would report the time of the copy instead of the time
// of arrival, and a batch queue copies on every reallocation. Sleep long
// enough that a re-stamp cannot pass as clock granularity.
TEST(MessageContextTest, CopiesAndMovesKeepTheArrivalTime) {
  auto payload = make_payload("stamped");
  const MessageContext original(1, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())));
  const auto arrived = original.received_at();

  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  MessageContext copy_constructed = original;
  EXPECT_EQ(copy_constructed.received_at(), arrived);

  MessageContext copy_assigned(0, memory::ConstByteSpan{});
  copy_assigned = original;
  EXPECT_EQ(copy_assigned.received_at(), arrived);

  const MessageContext moved = std::move(copy_constructed);
  EXPECT_EQ(moved.received_at(), arrived);
}

// A borrowed context is the single-shot dispatch path and gets stamped the
// same way an owning one does.
TEST(MessageContextTest, ArrivalTimeIsStampedAtConstruction) {
  const auto before = std::chrono::steady_clock::now();
  auto payload = make_payload("now");
  const MessageContext view_ctx(0, memory::ConstByteSpan(payload.data(), payload.size()));
  const auto after = std::chrono::steady_clock::now();

  EXPECT_GE(view_ctx.received_at(), before);
  EXPECT_LE(view_ctx.received_at(), after);
}

TEST(MessageContextTest, ClientInfoIsCarriedByBothConstructors) {
  auto payload = make_payload("x");
  const MessageContext view_ctx(2, memory::ConstByteSpan(payload.data(), payload.size()), "127.0.0.1:9000");
  const MessageContext owned_ctx(2, memory::SafeDataBuffer(memory::ConstByteSpan(payload.data(), payload.size())),
                                 "127.0.0.1:9000");

  EXPECT_EQ(view_ctx.client_info(), "127.0.0.1:9000");
  EXPECT_EQ(owned_ctx.client_info(), "127.0.0.1:9000");
}

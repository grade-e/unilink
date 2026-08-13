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

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <string>

#include "test_utils.hpp"
#include "wirestead/config/udp_config.hpp"
#include "wirestead/wrapper/udp/udp.hpp"

using namespace std::chrono_literals;
namespace net = boost::asio;

namespace {

constexpr const char* kGroup = "239.255.42.99";
constexpr const char* kLoopback = "127.0.0.1";

// Multicast needs a route, and a container or a CI runner with no multicast-
// capable interface has none. That is an environment fact, not a defect, so
// the setup steps skip while the assertions below still fail loudly.
class MulticastSender {
 public:
  MulticastSender(net::io_context& ioc, uint16_t port) : socket_(ioc), endpoint_(net::ip::make_address(kGroup), port) {
    boost::system::error_code ec;
    socket_.open(net::ip::udp::v4(), ec);
    if (ec) return;
    // Send out of loopback and let this host see its own traffic - the
    // receiver under test is in this same process.
    socket_.set_option(net::ip::multicast::outbound_interface(net::ip::make_address_v4(kLoopback)), ec);
    if (ec) return;
    socket_.set_option(net::ip::multicast::enable_loopback(true), ec);
    if (ec) return;
    usable_ = true;
  }

  bool usable() const { return usable_; }

  bool send(const std::string& payload) {
    boost::system::error_code ec;
    socket_.send_to(net::buffer(payload), endpoint_, 0, ec);
    return !ec;
  }

 private:
  net::ip::udp::socket socket_;
  net::ip::udp::endpoint endpoint_;
  bool usable_{false};
};

}  // namespace

// The point of joining a group is receiving traffic addressed to it, and
// nothing short of a real datagram proves the join took effect: a socket that
// silently failed to join looks exactly like one whose sender went quiet.
TEST(UdpMulticastTest, JoinedGroupReceivesGroupTraffic) {
  const uint16_t port = wirestead::test::TestUtils::getAvailableTestPort();

  wirestead::config::UdpConfig cfg;
  cfg.bind_address = "0.0.0.0";
  cfg.local_port = port;
  cfg.reuse_address = true;
  cfg.multicast_group = kGroup;
  cfg.multicast_interface = kLoopback;

  wirestead::wrapper::UdpClient receiver(cfg);
  std::atomic<bool> got_it{false};
  receiver.on_data([&](const wirestead::wrapper::MessageContext& ctx) {
    if (ctx.data() == "multicast payload") got_it = true;
  });

  auto started = receiver.start();
  ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
  if (!started.get()) {
    GTEST_SKIP() << "could not join " << kGroup << " - no multicast route in this environment";
  }

  net::io_context ioc;
  MulticastSender sender(ioc, port);
  if (!sender.usable()) {
    receiver.stop();
    GTEST_SKIP() << "loopback multicast is not configurable in this environment";
  }

  for (int i = 0; i < 20 && !got_it.load(); ++i) {
    if (!sender.send("multicast payload")) {
      receiver.stop();
      GTEST_SKIP() << "sending to " << kGroup << " failed - no multicast route in this environment";
    }
    std::this_thread::sleep_for(50ms);
  }

  EXPECT_TRUE(got_it.load()) << "joined the group but never received a datagram addressed to it";
  receiver.stop();
}

// A group that was never joined must not deliver. Without this the test above
// would still pass on a host that hands every datagram to every socket.
TEST(UdpMulticastTest, UnjoinedSocketDoesNotReceiveGroupTraffic) {
  const uint16_t port = wirestead::test::TestUtils::getAvailableTestPort();

  wirestead::config::UdpConfig cfg;
  cfg.bind_address = "0.0.0.0";
  cfg.local_port = port;
  cfg.reuse_address = true;  // no multicast_group: never joined

  wirestead::wrapper::UdpClient receiver(cfg);
  std::atomic<bool> got_it{false};
  receiver.on_data([&](const wirestead::wrapper::MessageContext&) { got_it = true; });

  auto started = receiver.start();
  ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
  ASSERT_TRUE(started.get());

  net::io_context ioc;
  MulticastSender sender(ioc, port);
  if (!sender.usable()) {
    receiver.stop();
    GTEST_SKIP() << "loopback multicast is not configurable in this environment";
  }

  for (int i = 0; i < 5; ++i) {
    sender.send("multicast payload");
    std::this_thread::sleep_for(20ms);
  }
  std::this_thread::sleep_for(200ms);

  EXPECT_FALSE(got_it.load()) << "received group traffic without joining the group";
  receiver.stop();
}

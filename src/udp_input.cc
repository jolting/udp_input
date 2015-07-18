/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, Hunter Laux
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of owner nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <nodelet/nodelet.h>

#include <udp_input/UdpPacket.h>
#include <udp_input/UdpSend.h>

#include <pluginlib/class_list_macros.h>

#include <boost/thread.hpp>

using boost::asio::ip::udp;

#include <ros/ros.h>

namespace udp_input {
class UdpInput {
public:
  UdpInput(short port, ros::Publisher pub)
      : io_service_(), socket_(io_service_, udp::endpoint(udp::v4(), port)),
        pub_(pub) {
    boost::asio::socket_base::broadcast option(true);
    socket_.set_option(option);
    do_receive();
    thread_ =
        boost::thread(boost::bind(&boost::asio::io_service::run, &io_service_));
  }
  ~UdpInput() {
    io_service_.stop();
    thread_.join();
  }

  void do_receive() {
    packet_.data.resize(1500);
    socket_.async_receive_from(
        boost::asio::buffer(packet_.data), sender_endpoint_,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
          packet_.data.resize(bytes_recvd);
          packet_.stamp = ros::Time::now();
          packet_.address = sender_endpoint_.address().to_string();
          packet_.srcPort = sender_endpoint_.port();
          pub_.publish(packet_);
          do_receive();
        });
  }
  size_t send(std::vector<u_char> &data, std::string &addr, uint16_t port) {
    boost::asio::ip::udp::endpoint remote(
        boost::asio::ip::address::from_string(addr), port);
    return socket_.send_to(boost::asio::buffer(data), remote);
  }

private:
  udp::endpoint sender_endpoint_;
  ros::Publisher pub_;
  UdpPacket packet_;
  boost::asio::io_service io_service_;
  udp::socket socket_;
  boost::thread thread_;
};

class UdpInputNodelet : public nodelet::Nodelet {
public:
  UdpInputNodelet() {}

private:
  virtual void onInit() {
    int port = 0;
    ros::NodeHandle &private_nh = getPrivateNodeHandle();
    private_nh.getParam("port", port);
    service = private_nh.advertiseService("send", &UdpInputNodelet::send, this);
    udpInput_.reset(new UdpInput(
        port, private_nh.advertise<udp_input::UdpPacket>("udp", 10)));
  }
  bool send(UdpSend::Request &request, UdpSend::Response &) {
    udpInput_->send(request.data, request.address, request.dstPort);
    return true;
  }

  std::unique_ptr<UdpInput> udpInput_;
  ros::ServiceServer service;
};
PLUGINLIB_DECLARE_CLASS(udp_input, UdpInputNodelet, udp_input::UdpInputNodelet,
                        nodelet::Nodelet);
}

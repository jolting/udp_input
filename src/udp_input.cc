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
  ~UdpInputNodelet() { delete udpInput_; }

private:
  virtual void onInit() {
    int port = 0;
    ros::NodeHandle &private_nh = getPrivateNodeHandle();
    private_nh.getParam("port", port);
    service = private_nh.advertiseService("send", &UdpInputNodelet::send, this);
    udpInput_ = new UdpInput(
        port, private_nh.advertise<udp_input::UdpPacket>("udp", 10));
  }
  bool send(UdpSend::Request &request, UdpSend::Response &) {
    udpInput_->send(request.data, request.address, request.dstPort);
    return true;
  }

  UdpInput *udpInput_;
  ros::ServiceServer service;
};
PLUGINLIB_DECLARE_CLASS(udp_input, UdpInputNodelet, udp_input::UdpInputNodelet,
                        nodelet::Nodelet);
}

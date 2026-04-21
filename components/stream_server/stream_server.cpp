/* Copyright (C) 2020-2022 Oxan van Leeuwen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stream_server.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#include "esphome/components/network/util.h"
#include "esphome/components/socket/socket.h"

static const char *TAG = "streamserver";

using namespace esphome;

void StreamServerComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up stream server...");
    this->rx_buf_.resize(rx_buffer_size_);
    this->tx_buf_.resize(tx_buffer_size_);

    struct sockaddr_in bind_addr = {
        .sin_len = sizeof(struct sockaddr_in),
        .sin_family = AF_INET,
        .sin_port = htons(this->port_),
        .sin_addr = {
            .s_addr = ESPHOME_INADDR_ANY,
        }
    };

    this->socket_ = socket::socket(AF_INET, SOCK_STREAM, PF_INET);
	
    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = 20000; // ESPHome recommends 20-30 ms max for timeouts
    this->socket_->setsockopt(SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    this->socket_->setblocking(false);
  
    this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(struct sockaddr_in));
    this->socket_->listen(1);


}

void StreamServerComponent::loop() {
    if (!this->client_) {
      accept();
    } else {
      read_uart_to_tcp();
      write_tcp_to_uart();
    }
}

void StreamServerComponent::accept() {
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(struct sockaddr_in);
    this->client_ = this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);

    if (this->client_) {
      this->client_->setblocking(false);
      int enable = 1;
      this->client_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

      std::string identifier = inet_ntoa(client_addr.sin_addr);
      ESP_LOGD(TAG, "New client connected from %s", identifier.c_str());
    }
}

void StreamServerComponent::read_uart_to_tcp() {
    int available = this->stream_->available();
    if (available > 0) {
        size_t len = std::min(available, this->rx_buffer_size_);
        this->stream_->read_array(rx_buf_.data(), len);

        if (this->client_) {
	  ssize_t written = this->client_->write((const char*)this->rx_buf_.data(), len);
	  if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		this->client_.reset();
	  }
	}
    }
}

void StreamServerComponent::write_tcp_to_uart() {
    if (!this->client_) return;

    int len = this->client_->read(tx_buf_.data(), tx_buffer_size_);
    if (len > 0) {
       this->stream_->write_array((const uint8_t*) tx_buf_.data(), len);
    } else if (len == 0) {
        ESP_LOGD(TAG, "Client disconnected");
        this->client_.reset();
    } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "Socket error: %d", errno);
        this->client_.reset();
    }
}

void StreamServerComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "Stream Server:");
    std::string ip_str = "";
    for (auto &ip : network::get_ip_addresses()) {
      if (ip.is_set()) {
    	char buf[network::IP_ADDRESS_BUFFER_SIZE];
        ip_str += " " + std::string(ip.str_to(buf));
	  }
    }
    ESP_LOGCONFIG(TAG, "  Address:%s", ip_str.c_str());
    ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
}

void StreamServerComponent::on_shutdown() {
    if (this->client_) {
      this->client_->shutdown(SHUT_RDWR);
      this->client_.reset();
    }
}


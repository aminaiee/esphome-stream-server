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
  
    this->socket_->bind(reinterpret_cast<struct sockaddr *>(&bind_addr), sizeof(struct sockaddr_in));
    this->socket_->listen(8);


}

void StreamServerComponent::loop() {
    this->accept();
    this->read();
    this->write();
    this->cleanup();
}

void StreamServerComponent::accept() {
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(struct sockaddr_in);
    std::unique_ptr<socket::Socket> socket = this->socket_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &client_addrlen);
    if (!socket)
        return;

    socket->setblocking(false);

    int enable = 1;
    socket->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

    std::string identifier = inet_ntoa(client_addr.sin_addr);
    this->clients_.emplace_back(std::move(socket), identifier);
    ESP_LOGD(TAG, "New client connected from %s", identifier.c_str());
}

void StreamServerComponent::cleanup() {
    auto discriminator = [](const Client &client) { return !client.disconnected; };
    auto last_client = std::partition(this->clients_.begin(), this->clients_.end(), discriminator);
    this->clients_.erase(last_client, this->clients_.end());
}

void StreamServerComponent::read() {
    int available = this->stream_->available();

    if (available > 0) {
        size_t len = std::min(available, this->rx_buffer_size_);
        this->stream_->read_array(rx_buf_.data(), len);

	for (Client &client : this->clients_)
	    if (!client.disconnected) {
	        ssize_t written = client.socket->write((const char*)this->rx_buf_.data(), len);
		if (written < 0 && errno != EAGAIN) {
			client.disconnected = true;
		}
	    }
    }
}

void StreamServerComponent::write() {
    int len;
    for (Client &client : this->clients_) {
	    if (client.disconnected) continue;

	    while ((len = client.socket->read(tx_buf_.data(), tx_buffer_size_)) > 0){
		    this->stream_->write_array((const uint8_t*) tx_buf_.data(), len);
	    }

	    if (len == 0) {
		    ESP_LOGD(TAG, "Client %s disconnected", client.identifier.c_str());
		    client.disconnected = true;
		    continue;
	    } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		    ESP_LOGW(TAG, "Socket error on %s: %d", client.identifier.c_str(), errno);
		    client.disconnected = true;
	    }
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
    for (const Client &client : this->clients_)
        client.socket->shutdown(SHUT_RDWR);
}

StreamServerComponent::Client::Client(std::unique_ptr<esphome::socket::Socket> socket, std::string identifier)
    : socket(std::move(socket)), identifier{identifier}
{
}

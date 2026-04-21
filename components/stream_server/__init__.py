# Copyright (C) 2021-2022 Oxan van Leeuwen
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PORT
from esphome.util import parse_esphome_version

# ESPHome doesn't know the Stream abstraction yet, so hardcode to use a UART for now.

AUTO_LOAD = ["socket", "binary_sensor"]

DEPENDENCIES = ["uart", "network"]

CONF_RX_BUFFER_SIZE = "rx_buffer_size"
CONF_TX_BUFFER_SIZE = "tx_buffer_size"

MULTI_CONF = True

ns = cg.global_ns
StreamServerComponent = ns.class_("StreamServerComponent", cg.Component)

CONFIG_SCHEMA = (
	cv.Schema(
		{
			cv.GenerateID(): cv.declare_id(StreamServerComponent),
			cv.Optional(CONF_PORT, default=6638): cv.port,
			cv.Optional(CONF_RX_BUFFER_SIZE, default=128): cv.positive_not_null_int,
			cv.Optional(CONF_TX_BUFFER_SIZE, default=128): cv.positive_not_null_int,
		}
	)
		.extend(cv.COMPONENT_SCHEMA)
		.extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
	var = cg.new_Pvariable(config[CONF_ID])
	cg.add(var.set_port(config[CONF_PORT]))
	cg.add(var.set_rx_buffer_size(config[CONF_RX_BUFFER_SIZE]))
	cg.add(var.set_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))

	yield cg.register_component(var, config)
	yield uart.register_uart_device(var, config)

esphome_version = parse_esphome_version()

	# Request UART to wake the main loop when data arrives for low-latency processing
# Apply the fix only for versions 2025.12.x through 2026.2.x
if (2025, 12, 0) <= esphome_version < (2026, 3, 0):
    uart.request_wake_loop_on_rx()

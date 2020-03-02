#!/bin/bash
# 
# File:   install.sh
# Author: pconroy
#
# Created on May 23, 2019, 9:53:28 AM
#
###
sudo cp epsolarmqtt.service /etc/systemd/system/.
sudo chmod 644 /etc/systemd/system/epsolarmqtt.service
sudo systemctl daemon-reload
sudo systemctl start epsolarmqtt
sudo systemctl status epsolarmqtt
sudo systemctl enable epsolarmqtt

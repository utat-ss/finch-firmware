#!/bin/bash

STM_DEVICES=$(lsusb | grep -i "STMicroelectronis\|0483")

while read -r line; do
    BUS=$(echo "$line" | awk '{print $2}')
    DEV=$(echo "$line" | awk '{print $4}' | tr -d ':')
    DEVICE_PATH="/dev/bus/usb/${BUS}/${DEV}"

    echo "Giving permissions to $DEVICE_PATH"
    sudo chmod 666 "$DEVICE_PATH"
done <<< "$STM_DEVICES"

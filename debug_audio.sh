#!/bin/bash

# Audio Debug Script for Bengal Platform with AW87XXX PA
# This script helps diagnose audio issues with aw87xxx power amplifiers

echo "Bengal Audio Debug Script - AW87XXX Power Amplifier Troubleshooting"
echo "==================================================================="

# Check if AW87XXX devices are detected
echo "1. Checking I2C devices for AW87XXX..."
if [ -d /sys/bus/i2c/devices/1-005b ]; then
    echo "✓ AW87XXX PA0 (0x5B) detected"
else
    echo "✗ AW87XXX PA0 (0x5B) NOT detected"
fi

if [ -d /sys/bus/i2c/devices/1-0058 ]; then
    echo "✓ AW87XXX PA1 (0x58) detected"
else
    echo "✗ AW87XXX PA1 (0x58) NOT detected"
fi

# Check ALSA controls for AW87XXX
echo -e "\n2. Checking ALSA controls for AW87XXX..."
amixer controls | grep -i aw87
if [ $? -ne 0 ]; then
    echo "✗ No AW87XXX ALSA controls found"
else
    echo "✓ AW87XXX ALSA controls found"
fi

# Check DAPM widgets
echo -e "\n3. Checking DAPM widgets..."
if [ -f /sys/kernel/debug/asoc/bengal-idp-snd-card/dapm/bias_level ]; then
    echo "✓ Sound card debugfs found"
    ls /sys/kernel/debug/asoc/bengal-idp-snd-card/dapm/ | grep -i aw87
    if [ $? -ne 0 ]; then
        echo "✗ No AW87XXX DAPM widgets found"
    else
        echo "✓ AW87XXX DAPM widgets found"
    fi
else
    echo "✗ Sound card debugfs not accessible"
fi

# Check GPIO states
echo -e "\n4. Checking GPIO states for reset pins..."
echo "GPIO 35 (PA0 reset): $(cat /sys/kernel/debug/gpio | grep "gpio-35" || echo "not found")"
echo "GPIO 106 (PA1 reset): $(cat /sys/kernel/debug/gpio | grep "gpio-106" || echo "not found")"

# Check audio routing
echo -e "\n5. Checking audio routing..."
if [ -f /sys/kernel/debug/asoc/bengal-idp-snd-card/dapm/sink ]; then
    cat /sys/kernel/debug/asoc/bengal-idp-snd-card/dapm/sink | grep -i aux
    if [ $? -ne 0 ]; then
        echo "✗ No AUX routing found"
    else
        echo "✓ AUX routing configured"
    fi
fi

# Check dmesg for AW87XXX related messages
echo -e "\n6. Checking kernel messages for AW87XXX..."
dmesg | grep -i aw87 | tail -10

# List all ALSA mixer controls
echo -e "\n7. All ALSA mixer controls:"
amixer controls | head -20

# Check if CONFIG_SND_SOC_AW87XXX is enabled
echo -e "\n8. Checking kernel config..."
if [ -f /proc/config.gz ]; then
    zcat /proc/config.gz | grep CONFIG_SND_SOC_AW87XXX
elif [ -f /boot/config-$(uname -r) ]; then
    grep CONFIG_SND_SOC_AW87XXX /boot/config-$(uname -r)
else
    echo "Kernel config not found - check manually"
fi

echo -e "\n==================================================================="
echo "Debug script completed. If issues persist, check:"
echo "1. Device tree configuration is applied correctly"
echo "2. AW87XXX firmware/ACF files are present"
echo "3. CONFIG_SND_SOC_AW87XXX=y in kernel config"
echo "4. GPIO reset pins are properly configured"
echo "5. Audio routing connects AUX to AW87XXX PA0/PA1"
#!/bin/bash

if [[ ! -z $1 && $1 != "enable" && $1 != "disable" ]]; then
    echo "Invalid argument: $1" >&2
    echo ""
    echo "Usage: $(basename $0) [disable|enable]"
    exit 1
fi

# install cpupower and msr-tools
sudo apt-get install -yqq cpupower msr-tools;

# turbo
if [[ $1 == "disable" ]]; then
    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
    echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost
fi
if [[ $1 == "enable" ]]; then
    echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
    echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost
fi

# CPU frequency
sudo cpupower frequency-set --governor performance 

# Disable DVFS (Dynamic voltage and frequency scaling), which is used to adjust
# the voltage and frequency of a CPU to reduce power consumption
for line in $(find /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor); do
    echo "performance" | sudo tee $line
done

# # hyper threading
# cat /sys/devices/system/cpu/smt/control
# echo forceoff | sudo tee /sys/devices/system/cpu/smt/control

sudo modprobe msr;
echo "current status ";
sudo rdmsr -p 0 0x1a0 -f 38:38;
sudo rdmsr -p 1 0x1a0 -f 38:38;

cores=$(cat /proc/cpuinfo | grep processor | awk '{print $3}')
for core in $cores; do
    if [[ $1 == "disable" ]]; then
        sudo wrmsr -p${core} 0x1a0 0x4000850089
    fi
    if [[ $1 == "enable" ]]; then
        sudo wrmsr -p${core} 0x1a0 0x850089
    fi
    state=$(sudo rdmsr -p${core} 0x1a0 -f 38:38)
    if [[ $state -eq 1 ]]; then
        echo "core ${core}: disabled"
    else
        echo "core ${core}: enabled"
    fi
done

echo "current status "
sudo rdmsr -p 0 0x1a0 -f 38:38;
sudo rdmsr -p 1 0x1a0 -f 38:38;


# Bengal Audio Fix Summary: AW87XXX Top Speaker Issues

## Problem Description
The top speaker on the Bengal platform is not outputting sound despite having two aw87xxx power amplifiers (PA) configured at I2C addresses 0x5B and 0x58.

## Root Cause Analysis
1. **Missing DAPM Widgets**: No speaker DAPM widgets were defined for the aw87xxx amplifiers
2. **No Event Handling**: No power management events to control the amplifiers
3. **Incomplete Audio Routing**: Device tree routing pointed to disabled WSA881x instead of aw87xxx
4. **Missing Device Access Function**: No way to access aw87xxx devices by index from machine driver

## Changes Made

### 1. Bengal Machine Driver (`qcom/opensource/audio-kernel/asoc/bengal.c`)

#### Added DAPM Widgets
```c
#ifdef CONFIG_SND_SOC_AW87XXX
	SND_SOC_DAPM_SPK("AW87XXX PA0", msm_aw87xxx_event),
	SND_SOC_DAPM_SPK("AW87XXX PA1", msm_aw87xxx_event),
#endif
```

#### Added Event Handler Function
```c
static int msm_aw87xxx_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	// Power on/off amplifiers based on DAPM events
	// Uses "speaker" profile for power on, "off" profile for power off
}
```

#### Added DAPM Ignore Suspend
```c
#ifdef CONFIG_SND_SOC_AW87XXX
	snd_soc_dapm_ignore_suspend(dapm, "AW87XXX PA0");
	snd_soc_dapm_ignore_suspend(dapm, "AW87XXX PA1");
#endif
```

### 2. AW87XXX Driver (`qcom/opensource/audio-kernel/asoc/codecs/aw87xxx/aw87xxx.c`)

#### Added Device Access Function
```c
struct aw87xxx *aw87xxx_get_dev_by_index(int index)
{
	// Searches the global device list for a device with the specified index
	// Returns pointer to aw87xxx device or NULL if not found
}
EXPORT_SYMBOL(aw87xxx_get_dev_by_index);
```

### 3. Device Tree Overlay (`bengal-audio-overlay-corrected.dtsi`)

#### Updated Audio Routing
```dts
qcom,audio-routing =
	// ... existing routing ...
	"AW87XXX PA0", "AUX",
	"AW87XXX PA1", "AUX",
	// ... rest of routing ...
```

#### Removed WSA881x from Active Codecs
```dts
asoc-codec  = <&stub_codec>, <&bolero_cdc>, <&wcd937x_codec>;
asoc-codec-names = "msm-stub-codec.1", "bolero-codec", "wcd937x_codec";
```

## Required Configuration

### 1. Kernel Configuration
Ensure these config options are enabled:
```
CONFIG_SND_SOC_AW87XXX=y
```

### 2. AW87XXX Firmware
The aw87xxx driver requires ACF (Audio Configuration File) binary:
- File: `/vendor/firmware/aw87xxx_acf.bin`
- Contains speaker profiles and tuning parameters

### 3. Device Tree Verification
Verify in your final device tree:
```dts
aw87xxx_pa_5B@5B {
	compatible = "awinic,aw87xxx_pa";
	reg = <0x5b>;
	reset-gpio = <&tlmm 35 0>;
	dev_index = <0>;
	status = "okay";
};

aw87xxx_pa_58@58 {
	compatible = "awinic,aw87xxx_pa";
	reg = <0x58>;
	reset-gpio = <&tlmm 106 0>;
	dev_index = <1>;
	status = "okay";
};
```

## Testing and Debugging

### 1. Run Debug Script
```bash
chmod +x debug_audio.sh
./debug_audio.sh
```

### 2. Manual Testing Commands
```bash
# Check if devices are detected
ls /sys/bus/i2c/devices/1-005b  # PA0
ls /sys/bus/i2c/devices/1-0058  # PA1

# Check ALSA controls
amixer controls | grep aw87xxx

# Test audio routing
tinymix | grep -i aux
tinymix | grep -i aw87

# Check DAPM widgets
ls /sys/kernel/debug/asoc/bengal-idp-snd-card/dapm/ | grep -i aw87
```

### 3. Profile Control
The driver creates mixer controls for each amplifier:
```bash
# Set speaker profile (turns on amplifier)
amixer cset name='aw87xxx_profile_switch_0' 1  # PA0
amixer cset name='aw87xxx_profile_switch_1' 1  # PA1

# Set off profile (turns off amplifier)  
amixer cset name='aw87xxx_profile_switch_0' 0  # PA0
amixer cset name='aw87xxx_profile_switch_1' 0  # PA1
```

## Potential Issues and Solutions

### Issue 1: No Sound from Top Speaker
**Symptoms**: Audio plays from headphones/earpiece but not speaker
**Solution**: 
1. Verify AUX output is working: `speaker-test -c2 -twav`
2. Check if AW87XXX widgets are powered: Check DAPM debugfs
3. Manually set speaker profile: Use amixer commands above

### Issue 2: I2C Communication Failed
**Symptoms**: AW87XXX devices not detected on I2C bus
**Solution**:
1. Check GPIO reset pins are properly configured
2. Verify I2C bus voltage levels (should be 1.8V or 3.3V)
3. Check device tree GPIO assignments match hardware

### Issue 3: No ALSA Controls
**Symptoms**: `amixer controls | grep aw87` returns nothing
**Solution**:
1. Verify `CONFIG_SND_SOC_AW87XXX=y` in kernel config
2. Check dmesg for aw87xxx probe errors
3. Ensure device tree aw87xxx nodes have `status = "okay"`

### Issue 4: Profile Loading Failed
**Symptoms**: Controls exist but amplifier doesn't turn on
**Solution**:
1. Check if `/vendor/firmware/aw87xxx_acf.bin` exists
2. Verify firmware contains "speaker" and "off" profiles
3. Check dmesg for firmware loading errors

## Files Modified
1. `qcom/opensource/audio-kernel/asoc/bengal.c` - Machine driver
2. `qcom/opensource/audio-kernel/asoc/codecs/aw87xxx/aw87xxx.c` - AW87XXX driver
3. `bengal-audio-overlay-corrected.dtsi` - Device tree overlay (new file)
4. `debug_audio.sh` - Debug script (new file)

## Next Steps
1. Apply all code changes
2. Update device tree overlay
3. Rebuild kernel and device tree
4. Flash updated images
5. Run debug script to verify configuration
6. Test audio playback to speakers

The changes provide proper integration of the aw87xxx power amplifiers into the audio subsystem with automatic power management through DAPM events.
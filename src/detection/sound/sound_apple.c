#include "sound.h"
#include "util/apple/cf_helpers.h"

#include <CoreAudio/CoreAudio.h>
#include <AvailabilityMacros.h>

#ifndef MAC_OS_VERSION_12_0
#define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
#endif

const char* ffDetectSound(FF_MAYBE_UNUSED const FFinstance* instance, FFlist* devices /* List of FFSoundDevice */)
{
    AudioDeviceID mainDeviceId;
    UInt32 dataSize = sizeof(mainDeviceId);
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &(AudioObjectPropertyAddress){
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    }, 0, NULL, &dataSize, &mainDeviceId) != kAudioHardwareNoError)
        return "AudioObjectGetPropertyData(kAudioHardwarePropertyDefaultOutputDevice) failed";

    AudioObjectID deviceIds[32] = {};
    dataSize = sizeof(deviceIds);
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &(AudioObjectPropertyAddress){
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeOutput,
        kAudioObjectPropertyElementMain
    }, 0, NULL, &dataSize, &deviceIds) != kAudioHardwareNoError)
        return "AudioObjectGetPropertyData(kAudioHardwarePropertyDevices) failed";

    for(uint32_t index = 0, length = dataSize / sizeof(*deviceIds); index < length; ++index)
    {
        AudioDeviceID deviceId = deviceIds[index];

        uint32_t muted;
        dataSize = sizeof(muted);
        if(AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
            kAudioDevicePropertyMute,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMain
        }, 0, NULL, &dataSize, &muted) != kAudioHardwareNoError)
            continue;

        FFSoundDevice* device = (FFSoundDevice*) ffListAdd(devices);
        device->main = deviceId == mainDeviceId;
        device->active = false;
        device->volume = FF_SOUND_VOLUME_UNKNOWN;
        ffStrbufInitF(&device->identifier, "%u", (unsigned) deviceId);
        ffStrbufInit(&device->name);

        uint32_t active;
        dataSize = sizeof(active);
        if(AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
            kAudioDevicePropertyDeviceIsAlive,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMain
        }, 0, NULL, &dataSize, &active) == kAudioHardwareNoError)
            device->active = !!active;

        if (!muted)
        {
            float volume;
            dataSize = sizeof(volume);
            if(AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
                kAudioDevicePropertyVolumeScalar,
                kAudioObjectPropertyScopeOutput,
                kAudioObjectPropertyElementMain
            }, 0, NULL, &dataSize, &volume) == kAudioHardwareNoError)
                device->volume = (uint8_t) (volume * 100 + 0.5);
            else
            {
                // Try detecting volume from channels
                uint32_t channels[2];
                dataSize = sizeof(channels);
                if (AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
                    kAudioDevicePropertyPreferredChannelsForStereo,
                    kAudioObjectPropertyScopeOutput,
                    kAudioObjectPropertyElementMain
                }, 0, NULL, &dataSize, channels) == kAudioHardwareNoError)
                {
                    dataSize = sizeof(volume);
                    if (AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
                        kAudioDevicePropertyVolumeScalar,
                        kAudioObjectPropertyScopeOutput,
                        channels[0]
                    }, 0, NULL, &dataSize, &volume) == kAudioHardwareNoError)
                    {
                        float temp;
                        if (AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
                            kAudioDevicePropertyVolumeScalar,
                            kAudioObjectPropertyScopeOutput,
                            channels[1]
                        }, 0, NULL, &dataSize, &temp) == kAudioHardwareNoError)
                            device->volume = (uint8_t) ((volume + temp) / 2 * 100 + 0.5);
                    }
                }
            }
        }

        CFStringRef name;
        dataSize = sizeof(name);
        if(AudioObjectGetPropertyData(deviceId, &(AudioObjectPropertyAddress){
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeOutput,
            kAudioObjectPropertyElementMain
        }, 0, NULL, &dataSize, &name) == kAudioHardwareNoError)
        {
            ffCfStrGetString(name, &device->name);
            CFRelease(name);
        }
    }

    return NULL;
}
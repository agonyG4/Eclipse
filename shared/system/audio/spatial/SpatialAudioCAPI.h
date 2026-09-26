#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpatialAudioHandle SpatialAudioHandle;

typedef struct SpatialAudioState {
  uint8_t active_preset;
  uint8_t target_preset;
  uint8_t queued_preset;
  uint8_t transitioning;
  uint64_t generation;
  uint64_t progress_frames;
  uint64_t transition_frames;
} SpatialAudioState;

SpatialAudioHandle *astrea_spatial_audio_create_default(int32_t *error_code);
void astrea_spatial_audio_destroy(SpatialAudioHandle *handle);
int32_t astrea_spatial_audio_select(SpatialAudioHandle *handle,
                                    uint8_t preset_id,
                                    uint64_t *generation_out);
int32_t astrea_spatial_audio_read_state(SpatialAudioHandle *handle,
                                        SpatialAudioState *state_out);
int32_t astrea_spatial_audio_process_planar(SpatialAudioHandle *handle,
                                            const float *const *input_channels,
                                            float *const *output_channels,
                                            size_t frames);

#ifdef __cplusplus
}
#endif

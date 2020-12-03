#ifndef ReconstructionOptions_h
#define ReconstructionOptions_h

#ifdef __cplusplus
#pragma once
#endif

#define OPTION_USE_VELOCITY_BUFFER (1 << 0)
#define OPTION_ASSUME_OCCLUSION (1 << 1)
#define OPTION_DIFFERENTIAL_BLENDING (1 << 2)

#define OPTION_DEBUG_SHOW_SAMPLES (1 << 3)
#define OPTION_DEBUG_SHOW_EVEN_SAMPLES (1 << 4)
#define OPTION_DEBUG_SHOW_VELOCITY (1 << 5)
#define OPTION_DEBUG_SHOW_COLORS (1 << 6)

#endif

typedef float sample_t;
typedef unsigned short sample_rate_t;
typedef float threshold_t;

#define MAX_HEART_RATE 240
#define MAX_HEART_RATE_INTERVAL (60.0f/MAX_HEART_RATE)

sample_t detect_peaks(sample_t* samples, size_t samples_size, const sample_rate_t hz, const threshold_t threshold_percentage);


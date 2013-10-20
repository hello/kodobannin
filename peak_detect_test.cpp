#include <stdio.h>

#include <vector>

extern "C" {
#include "peak_detect.h"
}

int main(int argc, char* argv[])
{
    if(argc < 2) {
	return 1;
    }

    const sample_rate_t hz = atoi(argv[1]);
    const threshold_t threshold_percentage = argc == 3 ? atof(argv[2]) : 0.0f;

    std::vector<sample_t> values;
    
    char line[1024];
    while(fgets(line, 1024, stdin)) {
        float f = atof(line);
        values.push_back(f);
    }
    
    if(!feof(stdin)) {
        perror("error reading from stdin");
    }
    
    float bpm = detect_peaks(&values[0], values.size(), hz, threshold_percentage);
    printf("Detected heart rate: %0.1f\n", bpm);
    
    return 0;
}

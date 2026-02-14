//
// Created by Clas Zachrisson on 2025-12-20.
//

#include "pitchyin.h"
#include <cstdio>
#include <cmath>

/**
 * Modified version of
 * https://github.com/JorenSix/Pidato/
 */

/*
// From B to A#
float semitones[] = {
	30.87,
	32.70,
	34.65,
	36.71,
	38.89,
	41.20,
	43.65,
	46.25,
	49.00,
	51.91,
	55.00,
	58.27
};
*/

void Yin::configure(float yinSampleRate,int yinBufferSize){
	sampleRate = yinSampleRate;
	threshold = 0.1;
	probability = 0.0;
	const int halfFrameSize = yinBufferSize / 2;
	harmonics = 4;
	constexpr int minFreq = 40;
	const int maxFreq = minFreq*(2<<harmonics);
	maxTau = std::min(static_cast<int>(std::ceil(sampleRate / minFreq)), halfFrameSize);
	minTau = std::min(std::max(static_cast<int>(sampleRate / maxFreq), 2), halfFrameSize);
	yinSize = maxTau + 1;
	yin = static_cast<float*>(malloc(sizeof(float) * yinSize));
}

Yin::Yin(){
}

Yin::Yin(float sampleRate,int bufferSize){
	configure(sampleRate,bufferSize);
}

float Yin::getProbability(){
	return probability;
}

float Yin::getPitch(float* signal){
	// ESP_LOGI(YIN_TAG, "Starting Yin");
	yin[0] = 1;

	// Compute difference function
	// Compute a cumulative mean normalized difference function and
	// simultaneously find lowest minimum below absolute threshold or global minimum
	float sum = 0.;
	int   threshTau = -1;
	float threshMin = 100;
	int   globalTau = -1;
	float globalMin = 100;
	for (int tau=1; tau < maxTau; ++tau) {
		yin[tau] = 0.;
		for (int j=0; j < yinSize-1; ++j) {
			float delta = signal[j] - signal[j+tau];
			yin[tau] += delta * delta;
		}
		sum += yin[tau];
		yin[tau] = yin[tau] * tau / sum;
		// if (std::isnan(yin[tau])) {
		// 	yin[tau] = 1;
		// }
		// if (tau < minTau) continue;
		// if (yin[tau] < threshold)
		// {
		// 	if (yin[tau] < threshMin) threshMin = yin[tau];
		// 	else
		// 	{
		// 		threshTau = tau-1;
		// 		break;
		// 	}
		// }
		// if (yin[tau] < globalMin)
		// {
		// 	globalMin = yin[tau];
		// 	globalTau = tau;
		// }
	}
	float minScore = 100000;
	int tau_min = -1;
	const float weights[harmonics] = {1, 0.8, 0.75, 0.6};
	for (int tau0 = maxTau; tau0 > maxTau/2+1; tau0--) {
		float score = 0;
		for (int k = 0; k<harmonics; ++k) {
			int tau_k = tau0 >> k;
			// if (tau_k < minTau) break;
			score += weights[k] * yin[tau_k] + 0.01 * tau_k;
		}
		if (score < minScore)
		{
			minScore = score;
			tau_min = tau0;
		}
	}
	float pitch = sampleRate / tau_min;
	printf("\rFrequency: %f Hz (score: %f)       ", pitch, minScore);

	// int tauEstimate = threshTau == -1 ? globalTau : threshTau;
	// probability = 1 - yin[tauEstimate];
	// float pitch = sampleRate / tauEstimate;
	// if (probability < 0)
	// {
	// 	pitch = 0;
	// 	probability = 0;
	// }
	// printf("\rFrequency: %f Hz (probability: %f)                ", pitch, probability);
	return pitch;
}
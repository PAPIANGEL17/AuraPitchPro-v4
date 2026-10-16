#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

// ==============================================================================
// // AUDIO ENGINE - ZERO LATENCY TD-PSOLA + AI VOICE PROFILER & ANALOG WARMTH
// ==============================================================================
class NaturalPitchEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize)
    {
        (void)maxBlockSize;
        sr = (sampleRate > 8000.0) ? sampleRate : 44100.0;
        bufferSize = 8192;
        analysisWindow = 1024;

        inputRing.assign(bufferSize, 0.0f);
        lpRing.assign(bufferSize, 0.0f);
        formantResidual.assign(bufferSize, 0.0f);
        yinBuffer.assign(analysisWindow / 2, 0.0f);

        writePos = 0;
        hopCounter = 0;
        grainPhase = 0.0;

        grainDelayA = 1024.0;
        grainDelayB = 1024.0;
        detectedPeriodSamples = sr / 220.0;

        pitchRatio = 1.0f;
        pitchCenter = 62.0f;
        lastTargetMidi = 62.0f;
        lastDetectedMidi = 62.0f;
        lastTargetNoteMidi = 62.0f;
        PureScaleNoteMidi = 62.0f;
        centsDeviation = 0.0f;
        noteHoldCounter = 0;
        voicedActive = false;
        lpState = 0.0f;
        medianHistory.fill(62.0f);
        medianIdx = 0;
        tubeState = 0.0f;
    }

    bool isNoteInScale(int midiNote, int rootNote, int scaleType) const
    {
        if (scaleType == 0) return true;
        int rel = (midiNote - rootNote + 1200) % 12;
        const bool majorMask[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
        const bool minorMask[12] = {true, false, true, true, false, true, false, true, true, false, true, false};
        return (scaleType == 1) ? majorMask[rel] : minorMask[rel];
    }

    float findClosestScaleNote(float currentMidi, int rootNote, int scaleType) const
    {
        int base = static_cast<int>(std::round(currentMidi));
        if (isNoteInScale(base, rootNote, scaleType)) return static_cast<float>(base);

        float bestNote = static_cast<float>(base);
        float minDist = 999.0f;
        for (int candidate = base - 3; candidate <= base + 3; ++candidate)
        {
            if (isNoteInScale(candidate, rootNote, scaleType))
            {
                float d = std::abs(currentMidi - static_cast<float>(candidate));
                if (d < minDist)
                {
                    minDist = d;
                    bestNote = static_cast<float>(candidate);
                }
            }
        }
        return bestNote;
    }

    bool detectVoicedState(const float* rawFrame, int length, float& rmsOut)
    {
        float sumSq = 0.0f;
        int zeroCrossings = 0;
        for (int i = 1; i < length; ++i)
        {
            sumSq += rawFrame[i] * rawFrame[i];
            if ((rawFrame[i] >= 0.0f && rawFrame[i - 1] < 0.0f) ||
                (rawFrame[i] < 0.0f && rawFrame[i - 1] >= 0.0f))
            {
                zeroCrossings++;
            }
        }
        rmsOut = std::sqrt(sumSq / static_cast<float>(length));
        float zcr = static_cast<float>(zeroCrossings) / static_cast<float>(length);
        return (rmsOut > 0.0025f) && (zcr < 0.19f);
    }

    float detectPitchYIN(const float* lpFrame, int length, float& confidenceOut)
    {
        int halfLen = length / 2;
        std::fill(yinBuffer.begin(), yinBuffer.end(), 0.0f);

        for (int tau = 1; tau < halfLen; tau += 2)
        {
            float sum = 0.0f;
            for (int i = 0; i < halfLen; i += 2)
            {
                float delta = lpFrame[i] - lpFrame[i + tau];
                sum += delta * delta;
            }
            yinBuffer[tau] = sum * 2.0f;
        }

        yinBuffer[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfLen; ++tau)
        {
            runningSum += yinBuffer[tau];
            yinBuffer[tau] *= static_cast<float>(tau) / (runningSum + 1e-9f);
        }

        int minTau = std::max(2, static_cast<int>(sr / 950.0));
        int maxTau = std::min(halfLen - 2, static_cast<int>(sr / 65.0));

        int bestTau = -1;
        const float threshold = 0.14f;

        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (yinBuffer[tau] < threshold)
            {
                while (tau + 1 < maxTau && yinBuffer[tau + 1] < yinBuffer[tau])
                    tau++;
                bestTau = tau;
                break;
            }
        }

        if (bestTau == -1)
        {
            float minVal = 1.0f;
            for (int tau = minTau; tau < maxTau; ++tau)
            {
                if (yinBuffer[tau] < minVal)
                {
                    minVal = yinBuffer[tau];
                    bestTau = tau;
                }
            }
            if (minVal > 0.35f)
            {
                confidenceOut = 0.0f;
                return 0.0f;
            }
        }

        float s0 = yinBuffer[std::max(0, bestTau - 1)];
        float s1 = yinBuffer[bestTau];
        float s2 = yinBuffer[std::min(halfLen - 1, bestTau + 1)];
        float denom = 2.0f * (2.0f * s1 - s2 - s0);
        float betterTau = static_cast<float>(bestTau);
        if (std::abs(denom) > 1e-6f)
            betterTau += std::clamp((s2 - s0) / denom, -0.5f, 0.5f);

        confidenceOut = 1.0f - std::clamp(s1, 0.0f, 1.0f);
        return static_cast<float>(sr) / betterTau;
    }

    float pushAndGetMedianMidi(float newMidi)
    {
        medianHistory[medianIdx] = newMidi;
        medianIdx = (medianIdx + 1) % 5;
        std::array<float, 5> sorted = medianHistory;
        std::sort(sorted.begin(), sorted.end());
        return sorted[2];
    }

    void processBlock(float* channelData, int numSamples,
                      float driftCorrection, float centerSnap,
                      float vibratoPreserve, float transitionSmooth,
                      float formantShiftSemitones, float mixLevel,
                      float onsetShaping, float tubeWarmth,
                      int rootNote, int scaleType)
    {
        const double grainSize = 1024.0;
        const double baseDelay = 1024.0;
        float lpCutCoeff = static_cast<float>(std::exp(-2.0 * 3.1415926535 * 950.0 / sr));

        for (int i = 0; i < numSamples; ++i)
        {
            float inSample = channelData[i];
            inputRing[writePos] = inSample;

            lpState = (1.0f - lpCutCoeff) * inSample + lpCutCoeff * lpState;
            lpRing[writePos] = lpState;

            int prevPos = (writePos - 1 + bufferSize) % bufferSize;
            float lowBody = 0.78f * inSample + 0.22f * inputRing[prevPos];
            formantResidual[writePos] = inSample - lowBody;

            if (++hopCounter >= 64)
            {
                hopCounter = 0;
                std::vector<float> rawFrame(analysisWindow);
                std::vector<float> filteredFrame(analysisWindow);
                for (int k = 0; k < analysisWindow; ++k)
                {
                    int idx = (writePos - analysisWindow + k + bufferSize) % bufferSize;
                    rawFrame[k] = inputRing[idx];
                    filteredFrame[k] = lpRing[idx];
                }

                float rms = 0.0f;
                bool isVoiced = detectVoicedState(rawFrame.data(), analysisWindow, rms);
                float confidence = 0.0f;
                float detectedHz = isVoiced ? detectPitchYIN(filteredFrame.data(), analysisWindow, confidence) : 0.0f;

                if (isVoiced && detectedHz >= 65.0f && detectedHz <= 950.0f && confidence > 0.65f)
                {
                    detectedPeriodSamples = std::clamp(sr / static_cast<double>(detectedHz), 40.0, 800.0);

                    float rawMidi = 69.0f + 12.0f * std::log2(detectedHz / 440.0f);
                    float cleanMidi = pushAndGetMedianMidi(rawMidi);

                    if (voicedActive && std::abs(cleanMidi - lastDetectedMidi) > 9.5f)
                        cleanMidi = lastDetectedMidi;

                    if (!voicedActive)
                    {
                        pitchCenter = cleanMidi;
                        lastTargetMidi = findClosestScaleNote(cleanMidi, rootNote, scaleType);
                    }

                    voicedActive = true;

                    // AI Voice Profiler & Attack shaping
                    float dynamicOnsetBoost = 1.0f + (onsetShaping - 0.5f) * 0.4f;
                    float alphaDrift = (0.12f * dynamicOnsetBoost) * (1.0f - transitionSmooth * 0.5f);
                    pitchCenter = (1.0f - alphaDrift) * pitchCenter + alphaDrift * cleanMidi;

                    float naturalVibrato = std::clamp(cleanMidi - pitchCenter, -1.5f, 1.5f);
                    if (vibratoPreserve > 1.0f)
                    {
                        float boost = 1.0f + (vibratoPreserve - 1.0f) * 1.5f;
                        naturalVibrato *= boost;
                    }
                    else
                    {
                        naturalVibrato *= vibratoPreserve;
                    }

                    float candidateTarget = findClosestScaleNote(pitchCenter, rootNote, scaleType);
                    if (std::abs(candidateTarget - lastTargetMidi) > 0.4f)
                    {
                        int requiredHold = 1 + static_cast<int>(transitionSmooth * 2.0f);
                        if (++noteHoldCounter >= requiredHold)
                        {
                            lastTargetMidi = candidateTarget;
                            noteHoldCounter = 0;
                        }
                    }
                    else
                    {
                        noteHoldCounter = 0;
                    }

                    float snapStrength = std::clamp(centerSnap, 0.0f, 1.0f);
                    float driftStrength = std::clamp(driftCorrection, 0.0f, 1.0f);

                    float targetCenter = pitchCenter + (lastTargetMidi - pitchCenter) * snapStrength;
                    float targetInstant = cleanMidi + (lastTargetMidi - cleanMidi) * driftStrength;

                    float baseCorrectedMidi = targetCenter * 0.55f + targetInstant * 0.45f;
                    float finalMidi = baseCorrectedMidi + naturalVibrato;

                    float correctedHz = 440.0f * std::pow(2.0f, (finalMidi - 69.0f) / 12.0f);
                    float cleanDetectedHz = 440.0f * std::pow(2.0f, (cleanMidi - 69.0f) / 12.0f);

                    float rawRatio = std::clamp(correctedHz / cleanDetectedHz, 0.72f, 1.40f);

                    float glideCoeff = 0.55f + 0.35f * (1.0f - transitionSmooth);
                    pitchRatio = (1.0f - glideCoeff) * pitchRatio + glideCoeff * rawRatio;

                    lastDetectedMidi = cleanMidi;
                    lastTargetNoteMidi = finalMidi;
                    PureScaleNoteMidi = lastTargetMidi;
                    centsDeviation = (cleanMidi - lastTargetMidi) * 100.0f;
                }
                else
                {
                    voicedActive = false;
                    pitchRatio = 0.85f * pitchRatio + 0.15f * 1.0f;
                    centsDeviation *= 0.80f;
                }
            }

            double prevPhase = grainPhase;
            grainPhase += 1.0 / grainSize;
            if (grainPhase >= 1.0)
            {
                grainPhase -= 1.0;
                double numPeriods = std::round((baseDelay - grainDelayB) / detectedPeriodSamples);
                grainDelayA = grainDelayB + numPeriods * detectedPeriodSamples;
            }

            if (prevPhase < 0.5 && grainPhase >= 0.5)
            {
                double numPeriods = std::round((baseDelay - grainDelayA) / detectedPeriodSamples);
                grainDelayB = grainDelayA + numPeriods * detectedPeriodSamples;
            }

            double pitchStep = 1.0 - static_cast<double>(pitchRatio);
            grainDelayA = std::clamp(grainDelayA + pitchStep, 32.0, static_cast<double>(bufferSize - 128));
            grainDelayB = std::clamp(grainDelayB + pitchStep, 32.0, static_cast<double>(bufferSize - 128));

            double phaseB = std::fmod(grainPhase + 0.5, 1.0);
            float winA = 0.5f * (1.0f - std::cos(2.0f * 3.1415926535f * static_cast<float>(grainPhase)));
            float winB = 0.5f * (1.0f - std::cos(2.0f * 3.1415926535f * static_cast<float>(phaseB)));

            float pitchedVocal = winA * readHermite(inputRing, grainDelayA)
                               + winB * readHermite(inputRing, grainDelayB);

            float formantGain = std::pow(2.0f, -formantShiftSemitones / 18.0f);
            float delayedDry  = inputRing[(writePos - static_cast<int>(baseDelay) + bufferSize) % bufferSize];
            float delayedAir  = formantResidual[(writePos - static_cast<int>(baseDelay) + bufferSize) % bufferSize];

            float wetSample = delayedDry;
            if (voicedActive || std::abs(pitchRatio - 1.0f) > 0.002f)
            {
                float formantSculpt = (formantGain - 1.0f) * 0.35f;
                wetSample = pitchedVocal + delayedAir * formantSculpt;
            }

            // Analog Tube Warmth & Saturation stage
            if (tubeWarmth > 0.01f)
            {
                float drive = 1.0f + tubeWarmth * 3.0f;
                wetSample = std::tanh(wetSample * drive) / std::tanh(drive);
            }

            // Dry / Wet Mix Level
            float outSample = (1.0f - mixLevel) * delayedDry + mixLevel * wetSample;

            channelData[i] = outSample;
            writePos = (writePos + 1) % bufferSize;
        }
    }

    float getLastDetectedMidi() const { return lastDetectedMidi; }
    float getLastTargetMidi() const { return lastTargetNoteMidi; }
    float getPureScaleNoteMidi() const { return PureScaleNoteMidi; }
    float getCentsDeviation() const { return centsDeviation; }
    bool isCurrentlyVoiced() const { return voicedActive; }

private:
    float readHermite(const std::vector<float>& buf, double delaySamples) const
    {
        double readPos = static_cast<double>(writePos) - delaySamples;
        while (readPos < 0.0) readPos += static_cast<double>(bufferSize);

        int idx1 = static_cast<int>(readPos) % bufferSize;
        int idx0 = (idx1 - 1 + bufferSize) % bufferSize;
        int idx2 = (idx1 + 1) % bufferSize;
        int idx3 = (idx1 + 2) % bufferSize;

        float frac = static_cast<float>(readPos - std::floor(readPos));
        float y0 = buf[idx0], y1 = buf[idx1], y2 = buf[idx2], y3 = buf[idx3];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    double sr = 44100.0;
    int bufferSize = 8192;
    int analysisWindow = 1024;
    std::vector<float> inputRing, lpRing, formantResidual, yinBuffer;
    int writePos = 0;
    int hopCounter = 0;

    double grainPhase = 0.0;
    double grainDelayA = 1024.0;
    double grainDelayB = 1024.0;
    double detectedPeriodSamples = 200.0;

    float pitchRatio = 1.0f;
    float pitchCenter = 62.0f;
    float lastTargetMidi = 62.0f;
    float PureScaleNoteMidi = 62.0f;
    int noteHoldCounter = 0;

    bool voicedActive = false;
    float lastDetectedMidi = 62.0f;
    float lastTargetNoteMidi = 62.0f;
    float centsDeviation = 0.0f;
    float lpState = 0.0f;
    float tubeState = 0.0f;

    std::array<float, 5> medianHistory { 62.0f, 62.0f, 62.0f, 62.0f, 62.0f };
    int medianIdx = 0;
};

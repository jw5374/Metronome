#include <AudioSessionTypes.h>
#include <Mmdeviceapi.h>
#include <Windows.h>
#include <Audioclient.h>
#include <basetsd.h>
#include <combaseapi.h>
#include <cstdlib>
#include <cstring>
#include <minwindef.h>
#include <mmdeviceapi.h>
#include <mmeapi.h>
#include <mmreg.h>
#include <math.h>
#include <conio.h>
#include <comdef.h>
#include <iostream>

// REFERENCE_TIME units = 100ns
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000
#define TWO_PI (3.14159265359*2)
#define RELEASE_RESOURCE(res) if((res) != NULL) { res->Release(); res = NULL; }

using namespace std;

// https://learn.microsoft.com/en-us/windows/win32/coreaudio/mmdevice-api
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

// https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

float pianoKey(float n) {
	// find nth piano frequency tuned to A = 440hz
	float key = pow(2.0, ((n-49.0)/12.0));
	return key * 440.0;
}

void buildSineTone(WAVEFORMATEX wave_format, UINT32 bufferFrameCount, BYTE *audioBytes, float freq) {
	// memcpy seemed to not work, must cast output buffer to float
	float *outData = (float*) audioBytes;

	// (The size of an audio frame = nChannels * wBitsPerSample) DOES NOT MATTER
	// INT32 FrameSize_bytes = bufferFrameCount * (wave_format.nChannels * wave_format.wBitsPerSample / 8);
	const size_t num_samples = bufferFrameCount * wave_format.nChannels;

	float dt = 1.0 / (float)wave_format.nSamplesPerSec;

	float ptime = 0.0;
	unsigned short vol = 2000;
	const float radsPerSec = 2 * 3.1415926536 * freq / (float) wave_format.nSamplesPerSec;

	// SAMPLES MUST BE BETWEEN -1.0 AND 1.0
	for (int i = 0; i < num_samples; i+=wave_format.nChannels) {
		float t = (float)i * dt;
		float amplitude = sin(t*TWO_PI*freq);

		// write values to all channels
		for (int j = 0; j < wave_format.nChannels; j++) {
			outData[i + j] = amplitude;
		}
	}
	// cout << freq << ", " << num_samples << ", " << wave_format.wBitsPerSample << endl;
	// cout << wave_format.nBlockAlign << ", " << wave_format.nSamplesPerSec << ", " << bufferFrameCount << endl;

}

void playMetronome(float bpm, float pianoNote, short timeSig, IMMDevice *device) {
	// Setup with reference to: https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
	IAudioClient *audClient = NULL;
	IAudioRenderClient *audRenderClient = NULL;
	WAVEFORMATEX *mixFormat = NULL;
	REFERENCE_TIME hnsBufferDur = REFTIMES_PER_SEC;
	UINT32 bufferFrameCount;

	device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audClient);
	audClient->GetMixFormat(&mixFormat);

	audClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, hnsBufferDur, 0, mixFormat, NULL);
	audClient->GetBufferSize(&bufferFrameCount);
	audClient->GetService(IID_IAudioRenderClient, (void**)&audRenderClient);

	REFERENCE_TIME hnsActualDur = ((double)(REFTIMES_PER_SEC) * bufferFrameCount) / mixFormat->nSamplesPerSec;

	audClient->Start();

	BYTE *bufAudData;
	UINT32 numFramesPadding;
	UINT32 numFramesAvail;

	short beatCounter = timeSig - 1;

	float freq = pianoKey(pianoNote);
	float downBeat = pianoKey(pianoNote + 8);
	DWORD bpmCalc = (DWORD)(((float)hnsActualDur/REFTIMES_PER_MILLISEC) / (bpm/60.0));

	cout << "Main frequency: " << freq << ", Downbeat: " << downBeat << endl;
	while(!_kbhit()) {
		beatCounter++;
		cout << beatCounter << " | ";
		Sleep(bpmCalc);
		audClient->GetCurrentPadding(&numFramesPadding);
		numFramesAvail = bufferFrameCount - numFramesPadding;
		numFramesAvail = (UINT32)(numFramesAvail / 32);

		audRenderClient->GetBuffer(numFramesAvail, &bufAudData);
		if(beatCounter % timeSig == 0) {
			beatCounter = 0;
			buildSineTone(*mixFormat, numFramesAvail, bufAudData, downBeat);
			audRenderClient->ReleaseBuffer(numFramesAvail, 0);

			// ANSI escape sequence '\x1b[' = ESC, '#K' = clear line without changing cursor position
			// Possible number values: 0 = clear line from cursor to end, 1 = clear line from cursor to beginning, 2 = clear whole line
			cout << "\x1b[2K\r";
			continue;
		}
		buildSineTone(*mixFormat, numFramesAvail, bufAudData, freq);
		audRenderClient->ReleaseBuffer(numFramesAvail, 0);
	}

	Sleep((DWORD)(hnsActualDur/REFTIMES_PER_MILLISEC/2));
	audClient->Stop();

	CoTaskMemFree(mixFormat);
	RELEASE_RESOURCE(audClient);
	RELEASE_RESOURCE(audRenderClient);
}

// assumed argv order: bpm, timeSignature, note
int main(int argc, char *argv[]) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	HRESULT hr = NULL;
	IMMDeviceEnumerator *pEnumerator = NULL;
	UINT numDevs = NULL;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator,
		NULL,
		CLSCTX_ALL,
		IID_IMMDeviceEnumerator,
		(void**)&pEnumerator
	);

	IMMDevice *dev = NULL;
	pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &dev);

	short bpm = (argc > 1) ? atoi(argv[1]) : 120;
	short timeSignature = (argc > 2) ? atoi(argv[2]) : 4; // always over 4, for now
	float note = (argc > 3) ? atoi(argv[3]) : 49.0;

	cout << "BPM: " << bpm << ", Time Signature: " << timeSignature << "/4, Piano Note: " << note << endl;
	playMetronome(bpm, note, timeSignature, dev);

	CoUninitialize();
	RELEASE_RESOURCE(pEnumerator);
	RELEASE_RESOURCE(dev);
	return 0;
}

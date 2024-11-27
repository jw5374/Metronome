#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <conio.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

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

class GetOpts {
		unordered_map<string, char *> argMap;
		int argc;
		char *argv[];
	public:
		GetOpts() {}
		GetOpts(int argcount, char *args[]) {
			processArgv(argcount, args);
		}
		void processArgv(int argcount, char *args[]) {
			unordered_map<string, char*> argopts;
			if(argcount > 1 && (argcount - 1) % 2 != 0) {
				throw runtime_error("All arguments must be in the form of '-option value' pairing.");
			}
			for(short i = 1; i < argcount; i += 2) {
				argopts.insert({string(args[i]), args[i+1]});
			}
			argMap = argopts;
		}
		const char* getOpt(string key) {
			auto findKey = argMap.find(key.data());
			if(findKey == argMap.end()) {
				return NULL;
			}
			return findKey->second;
		}
		bool optsContains(string key) {
			if(argMap.find(key.data()) == argMap.end()) {
				return false;
			}
			return true;
		}
};


float pianoKey(float n) {
	// find nth piano frequency tuned to A = 440hz
	float key = pow(2.0, ((n-49.0)/12.0));
	return key * 440.0;
}

void buildSineTone(WAVEFORMATEX wave_format, UINT32 bufferFrameCount, BYTE *audioBytes, float freq, float vol) {
	// memcpy seemed to not work, must cast output buffer to float
	float *outData = (float*) audioBytes;

	// (The size of an audio frame = nChannels * wBitsPerSample) DOES NOT MATTER
	// INT32 FrameSize_bytes = bufferFrameCount * (wave_format.nChannels * wave_format.wBitsPerSample / 8);
	const size_t num_samples = bufferFrameCount * wave_format.nChannels;
	float dt = 1.0 / (float)wave_format.nSamplesPerSec;

	float t, amplitude;
	// SAMPLES MUST BE BETWEEN -1.0 AND 1.0
	for (int i = 0; i < num_samples; i+=wave_format.nChannels) {
		t = (float)i * dt;
		amplitude = sin(t*TWO_PI*freq) * vol;

		// write values to all channels
		for (int j = 0; j < wave_format.nChannels; j++) {
			outData[i + j] = amplitude;
		}
	}
}

void playMetronome(float bpm, float pianoNote, short timeSig, float volume, IMMDevice *device) {
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
			buildSineTone(*mixFormat, numFramesAvail, bufAudData, downBeat, volume);
			audRenderClient->ReleaseBuffer(numFramesAvail, 0);

			// ANSI escape sequence '\x1b[' = ESC, '#K' = clear line without changing cursor position
			// Possible number values: 0 = clear line from cursor to end, 1 = clear line from cursor to beginning, 2 = clear whole line
			cout << "\x1b[2K\r";
			continue;
		}
		buildSineTone(*mixFormat, numFramesAvail, bufAudData, freq, volume);
		audRenderClient->ReleaseBuffer(numFramesAvail, 0);
	}

	Sleep((DWORD)(hnsActualDur/REFTIMES_PER_MILLISEC/2));
	audClient->Stop();

	CoTaskMemFree(mixFormat);
	RELEASE_RESOURCE(audClient);
	RELEASE_RESOURCE(audRenderClient);
}

// Expected optional args:
// -b = bpm
// -t = time signature (assumed to be over 4)
// -n = note (1 - 88)
// -vo = volume (should be between 0.0 - 1.0)
int main(int argc, char *argv[]) {
	GetOpts argopts;
	try {
		argopts = GetOpts(argc, argv);
	} catch (runtime_error e) {
		cerr << e.what() << endl;
		exit(EXIT_FAILURE);
	}
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

	short bpm = argopts.optsContains("-b") ? atoi(argopts.getOpt("-b")) : 120;
	short timeSignature = argopts.optsContains("-t") ? atoi(argopts.getOpt("-t")) : 4; // always over 4, for now
	float note = argopts.optsContains("-n") ? atof(argopts.getOpt("-n")) : 49.0;
	float volume = argopts.optsContains("-vo") ? atof(argopts.getOpt("-vo")) : 0.75;
	cout << "BPM: " << bpm << ", Time Signature: " << timeSignature << "/4, Piano Note: " << note << ", Volume: " << (int)(volume * 100) << "%" << endl;
	playMetronome(bpm, note, timeSignature, volume, dev);


	CoUninitialize();
	RELEASE_RESOURCE(pEnumerator);
	RELEASE_RESOURCE(dev);
	return EXIT_SUCCESS;
}

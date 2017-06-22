#ifndef RECORDER_H
#define RECORDER_H
#include <Windows.h>
#include <mmsystem.h>
#include <stdio.h>
#pragma comment(lib, "winmm.lib")

const u_int SILENCE_THRESHOLD = 10000;
const u_int SILENCE_WINDOW_LIMIT = 10;

enum RecorderState { WAIT_FOR_CHIRP_START, WAIT_FOR_CHIRP_END, WAIT_FOR_VOICE_START, WAIT_FOR_VOICE_END, VOICE_DONE };

class CRecorder
{
public:
	CRecorder(void);
	~CRecorder(void);

	MMRESULT start(const char *dev, const char *file);
	void stop(void);

private:
	void initFormat(WORD nCh,DWORD nSampleRate,WORD BitsPerSample);
	UINT waveInGetDevByName(const char* dev);
	static void CALLBACK CRecorder::MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	void writeData(HWAVEIN hwavein, DWORD dwParam1);
	void writeWaveHeadInfo(void);
	void findVoiceBuffer(LPSTR& data, DWORD& len);

private:
	static const u_int BUFFER_LENGTH = 22050;
	FILE *m_fp, *m_fp2;
	HWAVEIN m_phwi;
	
	WAVEHDR m_pwh1;
	char m_buffer1[BUFFER_LENGTH];
	
	WAVEHDR m_pwh2;
	char m_buffer2[BUFFER_LENGTH];
	
	size_t m_fileLength, m_fileLength2;
	size_t m_dataLength, m_dataLength2;
	
	WAVEFORMATEX m_WaveFormat;
	bool m_init;

	RecorderState m_state;
private:
	CRecorder& operator=(const CRecorder& obj){}
	CRecorder(const CRecorder& obj){}
};
#endif // RECORDER_H

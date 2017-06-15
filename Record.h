#ifndef RECORDER_H
#define RECORDER_H
#include <Windows.h>
#include <mmsystem.h>
#include <stdio.h>
#pragma comment(lib, "winmm.lib")

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
	static DWORD CALLBACK CRecorder::MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	void writeData(HWAVEIN hwavein, DWORD dwParam1);
	void writeWaveHeadInfo(void);

private:
	static const u_int BUFFER_LENGTH = 10240;
	FILE *m_fp;
	HWAVEIN m_phwi;
	
	WAVEHDR m_pwh1;
	char m_buffer1[BUFFER_LENGTH];
	
	WAVEHDR m_pwh2;
	char m_buffer2[BUFFER_LENGTH];
	
	size_t m_fileLength;
	size_t m_dataLength;
	
	WAVEFORMATEX m_WaveFormat;

private:
	CRecorder& operator=(const CRecorder& obj){}
	CRecorder(const CRecorder& obj){}
};
#endif // RECORDER_H

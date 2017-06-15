#include "Recorder.h"

CRecorder::CRecorder(void) :
	m_fp(0), m_dataLength(0), m_fileLength(0), m_phwi(0)
{
	// Init WaveInFormat
	initFormat(1, 22050, 16);

	// Init WaveIn Buffer 1
	m_pwh1.lpData=m_buffer1;
	m_pwh1.dwBufferLength=BUFFER_LENGTH;
	m_pwh1.dwUser=1;
	m_pwh1.dwFlags=0;

	// Init WaveIn Buffer 2
	m_pwh2.lpData=m_buffer2;
	m_pwh2.dwBufferLength=10240;
	m_pwh2.dwUser=2;
	m_pwh2.dwFlags=0;
}

void CRecorder::initFormat(WORD nCh,DWORD nSampleRate,WORD BitsPerSample)
{
	m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	m_WaveFormat.nChannels = nCh;
	m_WaveFormat.nSamplesPerSec = nSampleRate;
	m_WaveFormat.nAvgBytesPerSec = nSampleRate * nCh * BitsPerSample/8;
	m_WaveFormat.nBlockAlign = m_WaveFormat.nChannels * BitsPerSample/8;
	m_WaveFormat.wBitsPerSample = BitsPerSample;
	m_WaveFormat.cbSize = 0;
}

UINT CRecorder::waveInGetDevByName(const char* dev)
{
	MMRESULT mmResult;
	WAVEINCAPS waveIncaps;

	for (u_int i = 0; i < waveInGetNumDevs(); i++) {
		mmResult = waveInGetDevCaps(i, &waveIncaps, sizeof(WAVEINCAPS));
		if (mmResult != MMSYSERR_NOERROR)
		{
			break;
		}

		// printf("%s\n", waveIncaps.szPname);

		if (strstr(waveIncaps.szPname, dev) != 0)
		{
			return i;
		}
	}

	return WAVE_MAPPER;
}

CRecorder::~CRecorder(void)
{
}

void CRecorder::writeData(HWAVEIN hwavein, DWORD dwParam1)
{
	fwrite(((LPWAVEHDR)dwParam1)->lpData, sizeof(BYTE), ((LPWAVEHDR)dwParam1)->dwBytesRecorded, m_fp);
	
	m_dataLength += ((LPWAVEHDR)dwParam1)->dwBytesRecorded;
	m_fileLength = m_dataLength + sizeof(WAVEFORMATEX) + 20;  // m_fileLength equals total file size - length of "RIFF----".

	waveInAddBuffer(hwavein, (LPWAVEHDR)dwParam1, sizeof (WAVEHDR)) ;
}

DWORD CALLBACK CRecorder::MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	CRecorder * instance = reinterpret_cast<CRecorder *>(dwInstance);

	switch(uMsg)
	{
	case WIM_OPEN:
		//printf("\nDevice Opened...\n");
		break;

	case WIM_DATA:
		instance->writeData(hwavein, dwParam1);
		break;

	case WIM_CLOSE:
		//printf("\nDevice Closed...\n");
		break;

	default:
		break;
	}
	return 0;
}

MMRESULT CRecorder::start(const char *dev, const char *file)
{
	MMRESULT mmResult = MMSYSERR_NOERROR;
	UINT deviceId = WAVE_MAPPER;
		
	deviceId = waveInGetDevByName(dev);

	mmResult = waveInOpen(&m_phwi, deviceId, &m_WaveFormat, (DWORD)(MicCallback), (DWORD_PTR) this, CALLBACK_FUNCTION);
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

	mmResult = waveInPrepareHeader(m_phwi, &m_pwh1, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}
	
	mmResult = waveInAddBuffer(m_phwi, &m_pwh1, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

	mmResult = waveInPrepareHeader(m_phwi, &m_pwh2, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

	mmResult = waveInAddBuffer(m_phwi, &m_pwh2, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

	mmResult = waveInStart(m_phwi);
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

	errno_t err = fopen_s(&m_fp, file, "wb");
	if (m_fp == NULL)
	{
		return err;
	}

	writeWaveHeadInfo();

	return mmResult;
}

void CRecorder::writeWaveHeadInfo(void)
{
	size_t fmt_chunk_size = sizeof(m_WaveFormat);

	fwrite("RIFF----WAVEfmt ", sizeof(char), 16, m_fp);   // 16
	fwrite(&(fmt_chunk_size), sizeof(DWORD), 1, m_fp);    // 16+4 = 20
	fwrite(&m_WaveFormat, fmt_chunk_size, 1, m_fp);       // format chunk
	fwrite("data----", sizeof(char), 8, m_fp);            // 8

	m_fileLength = 0;
	m_dataLength = 0;
}

void CRecorder::stop(void)
{
	waveInStop(m_phwi);
	waveInClose(m_phwi);

	fseek(m_fp, 4, SEEK_SET);
	fwrite(&m_fileLength, sizeof(DWORD), 1, m_fp);
	fseek(m_fp, sizeof(m_WaveFormat) + 24, SEEK_SET);
	fwrite(&m_dataLength, sizeof(DWORD), 1, m_fp);

	fclose(m_fp);
}

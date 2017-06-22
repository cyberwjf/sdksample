#include "Recorder.h"
#include <cmath>
#include <ctime>

CRecorder::CRecorder(void) :
	m_fp(0), m_fp2(0), m_dataLength(0), m_fileLength(0), m_phwi(0), m_dataLength2(0), m_fileLength2(0), m_init(true), m_state(WAIT_FOR_CHIRP_START)
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
	m_pwh2.dwBufferLength=BUFFER_LENGTH;
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
	if (m_init == false) {
		waveInClose(m_phwi);
	}
}

void CRecorder::findVoiceBuffer(LPSTR& data, DWORD& length)
{
	LPSTR voice_start = data;
	static u_int silence_window = 0;

	if (m_state < VOICE_DONE) 
	{
		srand(time(NULL));

		short * frame = (short *) data;       // frame point to current checking voice frame
		DWORD len = length;                   // len == Bytes remaining in the data to check
		while (len > 0) 
		{
			u_int frame_len = min(m_WaveFormat.nSamplesPerSec/10, len/2);

			u_int energy = 0;
			for (int i=0; i < 10; i++) {
				int frame_index = rand() % frame_len;
				energy += abs(frame[frame_index]);
			}

			switch (m_state)
			{
			case WAIT_FOR_CHIRP_START:
				if(energy > SILENCE_THRESHOLD) {
					printf("Chirp Started\n");
					m_state = WAIT_FOR_CHIRP_END;
				}
				break;
			case WAIT_FOR_CHIRP_END:
				if(energy < SILENCE_THRESHOLD) {
					printf("Chirp Ended\n");
					m_state = WAIT_FOR_VOICE_START;
				}
				break;
			case WAIT_FOR_VOICE_START:
				if(energy > SILENCE_THRESHOLD) {
					printf("Voice Started\n");
					if ((LPSTR)(frame - frame_len) > data) {
						data = (LPSTR)(frame - frame_len);
						length = len + (2 * frame_len);
					}
					m_state = WAIT_FOR_VOICE_END;
					goto end;
				}
				break;
			case WAIT_FOR_VOICE_END: 
				if(energy < SILENCE_THRESHOLD)
				{
					silence_window += 1;
					if(silence_window > SILENCE_WINDOW_LIMIT) {
						length -= len;
						length = max(0, length - SILENCE_WINDOW_LIMIT * frame_len);
						printf("Voice Ended\n");
						m_state = VOICE_DONE;
						silence_window = 0;
						goto end;
					} 
				} 
				else 
				{
					silence_window = 0;
				}
				break;
			}
			len -= 2 * frame_len;
			frame += frame_len;
		}
end:
		if (m_state <= WAIT_FOR_VOICE_START)
		{
			length = 0;
		}

	}
	else
	{
		length = 0;
	}
}

void CRecorder::writeData(HWAVEIN hwavein, DWORD dwParam1)
{
	LPSTR data = ((LPWAVEHDR)dwParam1)->lpData;
	DWORD len = ((LPWAVEHDR)dwParam1)->dwBytesRecorded;

	if (m_fp2 != NULL) {
		if (len > 0) {
			fwrite(data, sizeof(BYTE), len, m_fp2);
			m_dataLength2 += len;
			m_fileLength2 = m_dataLength2  + sizeof(WAVEFORMATEX) + 20;  // m_fileLength equals (total file size - length of "RIFF----").
		}
	}

	if (m_fp != NULL) {
		findVoiceBuffer(data, len);

		if (len > 0) {
			fwrite(data, sizeof(BYTE), len, m_fp);
			m_dataLength += len;
			m_fileLength = m_dataLength + sizeof(WAVEFORMATEX) + 20;  // m_fileLength equals (total file size - length of "RIFF----").
		}
	}

	if (m_fp != NULL || m_fp2 != NULL) {
		waveInAddBuffer(hwavein, (LPWAVEHDR)dwParam1, sizeof (WAVEHDR)) ;
	}
}

void CALLBACK CRecorder::MicCallback(HWAVEIN hwavein, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	CRecorder * instance = reinterpret_cast<CRecorder *>(dwInstance);

	switch(uMsg)
	{
	case WIM_OPEN:
		//printf("\nDevice Opened...\n");
		break;

	case WIM_DATA:
		//printf("writeData...\n");
		instance->writeData(hwavein, dwParam1);
		break;

	case WIM_CLOSE:
		//printf("\nDevice Closed...\n");
		break;

	default:
		break;
	}
}

MMRESULT CRecorder::start(const char *dev, const char *file)
{
#ifdef _DEBUG
	printf("CRecorder::start\n");
#endif
	MMRESULT mmResult = MMSYSERR_NOERROR;
	UINT deviceId = WAVE_MAPPER;

	if (m_init == true) {
		m_init = false;
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
	}

	m_fp = fopen(file, "wb");
	if (m_fp == NULL)
	{
		return -1;
	}

	mmResult = waveInStart(m_phwi);
	if (mmResult != MMSYSERR_NOERROR)
	{
		return mmResult;
	}

#ifdef _DEBUG
	/*
	m_fp2 = fopen("baseline.wav", "wb");
	if (m_fp2 == NULL)
	{
		return -1;
	}
	*/
#endif

	writeWaveHeadInfo();

	return mmResult;
}

void CRecorder::writeWaveHeadInfo(void)
{
	size_t fmt_chunk_size = sizeof(m_WaveFormat);

	if (m_fp != NULL) {
		fwrite("RIFF----WAVEfmt ", sizeof(char), 16, m_fp);   // 16
		fwrite(&(fmt_chunk_size), sizeof(DWORD), 1, m_fp);    // 16 + 4 = 20
		fwrite(&m_WaveFormat, fmt_chunk_size, 1, m_fp);       // format chunk
		fwrite("data----", sizeof(char), 8, m_fp);            // 8

		m_fileLength = 0;
		m_dataLength = 0;
	}

	if (m_fp2 != NULL) {
		fwrite("RIFF----WAVEfmt ", sizeof(char), 16, m_fp2);   // 16
		fwrite(&(fmt_chunk_size), sizeof(DWORD), 1, m_fp2);    // 16+4 = 20
		fwrite(&m_WaveFormat, fmt_chunk_size, 1, m_fp2);       // format chunk
		fwrite("data----", sizeof(char), 8, m_fp2);            // 8
		
		m_fileLength2 = 0;
		m_dataLength2 = 0;
	}
}

void CRecorder::stop(void)
{
#ifdef _DEBUG
	printf("CRecorder::stop\n");
#endif

	if (m_fp != NULL)
	{
		waveInStop(m_phwi);
		fseek(m_fp, 4, SEEK_SET);
		fwrite(&m_fileLength, sizeof(DWORD), 1, m_fp);
		fseek(m_fp, sizeof(m_WaveFormat) + 24, SEEK_SET);
		fwrite(&m_dataLength, sizeof(DWORD), 1, m_fp);
		fclose(m_fp);
		m_fp = NULL;
	}
	
	m_state = WAIT_FOR_CHIRP_START;

	if (m_fp2 != NULL)
	{
		waveInStop(m_phwi);
		fseek(m_fp2, 4, SEEK_SET);
		fwrite(&m_fileLength2, sizeof(DWORD), 1, m_fp2);
		fseek(m_fp2, sizeof(m_WaveFormat) + 24, SEEK_SET);
		fwrite(&m_dataLength2, sizeof(DWORD), 1, m_fp2);
		fclose(m_fp2);
		m_fp2 = NULL;
	}
}
